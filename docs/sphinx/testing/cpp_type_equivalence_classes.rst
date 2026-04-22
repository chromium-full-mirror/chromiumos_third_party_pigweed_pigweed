.. _docs-testing-cpp-type-equivalence-classes:

============================
C++ Type Equivalence Classes
============================

.. _docs-testing-cpp-type-equivalence-classes-introduction:

------------
Introduction
------------

Purpose
=======
Exhaustive testing of `generic <https://en.wikipedia.org/wiki/Generic_programming>`_
features is typically impossible, because users may instantiate
templates with their own types. This document describes a method for limiting
the test space by defining a set of equivalence classes of types. It also
provides recommendations for classes of types to consider when testing
different features.

Scope
=====
This document is limited to providing high level guidance on common classes of
types to consider when testing generic code within the Pigweed codebase. It may
have applicability outside of Pigweed or outside of testing generics (e.g. in
designing generics), however, that is not the primary goal of the document.

Additionally, it is not intended to be comprehensive of all possible types or
use cases but rather to provide the most value across common use cases.

.. _docs-testing-cpp-type-equivalence-classes-summary:

--------------------------
Summary of Recommendations
--------------------------

Recommendations by Use Case
===========================

Identify the primary use case of your feature and test it against the
following classes of types:

1. Logic & Arithmetic Features
------------------------------

* **Basic Scalars**: Test with ``bool``, signed/unsigned integers of varying
  widths (``int8_t``, ``uint16_t``), and floating-point types (``float``
  ``double``).
* **Special Scalars**: Test with enumerations (``enum``, ``enum class``),
  pointers (object, function, and member pointers), and units of measure (e.g.,
  ``std::chrono::milliseconds``).
* **Non-Scalars**: If applicable, test with vectors, matrices, and complex
  numbers.

See :ref:`Logic & Arithmetic <docs-testing-cpp-type-equivalence-classes-logic-and-arithmetic>`.

2. Storage & Organization (Containers & Allocators)
---------------------------------------------------

* **Construction Traits**: Test with types that are default constructible,
  non-default constructible (require parameters), and non-constructible (e.g.,
  singletons).
* **Destruction Traits**: Test with types that have non-default or
  non-accessible destructors.
* **Copy/Move Traits**: Test with trivially copyable types, non-trivially
  copyable types, move-only types (e.g., ``std::unique_ptr``), and non-copyable
  non-moveable types.
* **Search/Order Traits**: Test with types that provide (and do not provide)
  relational operators (``<``), equality operators (``==``), and hashable vs.
  non-hashable types.

See :ref:`Storage & Organization of Data <docs-testing-cpp-type-equivalence-classes-storage-and-organization>`.

3. Communication & Data Transmission
------------------------------------

* **Internal Communication**: Focus testing on the full spectrum of copy and
  move traits (trivial, non-trivial, move-only, non-copyable).
* **External Communication**: Test with explicitly Serializable types (like
  Protobufs) and Non-Serializable types to verify expected failures and/or
  correct handling.

See :ref:`Communication & Data Transmission <docs-testing-cpp-type-equivalence-classes-communication-and-data-transmission>`.

4. Resource Management
----------------------

* **Lifetime/Ownership**: Prioritize testing with move-only types
  (``std::unique_ptr``) and types with explicit ownership semantics.
* **Transaction Management**: Test with Transactable types (e.g., mutexes, file
  handles that start/stop stateful operations) and non-transactable types.

See :ref:`Resource Management <docs-testing-cpp-type-equivalence-classes-resource-management>`.

Tips for Testing Generics
=========================

* **Perform Negative Testing**: Do not just test success cases. Explicitly test
  that invalid types fail to compile or trigger static assertions as expected
  (use ``PW_NC_TEST``).
* **Watch for Combinatorial Explosion**: The number of test types multiplied by
  the number of test values can cause test sizes to explode. Be discerning and
  pick only one representative type from each relevant class.
* **Leverage Tooling**: Use Google Test's ``TYPED_TEST`` to reuse test logic
  across different types, but keep the test body simple to avoid complex
  branching in test logic.
* **Don't Forget Type Qualifiers**: Remember to consider ``const``/``volatile``
  qualifiers and l-value/r-value reference types as axes for your test matrices
  where relevant.
* **Test Edge Cases**: Consider testing with Callable types (lambdas/functors),
  Void, and Empty/Null types to ensure robust handling of edge cases.

---------
Rationale
---------
A common method of deriving test inputs is to define "classes" (or categories)
of inputs and then pick one or more interesting examples from each category as
representatives of the class. For example, let's consider how to test a
function with the signature:

