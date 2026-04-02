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
use userspace::{entry, syscall};

#[unsafe(no_mangle)]
pub extern "C" fn test_thread_entry(_arg: usize) {
    info!("Test thread started. Entering infinite loop...");
    loop {
        core::hint::spin_loop();
    }
}

fn do_test() -> Result<()> {
    info!("🔄 [User Process Termination] RUNNING");

    // We cannot test successful process_terminate on our own process because
    // it kills the main thread, resulting in a kernel panic (run queue empty),
    // and thus a failed test. Furthermore, userspace apps cannot spawn new processes
    // or hold handles to other processes.
    // So we test that process_terminate correctly rejects invalid handles.

    // TEST_THREAD is a ThreadObject, not a ProcessObject!
    info!("🔄 ├─ Testing process_terminate on a Thread handle");
    let result = syscall::process_terminate(handle::TEST_THREAD);
    if result != Err(pw_status::Error::Unimplemented.into()) {
        pw_log::error!("Expected Unimplemented when terminating a thread handle");
        return Err(pw_status::Error::Internal.into());
    }

    info!("🔄 ├─ Testing process_terminate on an invalid handle");
    let result = syscall::process_terminate(0xdeadbeef);
    if result != Err(pw_status::Error::OutOfRange.into()) {
        pw_log::error!("Expected OutOfRange on a bogus handle");
        return Err(pw_status::Error::Internal.into());
    }

    info!("🔄 ├─ Testing process_terminate on an actual process handle");
    let result = syscall::process_terminate(handle::EXTRA_PROCESS);
    if result.is_err() {
        pw_log::error!("Failed to terminate extra process");
        return Err(pw_status::Error::Internal.into());
    }

    info!("🔄 ├─ Waiting for extra process to become joinable");
    syscall::object_wait(
        handle::EXTRA_PROCESS,
        syscall::Signals::JOINABLE,
        userspace::time::Instant::from_ticks(u64::MAX),
    )?;

    info!("🔄 ├─ Joining extra process");
    syscall::process_join(handle::EXTRA_PROCESS)?;

    info!("✅ └─ PASSED");

    Ok(())
}

#[entry]
fn main_entry() -> ! {
    let ret = do_test();

    if ret.is_err() {
        pw_log::error!("❌ ├─ FAILED");
        pw_log::error!("❌ └─ status code: {}", ret.status_code() as u32);
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
