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

use crate::scheduler::{PreemptDisableGuard, RescheduleReason};
use crate::{Kernel, scheduler};

/// This trait provides a generic interface to an architecture's interrupt
/// controller, such as a RISC-V PLIC or an Arm NVIC.
pub trait InterruptController {
    type InterruptHandler: Copy + Sync + Send + 'static;

    /// Called early in the kernel::main() function.
    fn early_init(&self) {}

    /// Enable a specific interrupt by its IRQ number.
    fn enable_interrupt(irq: u32);

    /// Disable a specific interrupt by its IRQ number.
    fn disable_interrupt(irq: u32);

    /// Userspace handling of the interrupt is complete.  Called
    /// from [`syscall_defs::interrupt_ack(object_handle: u32, signal_mask: Signals)`]
    fn userspace_interrupt_ack(irq: u32);

    /// Userspace interrupt handlers call [`userspace_interrupt_handler_enter(kernel: K, irq: u32)`]
    /// to mask the interrupt prior to the interrupt object being signalled.  The interrupt
    /// is unmasked from userspace by [`userspace_interrupt_ack(irq: u32)`].
    fn userspace_interrupt_handler_enter<K: Kernel>(kernel: K, irq: u32);

    /// Userspace interrupt handler is complete.
    ///
    /// This consumes the `InterruptGuard` by value. When it goes out of scope,
    /// its `Drop` implementation will handle thread termination checks and
    /// deferred rescheduling.
    fn userspace_interrupt_handler_exit<K: Kernel>(kernel: K, irq: u32, guard: InterruptGuard<K>);

    /// Kernel interrupt handler has started.
    fn kernel_interrupt_handler_enter<K: Kernel>(kernel: K, irq: u32);

    /// Kernel interrupt handler is complete.
    ///
    /// This consumes the `InterruptGuard` by value. When it goes out of scope,
    /// its `Drop` implementation will handle thread termination checks and
    /// deferred rescheduling.
    fn kernel_interrupt_handler_exit<K: Kernel>(kernel: K, irq: u32, guard: InterruptGuard<K>);

    /// Globally enable interrupts.
    fn enable_interrupts();

    /// Globally disable interrupts.
    fn disable_interrupts();

    /// Returns `true` if interrupts are globally enabled.
    fn interrupts_enabled() -> bool;

    /// Trigger an interrupt by IRQ.  This may not be supported
    /// on all interrupt controllers.
    fn trigger_interrupt(irq: u32);
}

pub struct InterruptGuard<K: Kernel> {
    kernel: K,
    preempt_guard: Option<PreemptDisableGuard<K>>,
    from_userspace: bool,
    reason: RescheduleReason,
}

impl<K: Kernel> InterruptGuard<K> {
    pub fn new(kernel: K, from_userspace: bool) -> Self {
        Self {
            kernel,
            preempt_guard: Some(crate::scheduler::PreemptDisableGuard::new(kernel)),
            from_userspace,
            reason: RescheduleReason::Preempted,
        }
    }

    pub fn set_reschedule_reason(&mut self, reason: RescheduleReason) {
        self.reason = reason;
    }
}

impl<K: Kernel> Drop for InterruptGuard<K> {
    fn drop(&mut self) {
        if let Some(guard) = self.preempt_guard.take() {
            drop(guard);
        }
        handle_thread_termination(self.kernel, self.from_userspace);
        scheduler::try_deferred_reschedule(self.kernel, self.reason);
    }
}

/// Handle conditional termination of interrupted thread
///
/// Checks if the current thread is terminating and was interrupted from userspace.
/// If so, exit the thread.
///
/// This should be called when returning to interrupt or exception.
pub fn handle_thread_termination<K: Kernel>(kernel: K, from_userspace: bool) {
    if from_userspace
        && kernel
            .get_scheduler()
            .lock(kernel)
            .current_thread()
            .is_terminating()
    {
        let sched = kernel.get_scheduler().lock(kernel);
        sched.thread_exit(kernel);
    }
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

pub type InterruptTableEntry<I> = Option<<I as InterruptController>::InterruptHandler>;