.. code-block:: c++

   bool DoTheThing(std::int32_t value);

When selecting test inputs, out of all possible 32-bit integers, we might
define the classes: positive integers, zero, negative integers, and limit
values. Then from each of these classes we might choose a representative value:
``1``, ``0``, ``-1``, ``INT_MAX``, and ``INT_MIN``. This works under the
assumption that values within the same category are equivalent (i.e. the
integer ``1`` is the same as ``2`` is the same as ``3``, etc). In fact,
`ISO 26262 Part 6 <https://www.iso.org/standard/68388.html>`_ Section 9
(Software Unit Testing) uses the term “Generation and analysis of equivalence
classes” in its recommendations on unit testing. Additionally, James Grenning
advocates for a similar approach to unit testing using the
`ZOMBIES framework <https://blog.wingman-sw.com/tdd-guided-by-zombies>`_.

The point is not to be exhaustive of all possible inputs (as that is often
impossible), but to provide the best bang-for-buck coverage with a small set of
inputs. These inputs should then give us strong confidence that the Unit Under
Test (UUT) is robust to potential failure modes.

In the same way that we can define and select classes of input **values**, we
may also define and select classes of input **types** for testing generic code.
There is already some precedent for this idea of equivalence classes with C++'s
`Named Requirements <https://en.cppreference.com/w/cpp/iterator/concepts.html>`_
and `Concepts <https://en.cppreference.com/w/cpp/language/constraints.html>`_.

Let's consider an example again. Imagine we are attempting to test a generic
(template) function with the signature

.. code-block:: c++

   // Add two values of potentially different types and
   // return a type which can hold the larger of the two
   template <typename A, typename B, typename R = /*magic*/>
   R SafeAdd(const A& a, const B& b);

To test this function, we not only need to consider what input values are
interesting but also what input types. For instance, we might consider what
happens when two different size integers are used (e.g. ``std::int8_t`` and
``std::int16_t``) or we might want to test that it works with floating point
types as well as integers or mixing signed and unsigned types.

From this basic example, we can start to form equivalence classes: size/width,
integer vs floating point, signedness. Then in our testing, we can decide on
representative cases from each equivalence class, rather than attempting to
test every possible type combination.

+-------------------+-------------------+----------------------------+
| Type of A         | Type of B         | Test Case                  |
+===================+===================+============================+
| ``std::int16_t``  | ``std::int16_t``  | Base signed integer case   |
+-------------------+-------------------+----------------------------+
| ``std::uint16_t`` | ``std::uint16_t`` | Base unsigned integer case |
+-------------------+-------------------+----------------------------+
| ``float``         | ``float``         | Base float case            |
+-------------------+-------------------+----------------------------+
| ``std::int8_t``   | ``std::int16_t``  | Mixed width integer        |
+-------------------+-------------------+----------------------------+
| ``std::int16_t``  | ``std::uint16_t`` | Mixed sign integer         |
+-------------------+-------------------+----------------------------+
| ``float``         | ``double``        | Mixed width float          |
+-------------------+-------------------+----------------------------+
| ``std::int32_t``  | ``float``         | Mixed integer/float        |
+-------------------+-------------------+----------------------------+

The number of test cases can be as comprehensive or sparse as we want,
depending on the rigor and coverage we desire, but the purpose of the
equivalence classes is to give us a better indication of coverage and ensure
we're directing our efforts wisely.

The rest of this document lays out our methodology for defining such a set of
equivalence classes (specifically considering C++-based embedded systems), then
proposes a set of equivalence classes which can be used as a baseline for
testing generics in various common use cases across the Pigweed codebase.

A Note on Negative Tests
========================
It is important to note that some Equivalence Classes may be expected to fail
depending on the intended design and implementation of the Generic. **In this
document, we do not specify positive and negative Equivalence Classes since
that is situation dependent, but keep this in mind as it can be just as
important to test expected failures (and provide appropriate warnings/
assertions) as it is to test expected successes.**

Practicalities of Testing Generics
==================================
It is important to remember that generics are not code themselves. They are a
template which is only instantiated and turned into compilable code when a
valid type (or in some cases a value) is provided. The behavior of the
instantiated code will often change depending on the type provided, so for each
instantiation we still need to determine which behaviors and input values we
wish to test.

While methods for defining input values and test cases are out of scope for
this document, it is important to remember that the number of test types is
typically multiplicative with the number of input value test cases. That means
the number of total test cases tends to explode as we add more test types, so
we must be somewhat discerning in our selection of types.

