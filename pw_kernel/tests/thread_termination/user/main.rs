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

use core::sync::atomic::{AtomicU32, Ordering};

use main_codegen::handle;
use pw_log::info;
use pw_status::{Result, StatusCode};
use userspace::{entry, syscall};

// NOTE: Atomic operations will not work on platforms without atomic support.
static THREAD_DONE: AtomicU32 = AtomicU32::new(0);

#[unsafe(no_mangle)]
pub extern "C" fn test_thread_entry(_arg: usize) -> ! {
    info!("Test thread started");
    THREAD_DONE.store(1, Ordering::SeqCst);
    info!("Test thread exiting");

    let _res = syscall::thread_terminate(handle::TEST_THREAD);
    pw_log::error!("thread_terminate FAILED!");
    loop {}
}

fn do_test() -> Result<()> {
    info!("🔄 [User Thread Termination] RUNNING");

    let thread_handle = handle::TEST_THREAD;

    static mut THREAD_STACK: [u8; 1024] = [0; 1024];

    let initial_pc = test_thread_entry as *const () as usize;
    let initial_sp =
        unsafe { core::ptr::addr_of_mut!(THREAD_STACK).cast::<u8>().add(1024) as usize };

    info!("🔄 ├─ Starting test thread");
    syscall::thread_start(thread_handle, initial_pc, initial_sp)?;

    info!("🔄 ├─ Waiting for test thread to terminate");
    syscall::object_wait(
        thread_handle,
        syscall::Signals::JOINABLE,
        userspace::time::Instant::MAX,
    )?;
    syscall::thread_join(thread_handle)?;

    info!("🔄 ├─ Thread joined");
    let done = THREAD_DONE.load(Ordering::SeqCst);
    if done == 1 {
        Ok(())
    } else {
        Err(pw_status::Error::Internal.into())
    }
}

#[entry]
fn main_entry() -> ! {
    let ret = do_test();

    if ret.is_err() {
        pw_log::error!("❌ ├─ FAILED");
        pw_log::error!("❌ └─ status code: {}", ret.status_code() as u32);
    } else {
        pw_log::info!("✅ └─ PASSED");
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
