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

#include "pw_allocator_zephyr/system_heap_allocator.h"

#include "pw_allocator/abstract_allocator.h"
#include "zephyr/kernel.h"

namespace pw::allocator_zephyr {

void* SystemHeapAllocator::DoAllocate(Layout layout) {
  return k_aligned_alloc(layout.alignment(), layout.size());
}

void SystemHeapAllocator::DoDeallocate(void* ptr) { k_free(ptr); }

void* SystemHeapAllocator::DoReallocate(void* ptr, Layout new_layout) {
  return new_layout.alignment() <= alignof(std::max_align_t)
             ? k_realloc(ptr, new_layout.size())
             : nullptr;
}

SystemHeapAllocator::SystemHeapAllocator()
    : pw::allocator::AbstractAllocator(kCapabilities) {}

SystemHeapAllocator::~SystemHeapAllocator() = default;

SystemHeapAllocator& GetSystemHeapAllocator() {
  static SystemHeapAllocator kInstance;
  return kInstance;
}

}  // namespace pw::allocator_zephyr
