.. _docs-build-bazel:

=====
Bazel
=====
Pigweed has full support for Bazel. See :ref:`docs-concepts-build-system` for
more information.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Install Bazel
      :link: docs-install-bazel
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      Recommendations on how to install Bazel.

   .. grid-item-card:: :octicon:`code-square` Bazel quickstart
      :link: https://cs.opensource.google/pigweed/quickstart/bazel
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      Fork our minimal, Bazel-based starter project to create a new
      Pigweed project from scratch. The project includes a basic
      blinky LED program that runs on Raspberry Pi Picos and can
      be simulated on your development host.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Bazel integration
      :link: docs-bazel-integration
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Learn how to use Pigweed in an existing Bazel project: add Pigweed
      as a dependency, start using Pigweed modules, and set up static and
      runtime analysis.

   .. grid-item-card:: :octicon:`rocket` Bazel build compatibility patterns
      :link: docs-bazel-compatibility
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      A deep-dive into the Bazel patterns Pigweed uses to express that a build
      target is compatible with a platform

.. toctree::
   :maxdepth: 2
   :hidden:

   self
   install
   quickstart
   integration/index
   compatibility
