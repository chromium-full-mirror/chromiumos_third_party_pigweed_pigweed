.. _module-pw_allocator_zephyr:

===================
pw_allocator_zephyr
===================
.. pigweed-module::
   :name: pw_allocator_zephyr

``pw_allocator_zephyr`` implements the :ref:`module-pw_allocator` interface for
the Zephyr RTOS kernel heapsm using the
`Zephyr API <https://github.com/zephyrproject-rtos/zephyr>`_.

SynchronizedHeapAllocator
=========================
Wraps a specific Zephyr kernel :cpp:class:`k_heap` to provide allocations from
it.

SystemHeapAllocator
===================
Wraps the Zephyr shared system heap API to provide allocations from it.

Using this require configuring the global heap size using
``CONFIG_HEAP_MEM_POOL_SIZE=<size>`` in your Zephyr kernel configuration.
