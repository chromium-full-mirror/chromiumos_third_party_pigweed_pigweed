.. _module-pw_async2-dispatcher:

==========
Dispatcher
==========
.. pigweed-module-subpage::
   :name: pw_async2

``pw_async2`` dispatchers implement the :cc:`pw::async2::Dispatcher` interface.
The :cc:`pw::async2::RunnableDispatcher` class can optionally be used to support
running the dispatcher directly in a thread.

Pigweed provides two :cc:`Dispatcher <pw::async2::Dispatcher>` implementations:

* :cc:`pw::async2::BasicDispatcher` is a simple thread-notification-based
  :cc:`RunnableDispatcher <pw::async2::RunnableDispatcher>` implementation.
* :cc:`pw::async2::EpollDispatcher` is a :cc:`RunnableDispatcher
  <pw::async2::RunnableDispatcher>` backed by Linux's `epoll`_ notification
  system.

.. _epoll: https://man7.org/linux/man-pages/man7/epoll.7.html
