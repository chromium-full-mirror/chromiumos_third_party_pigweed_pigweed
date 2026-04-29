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

#include "pw_allocator_zephyr/synchronized_heap_allocator.h"

#include "zephyr/kernel.h"

namespace pw::allocator_zephyr {

SynchronizedHeapAllocator::SynchronizedHeapAllocator(pw::span<std::byte> buffer)
    : pw::allocator::AbstractAllocator(kCapabilities) {
  k_heap_init(&heap_, buffer.data(), buffer.size());
}

void* SynchronizedHeapAllocator::DoAllocate(pw::allocator::Layout layout) {
  constexpr k_timeout_t timeout = K_NO_WAIT;
  return k_heap_aligned_alloc(
      &heap_, layout.alignment(), layout.size(), timeout);
}

void SynchronizedHeapAllocator::DoDeallocate(void* ptr) {
  k_heap_free(&heap_, ptr);
}

void* SynchronizedHeapAllocator::DoReallocate(
    void* ptr, pw::allocator::Layout new_layout) {
  constexpr k_timeout_t timeout = K_NO_WAIT;
  return new_layout.alignment() <= alignof(std::max_align_t)
             ? k_heap_realloc(&heap_, ptr, new_layout.size(), timeout)
             : nullptr;
}

}  // namespace pw::allocator_zephyr
