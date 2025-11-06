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

#include "pw_async2/dispatcher_native.h"

#include <mutex>
#include <optional>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"

namespace pw::async2::backend {

Poll<> NativeDispatcher::DoRunUntilStalled(Dispatcher& dispatcher) {
  while (true) {
    RunTaskResult result = RunOneTask(dispatcher);
    if (result == kNoTasks) {
      return Ready();
    }
    if (result == kNoReadyTasks) {
      return Pending();
    }
  }
}

void NativeDispatcher::DoRunToCompletion(Dispatcher& dispatcher) {
  while (true) {
    RunTaskResult result = RunOneTask(dispatcher);
    if (result == kNoTasks) {
      return;
    }
    if (result != kReadyTasks) {
      SleepInfo sleep_info = AttemptRequestWake(/*allow_empty=*/false);
      if (sleep_info.should_sleep()) {
        notify_.acquire();
      }
    }
  }
}

}  // namespace pw::async2::backend
