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
use userspace::time::{Clock, Duration, SystemClock, sleep_until};
use userspace::{entry, syscall};

const PROCESS_JOIN_TIMEOUT: Duration = Duration::from_secs(5);

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
        pw_log::error!("Expected OutOfRange on an invalid handle");
        return Err(pw_status::Error::Internal.into());
    }

    for pass in 0..2 {
        info!("🔄 ├─ Iteration {}", pass as u32);

        // Give `EXTRA_PROCESS` a chance to start.
        //
        // TODO: https://pwbug.dev/505490714 - Query process state to ensure it's running
        if let Err(err) = sleep_until(SystemClock::now() + Duration::from_millis(200)) {
            pw_log::error!("Failed to sleep for 200ms");
            return Err(err);
        }

        info!("🔄 ├─ Testing clean exit");
        info!("🔄 ├─ Waiting for clean exit process to become joinable");
        if let Err(err) = syscall::object_wait(
            handle::CLEAN_EXIT_PROCESS,
            syscall::Signals::JOINABLE,
            SystemClock::now() + PROCESS_JOIN_TIMEOUT,
        ) {
            pw_log::error!("Failed to wait for extra process to become joinable");
            return Err(err);
        }

        info!("🔄 ├─ Joining clean exit process");
        match syscall::process_join(handle::CLEAN_EXIT_PROCESS) {
            Err(err) => return Err(err),
            Ok(syscall::ExitStatus::Success(42)) => (),
            Ok(_) => {
                pw_log::error!("❌ ├─ Clean exit process joined with unexpected status");
                return Err(pw_status::Error::Internal);
            }
        }
        info!("🔄 ├─ Clean exit process joined");

        info!("🔄 ├─ Testing forced exit");
        // The forced_exit process is started automatically by the kernel on boot.
        // On subsequent iterations, we start it manually below.
        info!("🔄 ├─ Terminating forced exit process");
        syscall::process_terminate(handle::FORCED_EXIT_PROCESS)?;

        info!("🔄 ├─ Waiting for forced exit process to become joinable");
        if let Err(err) = syscall::object_wait(
            handle::FORCED_EXIT_PROCESS,
            syscall::Signals::JOINABLE,
            SystemClock::now() + PROCESS_JOIN_TIMEOUT,
        ) {
            pw_log::error!("❌ Error waiting for process to be joinable");
            return Err(err);
        }

        info!("🔄 ├─ Joining forced exit process");
        let status = syscall::process_join(handle::FORCED_EXIT_PROCESS)?;
        if status != syscall::ExitStatus::TerminatedBySyscall {
            pw_log::error!(
                "❌ ├─ Process joined with unexpected status (expected TerminatedBySyscall)"
            );
            return Err(pw_status::Error::Internal.into());
        }
        info!("🔄 ├─ Forced exit process joined");

        info!("🔄 ├─ Testing exception exit");
        info!("🔄 ├─ Waiting for exception exit process to become joinable");
        if let Err(err) = syscall::object_wait(
            handle::EXCEPTION_EXIT_PROCESS,
            syscall::Signals::JOINABLE,
            SystemClock::now() + PROCESS_JOIN_TIMEOUT,
        ) {
            pw_log::error!("❌ Error waiting for exception process to be joinable");
            return Err(err);
        }

        info!("🔄 ├─ Joining exception exit process");
        let status = syscall::process_join(handle::EXCEPTION_EXIT_PROCESS)?;
        if status != syscall::ExitStatus::UnhandledException(0) {
            pw_log::error!(
                "❌ ├─ Process joined with unexpected status (expected UnhandledException(0))"
            );
            return Err(pw_status::Error::Internal.into());
        }
        info!("🔄 ├─ Exception exit process joined");

        info!("🔄 ├─ Restarting test processes");
        syscall::process_start(handle::CLEAN_EXIT_PROCESS)?;
        syscall::process_start(handle::FORCED_EXIT_PROCESS)?;
        syscall::process_start(handle::EXCEPTION_EXIT_PROCESS)?;
    }

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