Tools such as Google Test's ``TYPED_TEST`` can somewhat help with this by
commonizing test code, but this can also add more complexity to test logic due
to handling multiple different types. There is a balance to be struck when
commonizing templated test cases, and we leave that up to the reader to
determine the right balance.

.. note::
   ``pw_unit_test`` provides two backends, ``googletest`` and ``light``. Only ``googletest`` currently supports ``TYPED_TEST``.

.. _docs-testing-cpp-type-equivalence-classes-methodology:

-----------
Methodology
-----------

Definitions
===========

**Type**
   For our purposes, a Type is an abstract concept of the C++ language that can
   generally be broken down into two parts: Behavior (or interface) and Data
   Representation.

   *Behaviors* are the actions that can be taken on the object (its interface),
   including any side effects those actions might have either on the object
   itself or on the rest of the integrated hardware/software system.

   *Data Representation* is how the state of the object is actually stored in
   hardware.

   In the rest of this analysis we'll consider these two facets when grouping
   types into our equivalence classes.

   The C++ standard also defines many `categories of types
   <https://en.cppreference.com/w/cpp/language/type-id.html>`_, we will use
   this to guide our definitions but the standard is generally much more
   extensive than is required for our purpose.

**Object**
   An Object is just an instance of a Type and typically (though not always)
   has its own storage in memory.

**Input Type**
   The type(s) which are used to instantiate a Generic. This is the type we
   select from each of our Equivalence Classes. A.K.A. Template Parameter.

**Compound Type**
   According to the C++ standard, `Compound Types
   <https://en.cppreference.com/w/cpp/types/is_compound.html>`_ include
   everything else not a Fundamental Type, so reference types, pointer types,
   array types, function types (callables), enumeration types, and class types.

**Generic**
   `Generic Programming <https://en.wikipedia.org/wiki/Generic_programming>`_
   is a term of art within software engineering. In this document, we use the
   term *Generic* (along with the more C++-specific `Template
   <https://en.cppreference.com/w/cpp/language/templates.html>`_) to describe a
   type, function, etc which uses this programming method. Additionally, we use
   *Instantiation* or *Instantiated Type* to describe the type, function, etc
   after it has been instantiated with a `Template Parameter
   <https://en.cppreference.com/w/cpp/language/template_parameters.html>`_.

**Generic Under Test (GUT)**
   Similar to a "Unit Under Test (UUT)", this is the Generic type that we are
   attempting to test using Equivalence Classes.

Use Cases
=========
In order to define Equivalence Classes, we will first define how Generics are
used in typical embedded systems. Then, by understanding how they are used and
the characteristics and traits of the Template Parameters they will be acting
on, we can use those characteristics and traits to define our Equivalence
Classes.

Defining use cases not only provides a framework for generating Equivalence
Classes but also provides better guidance to the reader on when to consider
using each Equivalence Class. For this reason, in `Equivalence Classes
<#3-equivalence-classes>`_ we maintain the relationship between the classes and
the use cases for easier cross-referencing.

The most common use cases for Generics, which we will analyze below, are:

* **Logic & Arithmetic** - Common mathematical or logical operations on data,
  especially ones which are easy to get wrong.
* **Storage & Organization of Data** - Containers for organizing, storing, and
  managing relationships between data.
* **Communication & Data Transmission** - Type-aware code for communicating
  data, events, or state between different parts of the application or system.
* **Resource Management** - Reusable code for managing lifetimes, ownership,
  and access to resources.

.. note::

   Metaprogramming is another common use case for Generics, but we intentionally
   omit that from this document as it is really its own complex topic.

.. _docs-testing-cpp-type-equivalence-classes-analysis:

--------
Analysis
--------

.. _docs-testing-cpp-type-equivalence-classes-logic-and-arithmetic:

Logic & Arithmetic
==================
Seemingly the most basic usage of Generics is to reuse logic and arithmetic
functions across multiple fundamental types. While this seems simple, it is
often the most fraught because of the complex and often unexpected interactions
between different fundamental types (especially since most logic and arithmetic
functions take two or more operands of potentially differing types).

For a concrete example, consider our ``SafeAdd(...)`` function above. What
should happen when we attempt to add a signed integer and an unsigned integer?
Should it fail to compile, convert to a signed integer that can represent the
width of the unsigned integer, dynamically check the resultant value?

Ensuring correct, well-defined implementations (and designs) in such complex
scenarios is one of the central goals of this document. To provide a set of
Equivalence Classes that provides assurance of good coverage over the space, we
use a hierarchical approach to break down sets of “mathable” types into groups
based on their characteristics.

