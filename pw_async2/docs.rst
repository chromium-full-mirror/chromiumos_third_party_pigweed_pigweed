.. _module-pw_async2:

=========
pw_async2
=========
.. pigweed-module::
   :name: pw_async2

``pw_async2`` is a cooperatively scheduled asynchronous framework for C++,
optimized for use in resource-constrained systems. It helps you write complex,
concurrent applications without the overhead of traditional preemptive
multithreading. The design prioritizes efficiency, minimal resource usage
(especially memory), and testability.

--------
Benefits
--------
- **Simple Ownership**: Say goodbye to that jumble of callbacks and shared
  state! Complex tasks with many concurrent elements can be expressed by
  simply combining smaller tasks.
- **Efficient**: No dynamic memory allocation required.
- **Pluggable**: Your existing event loop, work queue, or task scheduler
  can run the ``Dispatcher`` without any extra threads.

-------
Example
-------
:ref:`module-pw_async2-informed-poll` is the core design philosophy behind
``pw_async2``. :cc:`Task <pw::async2::Task>` is the main async primitive.
It's a cooperatively scheduled "thread" which yields to the
:cc:`Dispatcher <pw::async2::Dispatcher>` when waiting. When a ``Task``
is able to make progress, the ``Dispatcher`` runs it again:

.. literalinclude:: examples/basic_manual.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-manual]
   :end-before: [pw_async2-examples-basic-manual]

Tasks can then be run on a :cc:`Dispatcher <pw::async2::Dispatcher>`
using the :cc:`Post() <pw::async2::Dispatcher::Post>` method:

.. literalinclude:: examples/basic_manual.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-dispatcher]
   :end-before: [pw_async2-examples-basic-dispatcher]

----------
Learn more
----------
.. grid:: 2

   .. grid-item-card:: :octicon:`light-bulb` Informed poll
      :link: module-pw_async2-informed-poll
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      The core design philosophy behind ``pw_async2``. We strongly encourage
      all ``pw_async2`` users to internalize this concept before attempting to
      use ``pw_async2``!

   .. grid-item-card:: :octicon:`beaker` Codelab
      :link: module-pw_async2-codelab
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Get hands-on experience with the core concepts of ``pw_async2`` by
      building a simple, simulated vending machine.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Quickstart
      :link: module-pw_async2-quickstart
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to quickly integrate ``pw_async2`` into your project and start using
      basic features.

   .. grid-item-card:: :octicon:`rocket` Guides
      :link: module-pw_async2-guides
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to use dispatchers to coordinate tasks, pass data between tasks,
      and more.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: ../api/cc/group__pw__async2.html
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      C/C++ API reference for ``Task``, ``Dispatcher``, ``Coro``, and more.

   .. grid-item-card:: :octicon:`pencil` Code size analysis
      :link: module-pw_async2-size-reports
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Reports on the code size cost of adding ``pw_async2`` to a system.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Dispatchers
      :link: module-pw_async2-dispatcher
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      You can use a Pigweed-provided dispatcher or roll your own.

   .. grid-item-card:: :octicon:`container` Futures
      :link: module-pw_async2-futures
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Futures are the basic async primitive in ``pw_async2``. Learn about
      future ownership, lifetimes, polling, composability, and more.

.. grid:: 2

   .. grid-item-card:: :octicon:`inbox` Channels
      :link: module-pw_async2-channels
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Channels are the primary mechanism for inter-task communication in
      ``pw_async2``. Learn about channel creation, handles, sending and
      receiving, lifetimes, allocation, and more.

   .. grid-item-card:: :octicon:`pencil` Coroutines
      :link: module-pw_async2-coro
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to define tasks with coroutines, allocate memory, perform async
      operations from coroutines, and more.

.. toctree::
   :hidden:
   :maxdepth: 1

   informed_poll
   codelab/docs
   quickstart
   guides
   dispatcher
   code_size
   coroutines
   futures
   channels
   tasks
