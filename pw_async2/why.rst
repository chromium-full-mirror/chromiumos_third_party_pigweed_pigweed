.. _module-pw_async2-why:

==============
Why pw_async2?
==============
.. pigweed-module-subpage::
   :name: pw_async2

Embedded systems frequently need to manage multiple concurrent operations, such
as handling network traffic, processing sensor data, and controlling actuators.
Traditional asynchronous approaches like ad-hoc callbacks and multi-threading
suffice for simple systems but introduce risks around deadlocks, memory usage,
and complexity as the software grows.

Pigweed requires a concurrency model that scales to complex libraries like a
Bluetooth stack without sacrificing safety, ergonomics, or composition.
We adopted the :ref:`module-pw_async2-informed-poll` architecture to meet these
needs. This document explains why we built ``pw_async2`` and provides a guide to
help you evaluate if it is the right fit for your project.

-----------------------
Pigweed's async journey
-----------------------
Pigweed's support for asynchronous execution evolved over several years, driven
by the increasing complexity of embedded applications.

Our first async-adjacent module was :ref:`module-pw_work_queue`, a simple
utility for deferring work from interrupts to a dedicated thread. While it was
explicitly designed to not be a general-purpose async solution, its availability
led teams to begin using it for various types of async work, making it clear
that we needed to provide something more structured for complex, multi-step
operations.

To address this, we developed :ref:`module-pw_async` (v1). It introduced a
formal ``Dispatcher`` and ``Task`` model on top of a structured event loop, with
these interfaces being virtualized to allow for system-specific optimizations.
While this solved execution context issues and helped serialize work, each task
was still fundamentally a callback. As we built complex protocols like
Bluetooth, we found that chaining callbacks led to fragmented logic and complex
manual state tracking.

This experience led us to build ``pw_async2``, centered on the
:ref:`module-pw_async2-informed-poll` model. Inspired primarily by Rust's async
design, ``pw_async2`` provides a cooperatively scheduled event loop for running
persistent tasks which suspend execution while blocked to allow other tasks to
run.

An explicit goal of the Rust-aligned design is to allow for interoperability
between C++ and Rust tasks as Rust becomes more widely used in embedded systems.
Compared to the other async models discussed below, the self-contained and
composable nature of ``pw_async2`` tasks provides a convenient interface for
cross-language asynchronous operations. We already have early demos of Rust
futures running on the C++ dispatcher.

----------------------------------------------
The pitfalls of traditional concurrency models
----------------------------------------------
Let's consider a typical embedded transaction in a consumer electronics device.
We read multiple sensors, perform some logic on the result, then forward it to
a consumer. Specifically, the code needs to:

1. Sample two sensors in parallel, timing out the attempts after 10ms.
2. Send a packet containing the combined sensor data, with backpressure if the
   consumer cannot keep up.
3. Repeat this at a constant rate.
4. At any point, the device may signal that it is going to sleep. When this
   happens, all pending operations must be cancelled.

Note that this is more complex than a typical introductory example as it
involves interactions between different subsystems. The reason for this
complexity is that this is what real world products have to do. If this feels
more advanced than your use cases, you may not need ``pw_async2``.

Ad-hoc callbacks
================
In an ad-hoc callback model, asynchronous operations are triggered by hardware
interrupts, timers, or background threads, invoking a provided callback
function upon completion.

While this fire-and-forget style appears simple initially, it becomes difficult
to manage as complexity grows. The following example demonstrates the implementation
of the multi-sensor transaction described above using callbacks:

.. literalinclude:: examples/motivation.cc
   :language: cpp
   :start-after: [pw_async2-examples-motivation-callbacks]
   :end-before: [pw_async2-examples-motivation-callbacks]

This approach introduces several critical risks and complexities:

* **Manual state synchronization and race conditions.** Because callbacks can
  execute in arbitrary contexts (e.g., hardware interrupts or timer threads),
  all shared state must be protected by explicit synchronization primitives.
  Additionally, the varied execution contexts make it hard to reason about what
  operations are safe to perform in callbacks. In the example, the
  ``timeout_timer_`` callback and the sensor completion callbacks can execute
  concurrently, necessitating careful locking to avoid corrupting the shared
  ``State`` structure.