The two main categories of mathematics that we consider are Logic and
Arithmetic. We define Logic as any operation that results in a true or false
answer (e.g. comparing two numbers for equality), and Arithmetic as all other
mathematical operations. In most cases, there is no significant difference
between the Equivalence Classes for these two so we do not analyze them
separately, however, where there is a difference we will identify it explicitly.

Where we do see significant differences is between Scalar types and Non-Scalar
types, so our analysis will approach these two separately.

Scalars
-------
Scalars are the representation of a number on a number line. They are by far
the most common thing we do math on in embedded systems. It includes types like
integers, floats, and pointers, which allow basic operations like addition,
subtraction, multiplication, and division. These types may have certain
characteristics which limit their ability to represent certain values, such as
width or signedness.

.. note::

   The C++ language has a more strict definition of `scalar
   <https://en.cppreference.com/w/cpp/types/is_scalar.html>`_ than what we use
   here. It is essentially a subset of our definition, with the main differences
   being our inclusion of BigInt and Unit of Measure. We consider Scalar to mean
   anything that “looks and acts like a simple number”.

When considering Equivalence Classes for the set of Scalar types, we first
group the types by what sorts of numbers the type stores then further by common
restrictions or limitations of the types within that grouping. This leads to a
hierarchical set of Equivalence Classes which can be applied at either level of
granularity.

Using the above method, we define the following list of Equivalence Classes:

* Boolean - A type that may only take on the values ``true`` (``1``) or
  ``false`` (``0``). In C++, this generally means ``bool``.
* Integer - A type which may represent only whole integer values.

  * Width - The maximum number of bits that may be used to represent a number
    (typically 8, 16, 32, or 64).
  * Signedness - Whether or not the type can represent negative values or only
    positive.
  * BigInt - A type (user defined) which allows the width to grow dynamically,
    representing any size integer. This is often a very niche case.

* Enumeration - A type which is intended to represent a limited number of named
  values. Because math on enumerations is treated as integer math, an object of
  this type may not provide any guarantees of staying within the defined set of
  named values. For this reason, specifying and testing interfaces which support
  enumerations is particularly important.
* Fixed Point - A type which may represent fractional numbers but only up to a
  set precision (scaling factor).

  * Width - The maximum number of bits that may be used to represent a number
    (typically 8, 16, 32, or 64).
  * Signedness - Whether or not the type can represent negative values or only
    positive.
  * Precision - The smallest fractional part that can be represented (i.e.
    scaling factor).

* Floating Point - A type which may represent fractional numbers and which
  allows the radix point to “float”.

  * Width - The maximum number of bits that may be used to represent a number
    (typically 32 or 64).

* Pointer - A special type which represents a memory address and a “pointed to”
  type.

  * Object - Whether the pointer points to an object.
  * Function - Whether the pointer points to a function (and is callable).
  * Member - Whether the pointer points to a member of a class and retains that
    class member information in the type (i.e. ``U::*``).

* Unit of Measure - A type which represents a physical measurement or value.

  * Scale - The order of magnitude associated with the value, e.g.
    \<milli\>meter or \<nano\>second.
  * Conversion - The ability to convert a unit of measurement into another
    equivalent unit (e.g. meters to feet). Mainly useful when mixing two
    operands of different types.

Non-Scalars
-----------
Non-Scalars are more complex, multi-dimensional constructions often made up of
Scalars (e.g. vectors and matrices). Some mathematical operations allow mixing
scalar and non-scalar operands, e.g. multiplication, and some have equivalent
non-scalar versions, e.g. addition. However, some operations only apply to
non-scalars, such as the cross-product of vectors.

In general, handling such specific mathematical operations is done in
specialized libraries, so we don't go into much detail in this document. The
main relevant point is to consider whether a Pigweed interface should allow
non-scalar values or not.

The Equivalence Classes defined for non-scalars are:

* Vector - An N-dimensional vector may represent a point in N-dimensional space
  or a magnitude and direction.

  * Dimension - The number of values in the vector.

* `Complex Number <https://en.wikipedia.org/wiki/Complex_number>`_ - Typically,
  a special case of a vector. It represents numbers that contain both a real and
  an imaginary part.
* Matrix - A 2-dimensional array of scalar numbers, often used in linear
  algebra, physics, and machine learning.

  * Dimension - The number of rows and columns in the matrix.

