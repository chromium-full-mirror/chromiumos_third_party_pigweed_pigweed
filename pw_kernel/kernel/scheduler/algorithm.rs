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

use foreign_box::ForeignBox;
use list::ForeignList;

use crate::Kernel;
use crate::scheduler::Priority;
use crate::thread::{Thread, ThreadListAdapter};

/// The reason a thread is being rescheduled. This can be used as a hint to the
/// scheduling algorithm to determine where the thread should end up in the
/// queue of pending threads.
pub enum RescheduleReason {
    /// The currently running thread has been preempted causing it to be re-added to the scheduler.
    Preempted,
    /// A scheduler tick occurred causing the currently running thread to be readded to the scheduler.
    Ticked,
    /// A currently waiting thread has been unblocked.
    Woken,
    /// A thread has been started and is being added to the scheduler for the first time.
    Started,
}

/// Per-thread state used by the scheduling algorithm.
pub struct SchedulerAlgorithmThreadState {
    /// The current priority of the thread.
    #[allow(dead_code)]
    current_priority: Priority,
}

impl SchedulerAlgorithmThreadState {
    #[allow(clippy::new_without_default)]
    pub const fn new(current_priority: Priority) -> Self {
        Self { current_priority }
    }
}

/// The algorithm used for determining which thread to run next.
pub struct SchedulerAlgorithm<K: Kernel> {
    run_queue: ForeignList<Thread<K>, ThreadListAdapter<K>>,
}

unsafe impl<K: Kernel> Sync for SchedulerAlgorithm<K> {}
unsafe impl<K: Kernel> Send for SchedulerAlgorithm<K> {}

impl<K: Kernel> SchedulerAlgorithm<K> {
    #[allow(clippy::new_without_default)]
    pub const fn new() -> Self {
        Self {
            run_queue: ForeignList::new(),
        }
    }

    pub fn schedule_thread(&mut self, thread: ForeignBox<Thread<K>>, reason: RescheduleReason) {
        let run_queue = &mut self.run_queue;
        match reason {
            RescheduleReason::Preempted => {
                self.run_queue.push_front(thread);
            }
            RescheduleReason::Started | RescheduleReason::Ticked | RescheduleReason::Woken => {
                run_queue.push_back(thread);
            }
        }
    }

    pub fn get_next_thread(&mut self) -> Option<ForeignBox<Thread<K>>> {
        self.run_queue.pop_head()
    }
}
