// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include "pw_async2/poll.h"

namespace pw::async2 {

class Task;

/// @submodule{pw_async2,context}

/// Asynchronous context for functions running on a `Dispatcher`.
///
/// `Context` objects are provided when a task is run by a `Dispatcher`. The
/// `Context&` is passed to `Task::Pend`, and can be passed to futures, other
/// tasks, or other async operations. `Context`'s primary purpose is to allow
/// async code to store wakers.
class Context {
 public:
  Context(const Context&) = delete;
  Context(Context&&) = delete;

  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&) = delete;

  /// Queues the current `Task::Pend` to run again in the future, possibly after
  /// other work is performed.
  ///
  /// This may be used by `Task` implementations that wish to provide additional
  /// fairness by yielding to the dispatch loop rather than perform too much
  /// work in a single iteration.
  ///
  /// This is semantically equivalent to calling:
  ///
  /// @code{.cpp}
  ///   Waker waker;
  ///   PW_ASYNC_STORE_WAKER(cx, waker, ...);
  ///   waker.Wake();
  /// @endcode
  void ReEnqueue();  // Implemented inline in task.h after Task is defined.

  /// Indicates that the task has not completed, but that it also does not need
  /// to register a waker and go to sleep. This results in the task being
  /// removed from the dispatcher, requiring it to be manually re-posted to run
  /// again.
  PendingType Unschedule();

 private:
  friend class Task;

  constexpr Context() = default;
};

/// @endsubmodule

}  // namespace pw::async2