* `Tensor <https://en.wikipedia.org/wiki/Tensor>`_ - An N-dimensional array of
  scalar numbers, often used in physics and machine learning.

  * Rank - The number of dimensions in the tensor, e.g. Rank 2 is equivalent to
    a 2D matrix and Rank 3 is equivalent to a 3D volume.
  * Dimension - The number of values in each dimension of the tensor.

.. note::

   While technically a Vector is just a Rank 1 Tensor and a Matrix is a Rank 2
   Tensor, libraries which handle Vectors and Matrices may not always generalize
   these types to N-dimensional Tensors. For that reason, we consider them as
   separate Equivalence Classes.

.. _docs-testing-cpp-type-equivalence-classes-storage-and-organization:

Storage & Organization of Data
==============================
When discussing Generics which support storage and organization of data, the
main two categories we will consider are Containers (similar to the `C++ STL
container library <https://en.cppreference.com/w/cpp/container.html>`_) and
Type-Aware Allocators. Both must manage a store of objects for a user, but an
Allocator is generally only concerned with creation and deletion of a type,
whereas Containers often care about moving, copying, and comparing objects.

Containers
----------
Type-Aware Containers are data structures which store a particular data type
and which may enforce relationships between each entry or make certain access,
ordering, or storage guarantees. These characteristics are different for each
container, for example, a vector ensures contiguous storage whereas a linked
list ensures O(1) insertion and deletion.

The main categories we consider for Input Types into Generic Containers are:

* Creation / Deletion - Ability to in-place construct an object inside the
  container storage and destruct the objects at the end of life of the container.
* Copying / Moving - Ability to copy or move an object into the container
  storage or move it around within the storage as the container updates.
* Comparison - Ability to compare objects against each other in meaningful ways
  to provide guarantees of ordering or access speed.

.. _docs-testing-cpp-type-equivalence-classes-creation-and-deletion:

Creation & Deletion
^^^^^^^^^^^^^^^^^^^
Creation and deletion of an object is defined by the ability to construct and
destruct the object. We define the following Equivalence Classes which capture
various common construction and destruction traits of Input Types:

* Default Constructible
* Non-Default Constructible
* Non-Constructible
* Non-Default Destructible
* Non-Destructible

.. _docs-testing-cpp-type-equivalence-classes-copy-and-moving:

Copying & Moving
^^^^^^^^^^^^^^^^
In order to move data into, out of, and within the container storage, we
consider the standard data copy / movement traits within the C++ language. The
language defines the following copy and move traits for all types:

* Copy constructible
* Copy assignable
* Move constructible
* Move assignable

If we assume each of these traits can take on one of three states: trivial,
non-trivial, or ``delete``'d, then limit the combinations using the `Rule of
Three/Five/Zero <https://en.cppreference.com/w/cpp/language/rule_of_three
html>`_ (i.e. non-trivial types will either be copyable and moveable or just
moveable), we get the following Equivalence Classes:

+-----------------------------------+------------------+-----------------+------------------+-----------------+
| Equivalence Class                 | Copy Constructor | Copy Assignment | Move Constructor | Move Assignment |
+===================================+==================+=================+==================+=================+
| Trivially Copyable / Moveable     | Trivial          | Trivial         | Trivial          | Trivial         |
+-----------------------------------+------------------+-----------------+------------------+-----------------+
| Non-Trivially Copyable / Moveable | Non-Trivial      | Non-Trivial     | Non-Trivial      | Non-Trivial     |
+-----------------------------------+------------------+-----------------+------------------+-----------------+
| Non-Trivially Moveable            | ``delete``       | ``delete``      | Non-Trivial      | Non-Trivial     |
+-----------------------------------+------------------+-----------------+------------------+-----------------+
| Non-Copyable / Non-Moveable       | ``delete``       | ``delete``      | ``delete``       | ``delete``      |
+-----------------------------------+------------------+-----------------+------------------+-----------------+

.. note::

   Technically, a type could also hide copy and move APIs as private or
   protected functions, but we will consider that the same as ``delete``'d for
   this discussion.

Comparison
^^^^^^^^^^
There are two main reasons for comparing objects in containers:

* Ordering - Determining placement of the object in the container in reference
  to other objects already in the container. Generally requires Input Types to
  provide `relational operators
  <https://en.cppreference.com/w/cpp/language/operator_comparison.html>`_.
* Searching - Finding a particular object within the container. Generally
  requires Input Types to provide `equality operators
  <https://en.cppreference.com/w/cpp/language/operator_comparison.html>`_ (or the
  user to provide a predicate function).

