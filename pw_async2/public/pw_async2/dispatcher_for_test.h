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

#include "pw_async2/dispatcher.h"

namespace pw::async2 {
namespace internal {

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

/// `DispatcherForTest` is a `Dispatcher` implementation to use in unit tests.
///
/// This class will be restructured as a facade when multiple `Dispatcher`
/// implementations are supported.
class DispatcherForTest : public Dispatcher {
 public:
  DispatcherForTest() = default;

  DispatcherForTest(const DispatcherForTest&) = delete;
  DispatcherForTest& operator=(const DispatcherForTest&) = delete;

  DispatcherForTest(DispatcherForTest&&) = delete;
  DispatcherForTest& operator=(DispatcherForTest&&) = delete;

  template <typename Pendable>
  Poll<PendOutputOf<Pendable>> RunInTaskUntilStalled(Pendable& pendable)
      PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    internal::PendableAsTaskWithOutput<Pendable> task(pendable);
    Post(task);
    RunUntilStalled().IgnorePoll();

    // Ensure that the task is no longer registered, as it will be destroyed
    // once we return.
    //
    // This operation will not block because we are on the dispatcher thread
    // and the dispatcher is not currently running (we just ran it).
    task.Deregister();

    return task.TakePoll();
  }
};

}  // namespace pw::async2