* **Lock inversion and deadlocks.** To prevent deadlocks, locks must be tightly
  scoped. Invoking external callbacks or cancelling timers while holding
  synchronization locks risks recursive deadlocks or lock inversion. The
  developer must manually ensure that locks are released before invoking
  operations like ``tx_.SendAsync`` or ``timeout_timer_.Cancel()``.
  Additionally, this complex locking comes with the cost of either keeping
  interrupts disabled (if using spinlocks), or blocking the calling thread when
  using mutexes, and mutexes on some common RTOSes are quite heavy in terms of
  CPU and memory.

* **Implicit coupling.** Callback-based APIs often force tight dependencies
  on the behavior of their backend (e.g. a driver). This ranges from the
  context in which the callback is run (ISR or thread?) to how multiple
  operations are queued, whether callbacks are permitted to call into the same
  APIs (risking recursive locking), whether cancellation is possible, and so on.
  These assumptions are not always spelled out in the API contract, but code
  often relies on them and can break if run against a different implementation.

* **Lifetime and cancellation fragility.** There is no standard way to cancel
  an ongoing asynchronous operation in this model. If the
  ``CallbackSensorProcessor`` is destroyed while reads are pending, their
  subsequent callbacks will access invalid memory. The destructor must assert
  that no cycles are active, placing the burden of lifetime management entirely
  on the user.

Structured work queues
======================
A common improvement over the ad-hoc callback approach is the use of a work
queue which serializes callback execution on a single thread. These are widely
and successfully used in many real products. Pigweed even ships its own
implementation in :ref:`module-pw_work_queue`.

By guaranteeing that callbacks execute in a known context serially, the complex
internal locking required for ad-hoc callbacks is eliminated. Compare the work
queue-based solution to the previous example:

.. literalinclude:: examples/motivation.cc
   :language: cpp
   :start-after: [pw_async2-examples-motivation-work-queue]
   :end-before: [pw_async2-examples-motivation-work-queue]

As you can see, the implementation becomes much more straightforward and
readable. The mess of granular locking is gone, as each of the sensor
processor's operations runs on the same work queue.

However, this does not solve the ergonomic problems of callbacks, nor does it
eliminate locking entirely. External signals (such as the system going to sleep)
still require synchronization across thread boundaries, the execution flow
remains fragmented and difficult to reason about or debug, and callbacks can
still access invalid state if the object is destroyed.

Another implication in embedded systems in particular is that you typically
don't have artibrary context captures for callbacks (à la ``std::function``),
which means that async APIs need to be aware of the existence of the work queue
to know how to schedule their callbacks. This was not a problem with the ad-hoc
callback model as it would execute the callback directly.

Work queues also introduce some latency over ad-hoc callbacks because the
callbacks are not executed immediately. Instead, they are added to a queue
which is often shared by many parts of the system. Operations which are
time-sensitive can execute less predictably.

Synchronous multi-threading
===========================
Another common approach is to use a dedicated thread for the transaction, using
blocking APIs for I/O operations. This model provides a linear, readable control
flow via standard synchronous code, relying on the OS to handle concurrency.

Threads are often the default for many systems and for good reason. They are
easy to write, easy to reason about, easy to debug, and universally supported.
Almost all systems will have multiple threads running.

However, the simplicity of threads comes at a cost in resource-constrained
systems. While having a set of core threads is typical, problems start to arise
when you start spinning up new threads for an increasing number of concurrent
operations.

The following example demonstrates the implementation using Pigweed's
:ref:`module-pw_thread` module with its generic thread creation API:

.. literalinclude:: examples/motivation.cc
   :language: cpp
   :start-after: [pw_async2-examples-motivation-threads]
   :end-before: [pw_async2-examples-motivation-threads]

This code is notably simpler than any other example in this document, but it
introduces some architectural trade-offs:

* **High overhead.** Each thread requires its own dedicated stack. In the
  example, we allocate a 2KB stack for the sensor processor. In a system with
  many concurrent operations, committing a full stack per task quickly exhausts
  the limited RAM of a microcontroller, even though the thread may spend most
  of its time blocked waiting for I/O. Additionally, having more threads often
  leads to more resource contention, and mutexes are computationally expensive.
  Critical sections often become performance bottlenecks in products.

