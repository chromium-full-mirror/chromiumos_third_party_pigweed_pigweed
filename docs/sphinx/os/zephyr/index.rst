.. _docs-os-zephyr:

======
Zephyr
======
Pigweed has preliminary support for `Zephyr <https://www.zephyrproject.org/>`_.
See the docs for these modules for more information:

.. toctree::
   :maxdepth: 1

   pw_allocator_zephyr <../../pw_allocator_zephyr/docs>
   pw_assert_zephyr <../../pw_assert_zephyr/docs>
   pw_chrono_zephyr <../../pw_chrono_zephyr/docs>
   pw_interrupt_zephyr <../../pw_interrupt_zephyr/docs>
   pw_log_zephyr <../../pw_log_zephyr/docs>
   pw_sync_zephyr <../../pw_sync_zephyr/docs>
   pw_spi_zephyr <../../pw_spi_zephyr/docs>
   pw_sys_io_zephyr <../../pw_sys_io_zephyr/docs>
   pw_thread_zephyr <../../pw_thread_zephyr/docs>

.. note:: The version of Zephyr bundled with `pw package install zephyr` is
   being migrated to v3.6 as we near the latest release.

.. _docs-os-zephyr-get-started:

-----------------------------------
Get started with Zephyr and Pigweed
-----------------------------------
1. Check out the `zephyr_pigweed`_ repository for an example of a Zephyr starter
   project that has been set up to use Pigweed.
2. See :ref:`docs-os-zephyr-kconfig` to find the Kconfig options for
   enabling individual Pigweed modules and features.

-------
Testing
-------
To test against Zephyr, first go through the `zephyr_pigweed`_ tutorial.
Once set up, simply invoke:

.. code-block:: bash

   $ source ${PW_ROOT}/activate.sh
   $ west twister -T ${PW_ROOT}

.. attention:: Testing has only been verified with `-p native_posix`. Proceed with caution.

.. _zephyr_pigweed: https://github.com/yperess/zephyr-pigweed/

.. toctree::
   :hidden:

   kconfig
