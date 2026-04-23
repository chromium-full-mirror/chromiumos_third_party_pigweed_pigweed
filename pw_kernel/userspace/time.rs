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

use kernel_config::{KernelConfig, KernelConfigInterface};
use syscall_user::{SysCall, SysCallInterface};
pub use time::Clock;

pub struct SystemClock;

impl time::Clock for SystemClock {
    const TICKS_PER_SEC: u64 = KernelConfig::SYSTEM_CLOCK_HZ;
    fn now() -> time::Instant<Self> {
        // Use the debug_clock_now() system call until userspace time is designed
        // and implemented.
        let ticks = SysCall::debug_clock_now();
        Instant::from_ticks(ticks)
    }
}

pub type Instant = time::Instant<SystemClock>;
pub type Duration = time::Duration<SystemClock>;
