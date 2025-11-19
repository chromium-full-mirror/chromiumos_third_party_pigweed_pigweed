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

use core::cell::UnsafeCell;

/// This trait provides a generic interface to an architecture's interrupt
/// controller, such as a RISC-V PLIC or an Arm NVIC.
pub trait InterruptController {
    /// Called early in the kernel::main() function.
    fn early_init(&self) {}

    /// Enable a specific interrupt by its IRQ number.
    fn enable_interrupt(&self, irq: u32);

    /// Disable a specific interrupt by its IRQ number.
    fn disable_interrupt(&self, irq: u32);

    /// Handling of the interrupt is complete.
    fn interrupt_ack(irq: u32);

    /// Globally enable interrupts.
    fn enable_interrupts();

    /// Globally disable interrupts.
    fn disable_interrupts();

    /// Returns `true` if interrupts are globally enabled.
    fn interrupts_enabled() -> bool;
}

/// StaticContext provides a context with Send & Sync for
/// use with static instances of ForeignRc.
pub struct StaticContext<T> {
    inner: UnsafeCell<Option<T>>,
}

impl<T: Clone> StaticContext<T> {
    pub const fn new() -> Self {
        Self {
            inner: UnsafeCell::new(None),
        }
    }

    /// # Safety
    /// Users of [`StaticContext::set()`] must ensure that
    /// their calls into them are not done concurrently.
    pub unsafe fn set(&self, val: T) {
        unsafe { *self.inner.get() = Some(val) };
    }

    /// # Safety
    /// Users of [`StaticContext::get()`] must ensure that
    /// their calls into them are not done concurrently.
    pub unsafe fn get(&self) -> Option<T> {
        unsafe { (*self.inner.get()).clone() }
    }
}

unsafe impl<T> Sync for StaticContext<T> {}
unsafe impl<T> Send for StaticContext<T> {}

pub type InterruptHandler = extern "C" fn();
pub type InterruptTableEntry = Option<InterruptHandler>;
