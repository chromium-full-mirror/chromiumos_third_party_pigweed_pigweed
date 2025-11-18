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

#include "pw_async2/runnable_dispatcher.h"

namespace pw::async2 {
namespace internal {

// This should be updated to work for futures instead of generic pendables.
template <typename Pendable>
class PendableAsTaskWithOutput : public Task {
 public:
  using value_type = PendOutputOf<Pendable>;
  PendableAsTaskWithOutput(Pendable& pendable)
      : pendable_(pendable), output_(Pending()) {}

  Poll<value_type> TakePoll() { return std::move(output_); }

 private:
  Poll<> DoPend(Context& cx) final {
    output_ = pendable_.Pend(cx);
    return output_.Readiness();
  }
  Pendable& pendable_;
  Poll<value_type> output_;
};

}  // namespace internal

/// @module{pw_async2}

/// `DispatcherForTestFacade` defines the interface for `DispatcherForTest`.
/// Backends must provide:
///
/// - A ``pw_async2_backend/native_dispatcher_for_test.h`` header.
/// - A class or alias named ``pw::async2::backend::NativeDispatcherForTest``
///   that:
///   - Implements `RunnableDispatcher`.
///   - Is default constructible.
template <typename Native>
class DispatcherForTestFacade final : public RunnableDispatcher {
 public:
  /// `DispatcherForTest` is default constructible.
  DispatcherForTestFacade() = default;

  DispatcherForTestFacade(const DispatcherForTestFacade&) = delete;
  DispatcherForTestFacade& operator=(const DispatcherForTestFacade&) = delete;

  DispatcherForTestFacade(DispatcherForTestFacade&&) = delete;
  DispatcherForTestFacade& operator=(DispatcherForTestFacade&&) = delete;

  /// Whether to allow the dispatcher to block by calling `DoWaitForWake`.
  /// `RunToCompletion` may block the thread if there are no tasks ready to run.
  void AllowBlocking() { blocking_is_allowed_ = true; }

  template <typename Pendable>
  Poll<internal::PendOutputOf<Pendable>> RunInTaskUntilStalled(
      Pendable& pendable) PW_LOCKS_EXCLUDED(internal::lock()) {
    internal::PendableAsTaskWithOutput<Pendable> task(pendable);
    native().Post(task);
    native().RunUntilStalled();

    // Ensure that the task is no longer registered, as it will be destroyed
    // once we return.
    //
    // This operation will not block because we are on the dispatcher thread
    // and the dispatcher is not currently running (we just ran it).
    task.Deregister();

    return task.TakePoll();
  }

  /// Returns the total number of times the dispatcher has called a task's
  /// ``Pend()`` method.
  uint32_t tasks_polled() const { return tasks_polled_; }

  /// Returns the total number of tasks the dispatcher has run to completion.
  uint32_t tasks_completed() const { return tasks_completed_; }

  /// Returns the total number of times the dispatcher has been woken.
  uint32_t wake_count() const { return wake_count_; }

 private:
  // These functions are implemented in dispatcher_for_test.cc for the
  // NativeDispatcherForTest specialization only.
  bool DoRunUntilStalled() override;

  void DoWake() override;

  void DoWaitForWake() override;

  RunnableDispatcher& native() { return native_; }

  Native native_;
  bool blocking_is_allowed_ = false;

  // TODO: b/401049619 - Optionally provide metrics for production dispatchers.
  uint32_t tasks_polled_ = 0u;
  uint32_t tasks_completed_ = 0u;
  uint32_t wake_count_ = 0u;
};

}  // namespace pw::async2

#include "pw_async2_backend/native_dispatcher_for_test.h"

namespace pw::async2 {

/// `DispatcherForTest` is a `RunnableDispatcher` implementation to use in unit
/// tests. See `DispatcherForTestFacade` for details.
using DispatcherForTest =
    DispatcherForTestFacade<backend::NativeDispatcherForTest>;

}  // namespace pw::async2
