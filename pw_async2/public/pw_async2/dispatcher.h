// Copyright 2023 The Pigweed Authors
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

#include "pw_async2/context.h"
#include "pw_async2/dispatcher_native.h"
#include "pw_async2/lock.h"
#include "pw_async2/task.h"
#include "pw_async2/waker.h"

namespace pw::async2 {

/// @submodule{pw_async2,core}

/// A single-threaded cooperatively scheduled runtime for async tasks.
class Dispatcher {
 public:
  /// Constructs a new async Dispatcher.
  Dispatcher() = default;
  Dispatcher(Dispatcher&) = delete;
  Dispatcher(Dispatcher&&) = delete;
  Dispatcher& operator=(Dispatcher&) = delete;
  Dispatcher& operator=(Dispatcher&&) = delete;
  ~Dispatcher() { native_.Deregister(); }

  /// Tells the ``Dispatcher`` to run ``Task`` to completion.
  /// This method does not block.
  ///
  /// After ``Post`` is called, ``Task::Pend`` will be invoked once.
  /// If ``Task::Pend`` does not complete, the ``Dispatcher`` will wait
  /// until the ``Task`` is "awoken", at which point it will call ``Pend``
  /// again until the ``Task`` completes.
  ///
  /// This method is thread-safe and interrupt-safe.
  void Post(Task& task) PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    native_.Post(task);
  }

  /// Runs tasks until none are able to make immediate progress.
  Poll<> RunUntilStalled() PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    return native_.DoRunUntilStalled(*this);
  }

  /// Runs until all tasks complete.
  void RunToCompletion() PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    native_.DoRunToCompletion(*this);
  }

  /// Outputs log statements about the tasks currently registered with this
  /// dispatcher.
  void LogRegisteredTasks() { native_.LogRegisteredTasks(); }

  /// Returns the total number of times the dispatcher has called a task's
  /// ``Pend()`` method.
  uint32_t tasks_polled() const { return native_.tasks_polled(); }

  /// Returns the total number of tasks the dispatcher has run to completion.
  uint32_t tasks_completed() const { return native_.tasks_completed(); }

  /// Returns a reference to the native backend-specific dispatcher type.
  pw::async2::backend::NativeDispatcher& native() { return native_; }

 private:
  pw::async2::backend::NativeDispatcher native_;
};

/// @}

}  // namespace pw::async2
