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

use circular_buffer::CircularBuffer;
use kernel::Kernel;
use kernel::sync::spinlock::SpinLock;
use pw_status::Result;

const LOG_UART: bool = false;

const BUFFER_SIZE: usize = 128;
pub struct Uart<K: Kernel> {
    base_address: usize,
    read_buffer: SpinLock<K, CircularBuffer<u8, BUFFER_SIZE>>,
}

impl<K: Kernel> regs::BaseAddress for Uart<K> {
    fn base_address(&self) -> usize {
        self.base_address
    }
}

impl<K: Kernel> uart_16550_regs::Uart16550BaseAddress for Uart<K> {}

impl<K: Kernel> Uart<K> {
    #[must_use]
    pub const fn new(base_address: usize) -> Uart<K> {
        Self {
            base_address,
            read_buffer: SpinLock::new(CircularBuffer::new()),
        }
    }

    pub fn enable_loopback(&self) {
        let mut mcr = uart_16550_regs::Mcr;
        mcr.write(self, mcr.read(self).with_lo(true));
    }

    pub fn read(&self, kernel: K) -> Result<Option<u8>> {
        log_if::debug_if!(LOG_UART, "uart read");
        Ok(self.read_buffer.lock(kernel).pop_front())
    }

    pub fn write(&self, value: u8) -> Result<()> {
        log_if::debug_if!(LOG_UART, "uart write: {}", value as u8);
        let lsr = uart_16550_regs::Lsr;
        while !lsr.read(self).thre() {}

        uart_16550_regs::Thr.write(self, uart_16550_regs::ThrValue(value));
        log_if::debug_if!(LOG_UART, "done");
        Ok(())
    }
}

pub fn init<K: Kernel>(uarts: &[&Uart<K>]) {
    for uart in uarts {
        let mut ier = uart_16550_regs::Ier;
        ier.write(*uart, ier.read(*uart).with_erbfi(true));
    }
}

pub fn interrupt_handler<K: Kernel>(kernel: K, uart: &Uart<K>) {
    log_if::debug_if!(
        LOG_UART,
        "uart {}: interrupt_handler",
        uart.base_address as usize
    );

    let lsr = uart_16550_regs::Lsr;
    while lsr.read(uart).dr() {
        let value = uart_16550_regs::Rbr.read(uart);
        log_if::debug_if!(LOG_UART, "data ready: {}", value.data() as u8);
        let _ = uart.read_buffer.lock(kernel).push_back(value.data());
    }
}

#[doc(hidden)]
pub mod __private {
    pub use paste::paste;
}

#[macro_export]
macro_rules! declare_uarts {
    (
        $arch:ty,
        $uarts_array:ident,
        [ $( $uart_name:ident: $config:ident),* $(,)? ]
    ) => {
        $crate::__private::paste! {
            use $crate::Uart;
            use $crate::interrupt_handler;
            $(
                pub static $uart_name: $crate::Uart<$arch> = $crate::Uart::new($config::BASE_ADDRESS);
                #[unsafe(no_mangle)]
                pub extern "C" fn [<interrupt_handler_ $uart_name:lower>]() {
                    $crate::interrupt_handler($arch, &$uart_name);
                }
            )*
            pub static $uarts_array: [&$crate::Uart<$arch>; [$(stringify!($uart_name)),*].len()] = [
                $( &$uart_name ),*
            ];
        }
    };
}
