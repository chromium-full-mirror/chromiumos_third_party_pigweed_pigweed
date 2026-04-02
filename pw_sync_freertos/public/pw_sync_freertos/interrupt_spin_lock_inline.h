// Copyright 2020 The Pigweed Authors
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

#include "pw_assert/assert.h"
#include "pw_interrupt/context.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync_freertos/config.h"
#include "task.h"

namespace pw::sync {

#if PW_SYNC_FREERTOS_INTERRUPT_SPIN_LOCK_USES_SCHEDULER_LOCK
#if (INCLUDE_xTaskGetSchedulerState != 1) && (configUSE_TIMERS != 1)
#error "xTaskGetSchedulerState is required for pw::sync::InterruptSpinLock"
#endif  // (INCLUDE_xTaskGetSchedulerState != 1) && (configUSE_TIMERS != 1)
#endif  // PW_SYNC_FREERTOS_INTERRUPT_SPIN_LOCK_USES_SCHEDULER_LOCK

constexpr InterruptSpinLock::InterruptSpinLock()
    : native_type_{.locked = false, .saved_interrupt_mask = 0} {}

inline InterruptSpinLock::native_handle_type
InterruptSpinLock::native_handle() {
  return native_type_;
}

inline bool InterruptSpinLock::try_lock() {
  // This backend does not support SMP and on a uniprocesor we cannot actually
  // fail to acquire the lock. Recursive locking is already detected by lock().
  lock();
  return true;
}

inline void InterruptSpinLock::lock() {
  if (interrupt::InInterruptContext()) {
    native_type_.saved_interrupt_mask = taskENTER_CRITICAL_FROM_ISR();
  } else {  // Task context
#if PW_SYNC_FREERTOS_INTERRUPT_SPIN_LOCK_USES_SCHEDULER_LOCK
    // Suspending the scheduler ensures that kernel API calls that occur
    // within the critical section will not preempt the current task
    // (if called from a thread context).  Otherwise, kernel APIs called
    // from within the critical section may preempt the running task if
    // the port implements portYIELD synchronously.
    // Note: calls to vTaskSuspendAll(), like taskENTER_CRITICAL() can
    // be nested.
    // Note: vTaskSuspendAll()/xTaskResumeAll() are not safe to call before the
    // scheduler has been started.
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
      vTaskSuspendAll();
    }
#endif  // PW_SYNC_FREERTOS_INTERRUPT_SPIN_LOCK_USES_SCHEDULER_LOCK
    taskENTER_CRITICAL();
  }
  PW_DASSERT(!native_type_.locked);  // We can't deadlock here so crash instead.
  native_type_.locked = true;
}

inline void InterruptSpinLock::unlock() {
  native_type_.locked = false;
  if (interrupt::InInterruptContext()) {
    taskEXIT_CRITICAL_FROM_ISR(native_type_.saved_interrupt_mask);
  } else {  // Task context
    taskEXIT_CRITICAL();
#if PW_SYNC_FREERTOS_INTERRUPT_SPIN_LOCK_USES_SCHEDULER_LOCK
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
      xTaskResumeAll();
    }
#endif  // PW_SYNC_FREERTOS_INTERRUPT_SPIN_LOCK_USES_SCHEDULER_LOCK
  }
}

}  // namespace pw::sync
