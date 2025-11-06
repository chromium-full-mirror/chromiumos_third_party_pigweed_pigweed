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

#define PW_LOG_MODULE_NAME PW_ASYNC2_CONFIG_LOG_MODULE_NAME
#define PW_LOG_LEVEL PW_ASYNC2_CONFIG_LOG_LEVEL

#include "pw_async2/dispatcher_base.h"

#include <iterator>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_async2/internal/config.h"
#include "pw_async2/owned_task.h"
#include "pw_async2/waker.h"
#include "pw_log/log.h"
#include "pw_log/tokenized_args.h"

namespace pw::async2 {

void NativeDispatcherBase::Deregister() {
  std::lock_guard lock(impl::dispatcher_lock());
  UnpostTaskList(woken_);
  UnpostTaskList(sleeping_);
}

void NativeDispatcherBase::Post(Task& task) {
  bool wake_dispatcher = false;
  {
    std::lock_guard lock(impl::dispatcher_lock());
    task.PostTo(*this);
    woken_.push_back(task);
    if (wants_wake_) {
      wake_dispatcher = true;
      wants_wake_ = false;
    }
  }
  // Unlike in `WakeTask`, here we know that the `Dispatcher` will not be
  // destroyed out from under our feet because we're in a method being called on
  // the `Dispatcher` by a user.
  if (wake_dispatcher) {
    Wake();
  }
}

NativeDispatcherBase::SleepInfo NativeDispatcherBase::AttemptRequestWake(
    bool allow_empty) {
  std::lock_guard lock(impl::dispatcher_lock());
  // Don't allow sleeping if there are already tasks waiting to be run.
  if (!woken_.empty()) {
    PW_LOG_DEBUG("Dispatcher will not sleep due to nonempty task queue");
    return SleepInfo::DontSleep();
  }
  if (!allow_empty && sleeping_.empty()) {
    PW_LOG_DEBUG("Dispatcher will not sleep due to empty sleep queue");
    return SleepInfo::DontSleep();
  }
  /// Indicate that the `Dispatcher` is sleeping and will need a `DoWake` call
  /// once more work can be done.
  wants_wake_ = true;
  sleep_count_.Increment();
  // Once timers are added, this should check them.
  return SleepInfo::Indefinitely();
}

NativeDispatcherBase::RunTaskResult NativeDispatcherBase::RunOneTask(
    Dispatcher& dispatcher) {
  Task* task;
  Task::RunResult run_result;
  RunTaskResult task_state;

  {
    std::lock_guard lock(impl::dispatcher_lock());

    task = PopWokenTask();
    if (task == nullptr) {
      PW_LOG_DEBUG("Dispatcher has no woken tasks to run");
      return sleeping_.empty() ? kNoTasks : kNoReadyTasks;
    }

    run_result = task->RunInDispatcher(dispatcher);
    tasks_polled_.Increment();

    if (!woken_.empty()) {
      task_state = kReadyTasks;
    } else if (!sleeping_.empty()) {
      task_state = kNoReadyTasks;
    } else {
      task_state = kNoTasks;
    }
  }

  // If this is an OwnedTask, then no other threads should be accessing it, so
  // it is safe destroy it without holding impl::dispatcher_lock().
  if (run_result == Task::kCompletedNeedsDestroy) {
    static_cast<OwnedTask&>(*task).Destroy();
    tasks_completed_.Increment();
  } else if (run_result == Task::kCompleted) {
    tasks_completed_.Increment();
  }

  return task_state;
}

void NativeDispatcherBase::UnpostTaskList(IntrusiveList<Task>& list) {
  while (!list.empty()) {
    list.front().Unpost();
    list.pop_front();
  }
}

void NativeDispatcherBase::RemoveWokenTaskLocked(Task& task) {
  woken_.remove(task);
}

void NativeDispatcherBase::RemoveSleepingTaskLocked(Task& task) {
  sleeping_.remove(task);
}

void NativeDispatcherBase::AddSleepingTaskLocked(Task& task) {
  sleeping_.push_front(task);
}

void NativeDispatcherBase::WakeTask(Task& task) {
  if (!task.Wake()) {
    return;
  }

  woken_.push_back(task);
  if (wants_wake_) {
    // It's quite annoying to make this call under the lock, as it can result in
    // extra thread wakeup/sleep cycles.
    //
    // However, releasing the lock first would allow for the possibility that
    // the `Dispatcher` has been destroyed, making the call invalid.
    Wake();
  }
}

Task* NativeDispatcherBase::PopWokenTask() {
  if (woken_.empty()) {
    return nullptr;
  }
  Task& task = woken_.front();
  woken_.pop_front();
  return &task;
}

// TODO: b/456478818 - Provide task iteration API and rework LogRegisteredTasks
//     to use it.
void NativeDispatcherBase::LogRegisteredTasks() {
  PW_LOG_INFO("pw::async2::Dispatcher");
  std::lock_guard lock(impl::dispatcher_lock());

  PW_LOG_INFO("Woken tasks:");
  for (const Task& task : woken_) {
    PW_LOG_INFO("  - " PW_LOG_TOKEN_FMT() ":%p",
                task.name_,
                static_cast<const void*>(&task));
  }
  PW_LOG_INFO("Sleeping tasks:");
  for (const Task& task : sleeping_) {
    int waker_count = static_cast<int>(
        std::distance(task.wakers_.begin(), task.wakers_.end()));

    PW_LOG_INFO("  - " PW_LOG_TOKEN_FMT() ":%p (%d wakers)",
                task.name_,
                static_cast<const void*>(&task),
                waker_count);

    LogTaskWakers(task);
  }
}

void NativeDispatcherBase::LogTaskWakers([[maybe_unused]] const Task& task) {
#if PW_ASYNC2_DEBUG_WAIT_REASON
  int i = 0;
  for (const Waker& waker : task.wakers_) {
    i++;
    if (waker.wait_reason_ != log::kDefaultToken) {
      PW_LOG_INFO("    * Waker %d: " PW_LOG_TOKEN_FMT("pw_async2"),
                  i,
                  waker.wait_reason_);
    }
  }
#endif  // PW_ASYNC2_DEBUG_WAIT_REASON
}

}  // namespace pw::async2
