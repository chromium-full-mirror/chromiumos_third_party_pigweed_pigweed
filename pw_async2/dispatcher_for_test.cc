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

#include "pw_async2/dispatcher_for_test.h"

#include "pw_assert/check.h"

namespace pw::async2 {

template <>
bool DispatcherForTestFacade<
    backend::NativeDispatcherForTest>::DoRunUntilStalled() {
  State state;
  Task* task;
  while ((task = PopTaskToRun(state)) != nullptr) {
    tasks_polled_ += 1;
    if (RunTask(*task) == RunTaskResult::kCompleted) {
      tasks_completed_ += 1;
    }
  }
  return state != State::kNoTasks;
}

template <>
void DispatcherForTestFacade<backend::NativeDispatcherForTest>::DoWake() {
  wake_count_ += 1;
  native().DoWake();
}

template <>
void DispatcherForTestFacade<
    backend::NativeDispatcherForTest>::DoWaitForWake() {
  PW_CHECK(
      blocking_is_allowed_,
      "The DispatcherForTest attempted to block a thread to wait for a wake. "
      "If this is intentional, call dispatcher.AllowBlocking() to allow it.");
  native().DoWaitForWake();
}

}  // namespace pw::async2