* **Resource vs. latency tradeoff.** To keep resource usage low, we use a single
  thread that reads the sensors sequentially. If we wanted to read them in
  parallel to reduce latency, we would need to spawn additional threads, further
  compounding the RAM issue. Serializing the reads means the cycle time is the
  sum of both timeouts in the worst case.

* **Cancellation latency.** Blocking I/O calls are inherently difficult to
  cancel cooperatively. When the system requests to sleep by calling ``Stop()``,
  the worker thread may be blocked in ``ReadBlocking`` or ``SendBlocking``.
  The calling thread must wait for the blocking operation to complete or time
  out before ``join()`` returns, delaying the system's transition to low-power
  mode.

* **Manual synchronization is notoriously difficult.** Almost anyone who has
  worked on a multi-threaded system has spent countless hours chasing down
  deadlocks, livelocks, priority inversions, and other concurrency bugs.
  The more threads you introduce, the greater the risk of these problems.
  Other async models handle this complexity internally so you don't have to
  worry about it.

Events and message handling
===========================
To avoid the complex lifetime management and synchronization issues of
callbacks, some systems move to a higher level of abstraction using event and
message handlers (often called the Active Object pattern). In this model,
asynchronous operations post events to a centralized queue, and a state machine
processes them sequentially.

This eliminates the need for locks around shared state since all processing
happens in the same context. Event-based systems are easy to trace and easy to
test, and provides a unified interface across the platform, making it more
coherent.

The following example demonstrates an implementation of the sensor processor
as an event-driven state machine:

.. literalinclude:: examples/motivation.cc
   :language: cpp
   :start-after: [pw_async2-examples-motivation-event-handlers]
   :end-before: [pw_async2-examples-motivation-event-handlers]

Similar to work queues, the event model serializes processing and execution of
asynchronous tasks, while additionally adding type safety and structured
handling. However, it is not without its problems:

* **Event data bloat.** Many events carry data, such as sensor readings in our
  example. In a system where events are global, there needs to be a centralized
  definition of an event type that can accommodate all payloads. This results in
  either a large, complex type that is difficult to work with, or erasure tricks
  like ``void*`` which lose type safety.

* **Queue sizing and contention.** An event queue must either be conservatively
  sized to accommodate bursts of events, using extra memory, or the system must
  provide fallible or blocking APIs to the queue so that events can be dropped.
  The more widely used the event queue is, the worse this problem becomes.

* **State machine fragmentation.** A simple linear flow (read sensors, then
  transmit) is spread across multiple state handler functions. Reading the code
  requires jumping between states to understand the sequence of operations.
  Unlike with callbacks, the way in which the results of operations arrive via
  events are also fragmented, as the handler for an event is decoupled from the
  API that produced it. This makes it difficult to reason about how and when
  events will arrive, which can lead to incomplete or incorrect state machines.

* **Limited extensibility and composability.** When everything flows through a
  global dispatch table, that table needs to be aware of how to route to every
  possible async consumer, making it inherently coupled with the specifics of
  the system and difficult to extend or modularize. In particular, shared
  libraries are difficult to maintain as any modification to behavior requires
  coordination with each system's dispatch table.

  For example, say you wanted to add a delay in some shared module via a timer.
  Every user of that module would have to wire up the timer to route its
  completion event back into the library's event handler. In our experience with
  real products using this model, the high engineering cost of doing the right
  thing often results in shortcuts such as blocking briefly.

The informed poll approach (pw_async2)
======================================
The limitations of callbacks and threads led to the development of the
:ref:`informed poll <module-pw_async2-informed-poll>` architecture.

Informed polling separates *when* a task runs from *what* it does. Instead of
blocking a thread or pushing callbacks, asynchronous work is represented as
self-contained state machines that yield when they cannot make progress. A
central dispatcher polls these state machines only when they are ready to
advance.

The key to this model is the :cc:`pw::async2::Waker`. When a task yields while
waiting for an event, the event source acquires a ``Waker`` for that task. When
the event occurs, the source invokes the ``Waker`` to notify the dispatcher that
the task is ready to be polled again. This mechanism eliminates the need to poll
waiting tasks constantly and replaces callbacks with context-safe notifications.