Ordering is often done by establishing a greater-than, less-than, or equal-to
relationship between two objects. It can either be done on the objects
themselves or on a set of unique keys (potentially using hashing). The specific
method of ordering will be determined by the container implementation, so
details such as `weak vs strong ordering
<https://en.cppreference.com/w/cpp/header/compare.html>`_ are left up to the
reader.

Searching will often require comparison of two objects (or two unique keys) for
equality. Again, the specific method is determined by the container
implementation.

The following Equivalence Classes cover these comparison requirements (it is up
to the user to determine if these classes should apply to the unique key or the
value object in cases of Key-Value containers):

* Relational Operators Provided
* No Relational Operators Provided
* Equality Operators Provided
* No Equality Operators Provided
* Hashable
* Non-Hashable

Type-Aware Allocators
---------------------
Similar to a Container which only stores an object without any additional
guarantees, a Type-Aware Allocator will generally only consider the creation/
destruction of an object - typically by providing an in-place construction
mechanism. For that reason, we consider only the following Equivalence Classes
copied directly from the previous `Creation & Deletion
<_docs-testing-cpp-type-equivalence-classes-creation-and-deletion>`_ discussion:

* Default Constructible
* Non-Default Constructible
* Non-Constructible
* Non-Default Destructible
* Non-Destructible

.. _docs-testing-cpp-type-equivalence-classes-communication-and-data-transmission:

Communication & Data Transmission
=================================
When transmitting data, the key concern is consistency between sender and
receiver(s). In other words, is the data semantically equal for both sender and
receiver. We use “semantic equality” here because bitwise equality does not
always ensure true equivalence. For example, passing a pointer to data as part
of an RPC message or passing data between a little endian and big endian system.

In order to ensure consistency when transmitting *within* a software system, a
type must at minimum consider copy and move semantics. When transmitting
between different hardware systems it may also need to consider serialization
(conversion to a well-defined, portable byte stream).

Copyability & Moveability
-------------------------
In this analysis we assume the Input Type's design will determine whether it
can be copied/moved and still remain consistent. If the type itself does not
enforce this, we consider it a bug in the Input Type, which cannot, in general,
be caught by testing the Generic.

In C++, enforcement of consistency across copy/move is done by defining (or
relying on the compiler defined) copy/move constructors and copy/move
assignment operators. See `Copying & Moving
<_docs-testing-cpp-type-equivalence-classes-copy-and-moving>`_ in the
Containers section above for a discussion of copy and move semantics. From
there we define the following Equivalence Classes:

* Trivially Copyable / Moveable
* Non-Trivially Copyable / Moveable
* Non-Trivially Moveable
* Non-Copyable / Non-Moveable

Serializability
---------------
One additional trait which we must consider when transmitting data is
serialization. Especially when transmitting data between distinct hardware
systems, those systems may have different underlying representations for the
same data types. This means some transmission protocols rely on serializing
data before sending and deserializing upon receiving data.

.. note::

   Note that we intentionally separate Trivially Copyable and Serializable.
   While trivially copying a struct, for example, is a form of serialization,
   that is not what we're considering here.

While Serializability is a type trait that we regularly encounter (e.g.
Protobufs), it is often only used in specific cases and is not broadly
applicable to Generics outside that use case. For these reasons, we include it
as an Equivalence Class, but recommend considering whether it applies to your
specific case.

.. _docs-testing-cpp-type-equivalence-classes-resource-management:

Resource Management
===================
Resource management is ensuring correct ownership and usage of a given resource
throughout its lifecycle, either directly or indirectly, usually via `RAII
<https://en.cppreference.com/w/cpp/language/raii.html>`_ or similar mechanisms.
The lifecycle of a resource includes its creation/allocation, access, and
destruction/release. Some examples of possible resources are: file handles,
peripheral devices, dynamically allocated memory, shared memory, mutexes, etc.

For the purposes of defining Equivalence Classes, we will consider two cases:

1. Directly managing a resource (e.g. ``std::unique_ptr<T>``) and
2. Operating on a resource managing type (e.g.
   ``pw::Vector<std::unique_ptr<T>,10>``).


Direct Management of a Resource
-------------------------------
In the case of directly managing a resource, the Input Type into the Generic
Under Test is actually the resource to be managed. Some common use cases for a
resource managing Generic are:

* Managing the lifetime of the resource (creation and deletion),
* Enforcing ownership (limiting or tracking copies), and/or
* Automatically operating on the resource to provide some extra guarantees
  (e.g. locking/unlocking, flushing buffers, committing transactions, joining a
  thread)

We cannot exhaustively define all type traits for these use cases since many of
them are niche or implementation specific, but we will discuss some common
traits that can be used as a starting point for defining Equivalence Classes.

