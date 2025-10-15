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
use kernel::{self as _};
use kernel_config::{InterruptTableEntry, PlicConfig, PlicConfigInterface, Uart0Config};
use uart_16550_config::UartConfigInterface;

#[unsafe(no_mangle)]
#[allow(non_snake_case)]
pub extern "C" fn pw_assert_HandleFailure() -> ! {
    use kernel::Arch as _;
    Arch::panic()
}

uart_16550::declare_uarts!(Arch, UARTS, [
    UART0: Uart0Config,
]);

#[unsafe(no_mangle)]
pub static INTERRUPT_TABLE: [InterruptTableEntry; PlicConfig::INTERRUPT_TABLE_SIZE] = {
    let mut interrupt_table: [InterruptTableEntry; PlicConfig::INTERRUPT_TABLE_SIZE] =
        [None; PlicConfig::INTERRUPT_TABLE_SIZE];
    interrupt_table[Uart0Config::IRQ] = Some(interrupt_handler_uart0);
    interrupt_table
};

#[riscv_rt::entry]
fn main() -> ! {
    kernel::static_init_state!(static mut INIT_STATE: InitKernelState<Arch>);

    uart_16550::init(&UARTS);

    // SAFETY: `main` is only executed once, so we never generate more than one
    // `&mut` reference to `INIT_STATE`.
    #[allow(static_mut_refs)]
    kernel::main(Arch, unsafe { &mut INIT_STATE });
}
