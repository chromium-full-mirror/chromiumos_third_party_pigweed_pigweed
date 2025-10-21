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

#include <utility>

#include "pw_allocator/allocator.h"
#include "pw_allocator/unique_ptr.h"
#include "pw_containers/deque.h"
#include "pw_containers/internal/generic_queue.h"
#include "pw_containers/storage.h"

namespace pw {

/// @submodule{pw_containers,queues}

/// A queue implementation backed by `pw::Deque`.
///
/// This class provides a `std::queue`-like interface and stores items in a
/// caller-provided, fixed-size buffer.
///
/// @tparam T The type of elements stored in the queue.
/// @tparam SizeType How to store sizes. A smaller size type limits the maximum
///     number of items, but requires slightly less memory.
template <typename T, typename SizeType = uint16_t>
class Queue : public containers::internal::GenericQueue<Queue<T, SizeType>,
                                                        Deque<T, SizeType>> {
 public:
  /// Constructs an empty `Queue` using the provided buffer for storage.
  ///
  /// @param buffer Storage for the queue. If the buffer is not aligned as
  ///     as `value_type`, an aligned subset of the buffer will be used. If the
  ///     buffer is too small to hold at least one `value_type`, the queue's
  ///     capacity will be 0.
  explicit constexpr Queue(span<std::byte> buffer) noexcept : deque_(buffer) {}

  /// Constructs an empty `Queue` using a `pw::containers::Storage` buffer.
  /// This constructor avoids checking alignment since `containers::Storage`
  /// objects are always aligned.
  template <size_t kAlignment, size_t kSizeBytes>
  explicit constexpr Queue(
      containers::Storage<kAlignment, kSizeBytes>& storage) noexcept
      : deque_(storage) {}

  // Disable copy and move since the queue does not own the buffer.
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;

  Queue(Queue&&) = delete;
  Queue& operator=(Queue&&) = delete;

 private:
  template <typename, typename>
  friend class containers::internal::GenericQueue;

  template <typename, size_t, typename>
  friend class FixedQueue;

  // Constructor for a buffer that is known to be aligned.
  constexpr Queue(std::byte* aligned_buffer, size_t size) noexcept
      : deque_(typename Deque<T, SizeType>::Aligned(aligned_buffer, size)) {}

  void MoveItemsFrom(Queue& other) { deque_.MoveItemsFrom(other.deque_); }
  void MoveBufferFrom(Queue& other) { deque_.MoveBufferFrom(other.deque_); }
  void SwapValuesWith(Queue& other) { deque_.SwapValuesWith(other.deque_); }
  void SwapBufferWith(Queue& other) { deque_.SwapBufferWith(other.deque_); }

  Deque<T, SizeType>& deque() { return deque_; }
  const Deque<T, SizeType>& deque() const { return deque_; }

  Deque<T, SizeType> deque_;
};

/// Queue implementation equivalent to `pw::FixedDeque`.
///
/// @tparam T The type of elements stored in the queue.
/// @tparam kCapacity The maximum number of elements the queue can hold. If set
///     to `containers::kAllocatedStorage`, the queue will be dynamically
///     allocated.
/// @tparam SizeType The type used to store the size of the queue.
template <typename T,
          size_t kCapacity = containers::kAllocatedStorage,
          typename SizeType = uint16_t>
class FixedQueue final
    : private containers::internal::ArrayStorage<T, kCapacity>,
      public Queue<T, SizeType> {
 public:
  /// Constructs an empty `FixedQueue` with internal, statically allocated
  /// storage.
  constexpr FixedQueue()
      : containers::internal::ArrayStorage<T, kCapacity>(),
        Queue<T, SizeType>(this->storage_array) {}

  FixedQueue(const FixedQueue&) = delete;
  FixedQueue& operator=(const FixedQueue&) = delete;

  FixedQueue(FixedQueue&& other) noexcept : FixedQueue() {
    this->MoveItemsFrom(other);
  }

  FixedQueue& operator=(FixedQueue&& other) noexcept {
    this->clear();
    this->MoveItemsFrom(other);
    return *this;
  }

  template <size_t kOtherCapacity,
            typename = std::enable_if_t<kOtherCapacity <= kCapacity>>
  FixedQueue(FixedQueue<T, kOtherCapacity, SizeType>&& other) noexcept
      : FixedQueue() {
    this->MoveItemsFrom(other);
  }

  template <size_t kOtherCapacity,
            typename = std::enable_if_t<kOtherCapacity <= kCapacity>>
  FixedQueue& operator=(
      FixedQueue<T, kOtherCapacity, SizeType>&& other) noexcept {
    this->clear();
    this->MoveItemsFrom(other);
    return *this;
  }

  /// Swaps the contents with another queue.
  template <size_t kOtherCapacity>
  void swap(FixedQueue<T, kOtherCapacity, SizeType>& other) {
    this->SwapValuesWith(other);
  }
};

/// Specialization of `FixedQueue` using a dynamically allocated, but fixed,
/// storage buffer.
template <typename T, typename SizeType>
class FixedQueue<T, containers::kAllocatedStorage, SizeType> final
    : private containers::internal::DynamicStorage,
      public Queue<T, SizeType> {
 public:
  /// Allocates a buffer large enough to hold `capacity` items and uses it for a
  /// `FixedQueue`'s buffer. Crashes if unable to allocate space for `capacity`
  /// items.
  static FixedQueue Allocate(Allocator& allocator, const SizeType capacity) {
    FixedQueue queue = TryAllocate(allocator, capacity);
    PW_ASSERT(queue.capacity() == capacity);
    return queue;
  }

  /// Attempts to allocate a `T`-aligned buffer large enough to hold `capacity`
  /// items and use it for a `FixedQueue`'s buffer. If unable to allocate space
  /// for `capacity` items, returns a `FixedQueue` with a capacity of 0.
  static FixedQueue TryAllocate(Allocator& allocator, SizeType capacity) {
    const allocator::Layout layout = allocator::Layout::Of<T[]>(capacity);
    std::byte* array = static_cast<std::byte*>(allocator.Allocate(layout));
    return FixedQueue(allocator, array, array != nullptr ? layout.size() : 0u);
  }

  /// Constructs an empty queue that uses the provided `pw::UniquePtr` for its
  /// storage. If the `pw::UniquePtr` is not aligned as a `T`, an aligned subset
  /// of the buffer is used.
  explicit FixedQueue(pw::UniquePtr<std::byte[]>&& storage)
      : containers::internal::DynamicStorage{std::move(storage)},
        Queue<T, SizeType>(span(this->storage_unique_ptr.get(),
                                this->storage_unique_ptr.size())) {}

  FixedQueue(FixedQueue&& other) noexcept
      : containers::internal::DynamicStorage{std::move(
            other.storage_unique_ptr)},
        Queue<T, SizeType>({}) {
    this->MoveBufferFrom(other);
  }

  FixedQueue& operator=(FixedQueue&& other) noexcept {
    this->MoveBufferFrom(other);
    this->storage_unique_ptr = std::move(other.storage_unique_ptr);
    return *this;
  }

  /// Swaps the contents with another dynamically allocated queue.
  void swap(FixedQueue& other) noexcept {
    this->SwapBufferWith(other);
    std::swap(this->storage_unique_ptr, other.storage_unique_ptr);
  }

  /// Swaps the contents with a statically-allocated queue.
  template <size_t kCapacity>
  void swap(FixedQueue<T, kCapacity, SizeType>& other) {
    other.swap(*this);
  }

 private:
  FixedQueue(Allocator& allocator, std::byte* aligned_buffer, size_t size_bytes)
      : containers::internal::DynamicStorage{UniquePtr<std::byte[]>(
            aligned_buffer, size_bytes, allocator)},
        Queue<T, SizeType>(storage_unique_ptr.get(), size_bytes) {}
};

}  // namespace pw
