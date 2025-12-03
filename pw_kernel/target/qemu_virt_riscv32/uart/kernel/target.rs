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
#![no_std]
#![no_main]

use arch_riscv::Arch;
use kernel_config::Uart0Config;
use pw_status::Result;
use riscv_semihosting::debug::{EXIT_FAILURE, EXIT_SUCCESS, exit};
use target_common::{TargetInterface, declare_target};
use uart_16550_config::UartConfigInterface;
use {codegen as _, console_backend as _, entry as _};

pub struct Target {}
struct TargetUart {}

uart_16550_kernel::declare_uarts!(Arch, UARTS, [
    UART0: Uart0Config,
]);

impl test_uart::TestUart for TargetUart {
    fn enable_loopback() {
        UART0.enable_loopback()
    }

    fn read() -> Result<Option<u8>> {
        UART0.read(Arch)
    }

    fn write(byte: u8) -> Result<()> {
        UART0.write(byte)
    }
}

impl TargetInterface for Target {
    const NAME: &'static str = "QEMU-VIRT-RISCV Kernel UART";

    fn main() -> ! {
        uart_16550_kernel::init(Arch, &UARTS);

        let exit_status = match test_uart::main::<Arch, TargetUart>(Arch) {
            Ok(()) => EXIT_SUCCESS,
            Err(_e) => EXIT_FAILURE,
        };
        exit(exit_status);
        #[expect(clippy::empty_loop)]
        loop {}
    }
}

declare_target!(Target);