Lifetime Management
^^^^^^^^^^^^^^^^^^^
Managing the lifetime of a resource involves ensuring proper construction and
destruction of the resource. That could mean facilitating allocation, in-place
construction, destruction, and/or deallocation of the resource. To ensure
sufficient coverage of these areas, we consider these Equivalence Classes:

* Default Constructible
* Non-Default Constructible
* Non-Constructible
* Destructible
* Non-Destructible

.. note::

   We intentionally omit throwing constructors and destructors as exceptions are
   not used in Pigweed, however, if you think a user of your Generic might use
   exceptions in constructors and destructors, consider whether you should
   explicitly test equivalent types.

Ownership Management
^^^^^^^^^^^^^^^^^^^^
Ownership Management includes things like tracking or limiting copies,
reference counting, and tracking parent-child relationships. It is often
closely tied to Lifetime Management. In some cases, the input type may provide
user defined copy and/or move semantics which affect how ownership management
is done. For these use cases, the following Equivalence Classes should be
considered (see `Copying & Moving
<_docs-testing-cpp-type-equivalence-classes-copy-and-moving>`_ for further
discussion):

* Trivially Copyable & Moveable
* Non-Trivially Copyable & Moveable
* Non-Trivially Moveable
* Non-Copyable & Non-Moveable

Transaction Management
^^^^^^^^^^^^^^^^^^^^^^
Finally, enforcing some API operation(s) on an Input Type is often very
specific to the situation and the types involved. Some common examples include,
locking/unlocking a concurrency primitive, flushing a buffer after some series
of writes, finishing a transaction in a database, etc. Many of these examples
boil down to “completion of a transaction”. In general, we leave it up to the
reader to consider what this means for their specific use case, but we use
“Transactable” to capture this concept of starting and stopping a transaction:

* Transactable
* Non-Transactable

Operating on a Resource Managing Type
-------------------------------------
Operating on resource managing types (e.g. sending a ``std::unique_ptr<T>`` via
IPC) is most often done in Containers and Communication Generics, so we will
limit the discussion here and recommend reviewing those sections.

Other
=====
There are a few additional classes of types which do not fit neatly into any of
the above categories, but which are useful to consider:

* Callables - A function pointer, lambda, class with the function call operator
  overloaded, etc. This may be used for things like work queues, predicate
  functions, or in the `strategy pattern
  <https://en.wikipedia.org/wiki/Strategy_pattern>`_.
* Inherited / Base Class - Consider the situation where the Input Type is
  ``BaseClass`` and the user passes in an object of ``ChildClass``. This is
  especially common in Containers and Communications use cases.

  * Abstract Class - Most cases of using an abstract class Input Type will fail
    to compile, and this could generally be considered a `negative test
    <https://en.wikipedia.org/wiki/Negative_testing>`_. However, it may be
    useful to consider what happens if a user attempts to use an abstract class.

* Void - Often results in meaningless code, but may be useful as a negative
  test.
* Empty Type or Null Type - Similar to Void, often useful as a negative test.

.. _docs-testing-cpp-type-equivalence-classes-recommendations:

---------------
Recommendations
---------------
The following table captures all of the identified Equivalence Classes from
above. It also provides recommendations for which Use Cases each Equivalence
Class is most useful for testing. For example, if you are implementing a
Generic that handles Lifetime Management, the table below will provide
recommendations for which Equivalence Classes you should consider in your
testing.

The set of Use Cases defined above (and used in the table below) is as follows:

* Logic
* Arithmetic
* Container
* Allocator
* Internal Communication
* External Communication
* Lifetime Management
* Ownership Management
* Transaction Management

Note that there are two additional “axes” to this table which should be
considered:

* `cv-qualification <https://en.cppreference.com/w/cpp/language/cv.html>`_
* References (`l-value
  <https://cppreference.com/w/cpp/language/reference.html#Lvalue_references>`_ /
  `r-value
  <https://cppreference.com/w/cpp/language/reference.html#Rvalue_references>`_)

This is not explicitly included in the table as it would explode the size of
the table without much intrinsic benefit. Readers should be aware of these
additional axes and consider whether testing cv-qualified or reference types is
relevant to them.

