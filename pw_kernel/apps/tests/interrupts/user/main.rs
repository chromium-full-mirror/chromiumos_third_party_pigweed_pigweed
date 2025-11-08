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
#![no_main]
#![no_std]

use app_interrupts_test::mapping;
use pw_status::{Error, Result, StatusCode};
use uart_16550_user::Uart;
use userspace::{entry, syscall};

fn read_expected_value(uart: &mut Uart, expected_value: u8) -> Result<()> {
    uart.wait_for_rx();

    let Some(value) = uart.read() else {
        pw_log::error!("UART read() returned no value");
        return Err(Error::Unavailable);
    };

    if value != expected_value {
        pw_log::error!(
            "UART read() wrong value {} (expected {})",
            value as u8,
            expected_value as u8
        );
        return Err(Error::Internal);
    }

    Ok(())
}

fn test_uart_loopback() -> Result<()> {
    let mut uart = Uart::new(mapping::UART0_START_ADDRESS);

    // enable lo to support writing and then reading back the result.
    uart.enable_loopback();

    // drain UART buffer
    while !uart.read().is_none() {}

    uart.write(7);
    read_expected_value(&mut uart, 7)?;

    if !uart.read().is_none() {
        pw_log::error!("Buffer not empty after read");
        return Err(Error::FailedPrecondition);
    }

    for i in 0..3u8 {
        uart.write(i);
        read_expected_value(&mut uart, i)?;
    }

    if !uart.read().is_none() {
        pw_log::error!("Buffer not empty after multiple reads");
        return Err(Error::FailedPrecondition);
    }

    Ok(())
}

#[entry]
fn entry() -> ! {
    pw_log::info!("🔄 RUNNING");
    let ret = test_uart_loopback();

    // Log that an error occurred so that the app that caused the shutdown is logged.
    if ret.is_err() {
        pw_log::error!("❌ FAILED: {}", ret.status_code() as u32);
    } else {
        pw_log::info!("✅ PASSED");
    }

    // Since this is written as a test, shut down with the return status from `main()`.
    let _ = syscall::debug_shutdown(ret);
    loop {}
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
