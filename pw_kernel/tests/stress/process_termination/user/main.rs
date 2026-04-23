// Copyright 2026 The Pigweed Authors
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

#![no_std]
#![no_main]

use main_codegen::handle;
use pw_log::info;
use pw_status::{Result, StatusCode};
use time::Clock as _;
use userspace::time::{Clock, Duration};
use userspace::{entry, syscall};

fn do_test() -> Result<()> {
    info!("🔄 [User Process Termination Stress] RUNNING");

    let mut pass = 0;

    loop {
        info!("🔄 ├─ Pass {}", pass as u32);

        let result = syscall::process_terminate(handle::EXTRA_PROCESS);
        if result.is_err() {
            pw_log::error!("Failed to terminate extra process");
            return Err(pw_status::Error::Internal.into());
        }

        info!("🔄 ├─ Waiting", pass as u32);
        let deadline = Clock::now() + Duration::from_secs(5);
        syscall::object_wait(handle::EXTRA_PROCESS, syscall::Signals::JOINABLE, deadline)?;

        info!("🔄 ├─ Joining", pass as u32);
        syscall::process_join(handle::EXTRA_PROCESS)?;

        info!("🔄 ├─ Starting", pass as u32);
        syscall::process_start(handle::EXTRA_PROCESS)?;

        pass += 1;
    }
}

#[entry]
fn main_entry() -> ! {
    let ret = do_test();

    if ret.is_err() {
        pw_log::error!("❌ FAILED");
        pw_log::error!("❌ status code: {}", ret.status_code() as u32);
    }

    let _ = syscall::debug_shutdown(ret);
    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    pw_log::error!("❌ PANIC");
    let _ = syscall::debug_shutdown(Err(pw_status::Error::Internal.into()));
    loop {}
}