The core poll model
-------------------
This foundational architecture allows tasks to achieve cooperative multitasking
and explicit composition:

.. literalinclude:: examples/motivation.cc
   :language: cpp
   :start-after: [pw_async2-examples-motivation-informed-poll]
   :end-before: [pw_async2-examples-motivation-informed-poll]

In this purely polled model:

* **Memory is bounded.** Tasks do not need dedicated thread stacks. They only
  store the state needed to make progress within the object itself, giving
  known sizes at compile-time.
* **State is explicit.** All variables that must survive across yield points
  (like futures and intermediate results) are stored as class members and
  accessed solely by the task. Ownership is never ambiguous.
* **Logic is contained.** The sequence of operations performed by a task is
  an implementation detail of that task instead of being scattered across a set
  of callbacks or handlers.
* **Strong composability.** Every async task and future is just an object with
  a ``Pend`` function that returns a ``Poll<T>`` indicating if it is complete.
  This allows complex operations to be built out of small parts, and makes it
  easier to extract and reuse code without any strong global coupling.
* **Everything lives on a single stack.** Similarly to a thread, a task's
  entrypoint is a regular function call. This allows for better use of
  traditional debug tooling compared to callback-based approaches.

However, manually writing these state machines introduces some boilerplate
and complexity:

* **Manual coordination.** Users must manually begin operations, poll for
  completion, and coordinate operations that need to run in parallel.
* **State proliferation.** While explicit state provides several benefits, it
  is also a double-edged sword. The number of possible states and class members
  required to track them grows with the complexity of the task's operations.
  In our example, we had to store several different futures and their results to
  persist them until other operations completed.
* **Developer unfamiliarity.** Compared to traditional async paradigms, which
  are well understood by most developers, the informed polling model has an
  initial learning curve, and manual state machines are not the easiest code to
  read.
