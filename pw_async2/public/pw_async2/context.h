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

/// Context for an asynchronous `Task`.
///
/// This object contains resources needed for scheduling asynchronous work, such
/// as the current `Dispatcher` and the `Waker` for the current task.
///
/// `Context` s are most often created by `Dispatcher` s, which pass them
/// into `Task::Pend`.
class Context {
 public:
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
  /// Waker waker;
  /// PW_ASYNC_STORE_WAKER(cx, waker, ...);
  /// waker.Wake();
  /// @endcode
  void ReEnqueue();  // Implemented inline in waker.h after Waker is defined.

  /// Indicates that the task has not completed, but that it also does not need
  /// to register a waker and go to sleep. This results in the task being
  /// removed from the dispatcher, requiring it to be manually re-posted to run
  /// again.
  PendingType Unschedule() {
    requires_waker_ = false;
    return Pending();
  }

 private:
  friend class Task;
  friend class Waker;

  /// Creates a new `Context` for the `Task` being run within a `Dispatcher`.
  explicit constexpr Context(Task& task)
      : task_(&task), requires_waker_(true) {}

  Task* task_;
  bool requires_waker_;
};

/// @endsubmodule

}  // namespace pw::async2