+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Equivalence Class (Sub-classes)            | Recommended Use Cases                                                                                                  | Examples of Types to Test                                        |
+============================================+========================================================================================================================+==================================================================+
| Boolean                                    | Logic, Arithmetic, Container                                                                                           | ``bool``                                                         |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Integer (Width, Signedness, BigInt)        | Logic, Arithmetic                                                                                                      | ``int8_t``, ``uint16_t``, ``Boost.Multiprecision``               |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Enumeration (Width)                        | Logic, Arithmetic                                                                                                      | ``enum``, ``enum class``                                         |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Fixed Point (Width, Signedness, Precision) | Logic, Arithmetic                                                                                                      | ``int16_t`` (/100), ``uint32_t`` (/1000)                         |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Floating Point (Width)                     | Logic, Arithmetic                                                                                                      | ``float``, ``double``                                            |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Pointer (Object, Function, Member)         | Logic, Arithmetic                                                                                                      | ``void*``, ``int*``, ``int (*)(int)``, ``int (MyClass::*)(int)`` |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Unit of Measure (Scale, Conversion)        | Logic, Arithmetic                                                                                                      | ``std::chrono::milliseconds``                                    |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Vector (Dimension)                         | Logic, Arithmetic                                                                                                      | ``Eigen::SparseVector``                                          |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Complex Number                             | Logic, Arithmetic                                                                                                      | ``std::complex``                                                 |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Matrix (Dimension)                         | Logic, Arithmetic                                                                                                      | ``Eigen::SparseMatrix``                                          |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Tensor (Rank, Dimension)                   | Logic, Arithmetic                                                                                                      | ``tensorflow::Tensor``                                           |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Default Constructible/Destructible         | Container, Allocator, Lifetime Management, Ownership Management                                                        | ``int``, ``void*``, ``struct X``                                 |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Default Constructible                  | Container, Allocator, Lifetime Management, Ownership Management, Transaction Management                                | ``std::lock_guard``                                              |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Constructible                          | Container, Allocator, Lifetime Management, Ownership Management                                                        | Singletons, ``Create()`` Classes                                 |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Default Destructible                   | Container, Allocator, Lifetime Management, Ownership Management                                                        | Abstract Base Classes, Reference Counted Classes                 |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Destructible                           | Container, Allocator, Lifetime Management, Ownership Management                                                        | Singletons, Reference Counted Classes                            |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Trivially Copyable & Moveable              | Container, Allocator, Internal Communication, Lifetime Management, Ownership Management                                | ``int``, ``void*``, ``struct X``                                 |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Trivially Copyable & Moveable          | Container, Allocator, Internal Communication, Lifetime Management, Ownership Management, Transaction Management        | ``std::shared_ptr``                                              |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Trivially Moveable                     | Container, Allocator, Internal Communication, Lifetime Management, Ownership Management, Transaction Management        | ``std::unique_ptr``                                              |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Copyable & Non-Moveable                | Container, Allocator, Internal Communication, Lifetime Management, Ownership Management, Transaction Management        | Singletons, Locks                                                |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Relational Operators Provided              | Logic, Container                                                                                                       | ``int``, ``std::string``                                         |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| No Relational Operators Provided           | Logic, Container                                                                                                       | ``std::unordered_map``                                           |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Equality Operators Provided                | Logic, Container                                                                                                       | ``int``, ``std::vector``                                         |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| No Equality Operators Provided             | Logic, Container                                                                                                       | ``std::function``                                                |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Hashable                                   | Container                                                                                                              | ``std::string``                                                  |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Hashable                               | Container                                                                                                              | ``class A``                                                      |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Serializable                               | External Communication                                                                                                 | Protobuf                                                         |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Serializable                           | External Communication                                                                                                 | ``class A``                                                      |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Transactable                               | Transaction Management                                                                                                 | Mutexes, Databases, File Handles                                 |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Non-Transactable                           | Transaction Management                                                                                                 | Semaphores                                                       |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Callable                                   | Logic, Container, Lifetime Management, Ownership Management                                                            | ``std::function``                                                |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Inheritance Hierarchy (Abstract Class)     | Logic, Container, Allocator, Internal Communication, Lifetime Management, Ownership Management, Transaction Management | Abstract Base Classes, Base Classes                              |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+
| Void / Null Type / Empty Type              | All                                                                                                                    | ``void``, ``std::nullptr_t``                                     |
+--------------------------------------------+------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------+

--------
Appendix
--------

References
==========
1. `https://en.cppreference.com/w/cpp/language/type-id.html
   <https://en.cppreference.com/w/cpp/language/type-id.html>`_
2. `https://en.cppreference.com/w/cpp/iterator/concepts.html
   <https://en.cppreference.com/w/cpp/iterator/concepts.html>`_
3. `https://blog.wingman-sw.com/tdd-guided-by-zombies
   <https://blog.wingman-sw.com/tdd-guided-by-zombies>`_
