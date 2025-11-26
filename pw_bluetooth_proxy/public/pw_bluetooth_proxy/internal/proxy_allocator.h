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
#pragma once

#include <array>

#include "pw_allocator/allocator.h"
#include "pw_allocator/best_fit.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_assert/assert.h"
#include "pw_bluetooth_proxy/config.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy::internal {

/// Allocator wrapper type that uses an external allocator or provides an
/// internal allocator when PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE is
/// nonzero.
///
/// @tparam   AllocatorType   Type of the internal allocator, if applicable.
/// @tparam   kCapacity       Sice of the memory region provided to the internal
///                           allocator, if applicable.
///
/// TODO: https://pwbug.dev/369849508 - Fully migrate to client-provided
/// allocator and remove internal allocator.
template <typename AllocatorType, size_t kCapacity>
class BasicProxyAllocator : public Allocator {
 public:
  /// Constructor.
  ///
  /// `allocator` may be null if PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE is
  /// nonzero, in which case the internal allocator will be used.
  constexpr explicit BasicProxyAllocator(Allocator* allocator) {
    SetImpl(allocator);
  }

 private:
  // Forward all calls to the actual allocator.
  void* DoAllocate(Layout layout) override { return impl_->Allocate(layout); }
  void DoDeallocate(void* ptr) override { return impl_->Deallocate(ptr); }
  bool DoResize(void* ptr, size_t new_size) override {
    return impl_->Resize(ptr, new_size);
  }
  Result<Layout> DoGetInfo(InfoType info_type, const void* ptr) const override {
    return Deallocator::GetInfo(*impl_, info_type, ptr);
  }
  void* DoReallocate(void* ptr, Layout new_layout) override {
    return impl_->Reallocate(ptr, new_layout);
  }
  size_t DoGetAllocated() const override { return impl_->GetAllocated(); }

  Allocator* impl_ = nullptr;

#if PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE != 0
  std::array<std::byte, PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE> storage_;
  AllocatorType internal_{storage_};
  allocator::SynchronizedAllocator<sync::Mutex> sync_{internal_};

  constexpr void SetImpl(Allocator* allocator) {
    impl_ = allocator ? allocator : &sync_;
  }

#else  // PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE == 0

  constexpr void SetImpl(Allocator* allocator) {
    PW_ASSERT(allocator != nullptr);
    impl_ = allocator;
  }

#endif  // PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE
};

using ProxyAllocator =
    BasicProxyAllocator<allocator::BestFitAllocator<>,
                        PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE>;

}  // namespace pw::bluetooth::proxy::internal
