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

#include "pw_allocator/unique_ptr.h"
#include "pw_containers/algorithm.h"

namespace pw::containers {

/// @submodule{pw_containers,utilities}

/// Declares aligned storage for `kCount` items of type `T` in a `std::byte`
/// array.
///
/// `Storage` provides aligned external storage for containers such as
/// `pw::Deque`, avoiding the need for alignment checks.
///
/// @tparam T The type of items to store.
/// @tparam kCount The number of items for which to reserve space.
template <typename T, size_t kCount = 1>
class Storage {
 public:
  using value_type = std::byte;
  using size_type = size_t;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  constexpr Storage() : buffer_{} {}

  Storage(const Storage&) = delete;
  Storage& operator=(const Storage&) = delete;

  Storage(Storage&&) = delete;
  Storage& operator=(Storage&&) = delete;

  constexpr pointer data() { return buffer_.data(); }
  constexpr const_pointer data() const { return buffer_.data(); }

  /// The size of the storage in bytes.
  constexpr size_type size() const { return buffer_.size(); }

  [[nodiscard]] constexpr bool empty() const { return buffer_.empty(); }

  constexpr void fill(std::byte value) {
    pw::fill_n(buffer_.data(), buffer_.size(), value);
  }

 private:
  alignas(T) std::array<std::byte, sizeof(T) * kCount> buffer_;
};

/// Special reserved capacity for containers that own a storage buffer to
/// indicate that the buffer is dynamically allocated.
inline constexpr size_t kAllocatedStorage = static_cast<size_t>(-1);

namespace internal {

// Storage wrappers to be used as bases when implementing a container that
// combines a non-owning container with its buffer. These types have to be the
// first base so the storage outlives the container using it.
template <typename T, size_t kCapacity>
struct ArrayStorage {
  Storage<T, kCapacity> storage_array;
};

struct DynamicStorage {
  UniquePtr<std::byte[]> storage_unique_ptr;
};

}  // namespace internal
}  // namespace pw::containers