* **Operation lifecycle management.** In the other async models described, when
  you call an async API, you know that it will automatically start and notify
  you when it is complete. ``pw_async2``'s tasks and futures are lazy, however,
  and must be polled to make progress. When writing manual polling code, it is
  easy to forget to poll some operations (e.g. short-circuiting a ``select`` if
  the first operation is pending won't advance the others).

Some of these issues are addressed by coroutines, described below, though they
introduce their own set of tradeoffs.

Coroutines (C++20)
------------------
To simplify writing async code, Pigweed provides adapters around its async2
polling model to work with C++20 coroutines. Coroutines allow you to write
sequential logic, using ``co_await`` to let the compiler synthesize the polled
state machine. Any async2 ``Future`` can directly be awaited from a coroutine.

By using high-level combinators like ``Join`` and ``Select``, we can express
complex race conditions and parallel operations in a single declarative
statement:

.. literalinclude:: examples/motivation.cc
   :language: cpp
   :start-after: [pw_async2-examples-motivation-coro]
   :end-before: [pw_async2-examples-motivation-coro]

Compared to manual polling, coroutines offer a significiant improvement in
ergonomics, while maintaining the same efficiency and safety guarantees:

* **Sequential code style.** Developers write code that reads synchronously but
  suspends execution cooperatively. There are no nested callbacks or complex
  state machines to maintain, no centralized event routers, and no concurrency
  issues.
* **Clear ownership.** A coroutine task inherently owns its futures. If the
  task is destroyed or the futures go out of scope, any pending asynchronous
  operations it was waiting on are automatically cancelled. No manual lifecycle
  management is required.

However, coroutines come with a notable tradeoff for embedded systems in terms
of memory.

* **Dynamic allocation.** All coroutine frames are allocated to persist state
  across suspension points, with sizes determined by the compiler. In our
  experience, compilers are still not great at optimizing this. Pigweed's
  coroutine wrappers use a :cc:`pw::Allocator`, so you at least have control
  over the pools of memory used.
* **High code size.** Using powerful combinators like ``Select`` and ``Join``
  generates complex template instantiations that can significantly increase
  binary size.
* **Loss of debug context.** Unlike the informed polling model they're built on,
  the compiler-generated coroutine state machines and execution context wraps
  coroutines in complex template code which debuggers don't always handle well.

In practice, many systems may prefer to mix coroutines with manual ``pw_async``
state machines to balance these constraints.

---------------------
Concurrency tradeoffs
---------------------
This comparison summarizes the high-level characteristics of each approach
across several axes:

* **Ergonomics:** How easy is the code to write and understand?
* **Memory usage:** How much memory (RAM and flash) does the approach require?
* **Debuggability:** How well does it work with standard debugging tools?
* **Extensibility:** How easy is it to add to or change the functionality of an
  existing system?
* **Portability:** How well can the code be shared between systems or projects?

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 15 15 15 25

   * - Model
     - Ergonomics
     - Memory
     - Debugging
     - Extensibility
     - Portability
     - Use Case
   * - Ad-hoc callbacks
     - 🔴 Poor
     - 🟢 Low
     - 🔴 Poor
     - 🟡 Fair
     - 🔴 Poor
     - Simple fire-and-forget IRQs
   * - Work queues
     - 🟡 Fair
     - 🟢 Low
     - 🟡 Fair
     - 🟡 Fair
     - 🟡 Fair
     - Serialized background tasks
   * - Blocking threads
     - 🟢 Excellent
     - 🔴 High
     - 🟢 Excellent
     - 🟢 Good
     - 🟢 Good
     - Resource-rich systems
   * - Event handlers
     - 🟡 Fair
     - 🟢 Low
     - 🟡 Fair
     - 🔴 Poor
     - 🔴 Poor
     - Centralized event routing
   * - Informed poll (core)
     - 🟡 Fair
     - 🟢 Low
     - 🟡 Fair
     - 🟢 Good
     - 🟢 Good
     - Zero-allocation state machines
   * - Coroutines (optional)
     - 🟢 Excellent
     - 🟡 Medium
     - 🔴 Poor
     - 🟢 Good
     - 🟢 Good
     - Ergonomic async for larger systems

Selecting the right tool
========================
Each of the concurrency models compared has its place in a system, and many real
products use multiple models depending on their needs. The list below serves as
a rough guide for when to use each one.

* **Ad-hoc callbacks:** Best for simple, fire-and-forget interrupt handlers and
  one-off operations that are not part of a larger sequence.
* **Structured work queues:** Good for serializing simple background tasks onto
  a single thread to avoid race conditions without complex locking, but break
  down as the number of steps in an operation grows.
* **Synchronous multi-threading:** Best for simple, linear control flows where
  debugging visibility is a priority, RAM is plentiful, and concurrency density
  is low. Limited primarily by system resources.
* **Event and message handlers:** Good for centralizing state tracking and
  simplifying lifetime management, but struggle with increased scope and scale.
* **Informed Poll (pw_async2):** Good for complex, high-density concurrency and
  strong composability, with minimal memory and resource use when writing tasks
  manually, at the cost of verbosity and an initial learning curve.

-------------------------------------
Evaluating pw_async2 for your project
-------------------------------------
While ``pw_async2`` was built to solve specific architectural bottlenecks in
Pigweed, projects should evaluate it based on the value it brings to their
application. If you have to support the following, ``pw_async2`` might be a
good fit:

* **Many asynchronous operations.** If you have a lot of operations running at
  once that spend much of their time blocked on external resources, multiplexing
  them on a single dispatcher thread can save a considerable amount of memory.
* **Complex multi-step operations.** When operations involve more than a few
  steps and require coordinating different resources, having them structured as
  self-contained tasks simplifies writing, testing, and debugging.
* **Composability.** If your system consists of different components, having
  common interfaces to async operations enables code sharing and improves
  maintainability.
* **Application-level visibility.** In many embedded deployments, the underlying
  RTOS scheduler is a black box. Because the ``pw_async2`` dispatcher lives in
  the application layer, you own the scheduler's behavior. You can instrument
  it, profile task execution times, and trace transitions using standard
  application logging.

``pw_async2`` is designed for incremental adoption. One you have a dispatcher
running, code can be migrated over to async tasks piece by piece, with
``pw_async2``-provided utilities such as channels to bridge the gap.

If you're interested in learning more about ``pw_async2``, check out our
:ref:`module-pw_async2-informed-poll` doc for a detailed overview of the model,
or jump into the :ref:`Codelab <module-pw_async2-codelab>` to get started with
some hands-on experience.

.. _Future: https://doc.rust-lang.org/std/future/trait.Future.html
