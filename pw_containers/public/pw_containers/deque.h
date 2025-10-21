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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

#include "lib/stdcompat/utility.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/unique_ptr.h"
#include "pw_assert/assert.h"
#include "pw_containers/internal/count_and_capacity.h"
#include "pw_containers/internal/generic_deque.h"
#include "pw_containers/storage.h"
#include "pw_polyfill/language_feature_macros.h"
#include "pw_span/span.h"

namespace pw {

/// @submodule{pw_containers,queues}

/// Double-ended queue, similar to `std::deque`, but uses a fixed storage
/// buffer. The buffer is managed externally and may be statically or
/// dynamically allocated.
///
/// `Deque` is not movable. This avoids confusion about which buffer the deque
/// is using, since the buffer is unowned. `FixedDeque`, which derives from
/// `Deque`, owns its buffer and supports moving.
///
/// To instantiate a `Deque` with an integrated buffer, see `FixedDeque`.
/// `FixedDeque` supports either static or dynamic allocation of its buffer.
///
/// @tparam T What is stored in the deque.
/// @tparam SizeType How to store sizes. A smaller size type limits the maximum
///     number of items, but requires slightly less memory.
template <typename T, typename SizeType = uint16_t>
class Deque : public containers::internal::GenericDeque<
                  Deque<T, SizeType>,
                  T,
                  containers::internal::CountAndCapacity<SizeType>> {
 private:
  using Base = containers::internal::GenericDeque<
      Deque<T, SizeType>,
      T,
      containers::internal::CountAndCapacity<SizeType>>;

 public:
  using typename Base::const_iterator;
  using typename Base::const_pointer;
  using typename Base::const_reference;
  using typename Base::difference_type;
  using typename Base::iterator;
  using typename Base::pointer;
  using typename Base::reference;
  using typename Base::size_type;
  using typename Base::value_type;

  /// Constructs an empty `Deque` using the provided buffer for storage.
  ///
  /// @param buffer Storage for the deque. If the buffer is not aligned as
  ///     as `value_type`, an aligned subset of the buffer will be used. If the
  ///     buffer is too small to hold at least one `value_type`, the queue's
  ///     capacity will be 0.
  explicit constexpr Deque(span<std::byte> buffer) noexcept
      : Deque(Aligned::Align(buffer)) {}

  /// Constructs an empty `Deque` using a `pw::containers::Storage` buffer.
  /// This constructor avoids run time alignment checks.
  template <size_t kAlignment, size_t kSizeBytes>
  explicit constexpr Deque(
      containers::Storage<kAlignment, kSizeBytes>& buffer) noexcept
      : Deque(Aligned(buffer.data(), buffer.size())) {
    static_assert(kAlignment >= alignof(value_type));
  }

  /// Copying is not supported since it can fail.
  Deque(const Deque&) = delete;
  Deque& operator=(const Deque&) = delete;

  /// Move is not supported to avoid confusion about deque/buffer pairings.
  Deque(Deque&&) = delete;
  Deque& operator=(Deque&&) = delete;

  PW_CONSTEXPR_CPP20 ~Deque() { Base::DestroyAll(); }

  constexpr size_type max_size() const noexcept { return Base::capacity(); }

 private:
  friend Base;

  template <typename, typename>
  friend class Queue;

  template <typename, size_t, typename>
  friend class FixedDeque;

  static constexpr bool kFixedCapacity = true;  // uses a fixed buffer

  // Span that is known to be aligned as T.
  class Aligned {
   public:
    // Construct from an UNALIGNED span. Checks alignment if necessary.
    static constexpr Aligned Align(span<std::byte> unaligned_buffer) {
      Aligned buffer(unaligned_buffer.data(), unaligned_buffer.size());

      if constexpr (alignof(value_type) > alignof(std::byte)) {
        void* data_void = buffer.data_;
        buffer.data_ = static_cast<std::byte*>(std::align(
            alignof(value_type), sizeof(value_type), data_void, buffer.size_));
        if (buffer.data_ == nullptr) {
          buffer.size_ = 0;  // cannot fit any items
        }
      }

      return buffer;
    }

    constexpr Aligned(std::byte* aligned_data, size_t size_bytes)
        : data_(aligned_data), size_(size_bytes) {}

    constexpr std::byte* data() const { return data_; }
    constexpr size_t capacity() const { return size_ / sizeof(value_type); }

   private:
    std::byte* data_;
    size_t size_;
  };

  explicit constexpr Deque(Aligned buffer) noexcept
      : Base(static_cast<size_type>(buffer.capacity())),
        buffer_(buffer.data()) {}

  constexpr void MoveBufferFrom(Deque& other) noexcept {
    Base::DestroyAll();
    buffer_ = cpp20::exchange(other.buffer_, nullptr);
    Base::MoveAssignIndices(other);
  }

  void MoveItemsFrom(Deque& other) {
    this->assign(std::make_move_iterator(other.begin()),
                 std::make_move_iterator(other.end()));
    other.clear();
  }

  void SwapBufferWith(Deque& other) noexcept {
    Base::SwapIndices(other);
    std::swap(buffer_, other.buffer_);
  }

  void SwapValuesWith(Deque& other);

  pointer data() { return std::launder(reinterpret_cast<pointer>(buffer_)); }
  const_pointer data() const {
    return std::launder(reinterpret_cast<const_pointer>(buffer_));
  }

  // Pointer to the buffer used for the deque. This may differ from the pointer
  // passed into the constructor if it had to be shifted for alignment.
  std::byte* buffer_;
};

/// `FixedDeque` is a `Deque` that owns its statically allocated buffer.
///
/// This `FixedDeque` statically allocates its storage buffer internally.
/// Similar to `InlineDeque`, the capacity is specified as a template parameter.
/// Unlike `InlineDeque`, `FixedDeque` is usable as a generic `Deque`.
///
/// If the capacity is omitted (or set to `containers::kAllocatedStorage`), the
/// @ref FixedDeque<T, containers::kAllocatedStorage, SizeType>
/// "dynamic specialization" is used. Whether statically or dynamically
/// allocated, the capacity never changes.
template <typename T,
          size_t kInlineCapacity = containers::kAllocatedStorage,
          typename SizeType = uint16_t>
class FixedDeque final
    : private containers::internal::ArrayStorage<T, kInlineCapacity>,
      public Deque<T, SizeType> {
 public:
  /// Constructs an empty `FixedDeque` with internal, statically allocated
  /// storage.
  constexpr FixedDeque()
      : containers::internal::ArrayStorage<T, kInlineCapacity>{},
        Deque<T, SizeType>(this->storage_array) {}

  FixedDeque(FixedDeque&& other) noexcept : FixedDeque() {
    this->MoveItemsFrom(other);
  }

  FixedDeque& operator=(FixedDeque&& other) noexcept {
    this->clear();
    this->MoveItemsFrom(other);
    return *this;
  }

  /// `FixedDeque` supports moves that are guaranteed to succeed (moving into
  /// equal or greater capacity or between dynamically allocated deques).
  template <size_t kOtherCapacity,
            typename = std::enable_if_t<kOtherCapacity <= kInlineCapacity>>
  FixedDeque(FixedDeque<T, kOtherCapacity>&& other) noexcept : FixedDeque() {
    this->MoveItemsFrom(other);
  }

  template <size_t kOtherCapacity,
            typename = std::enable_if_t<kOtherCapacity <= kInlineCapacity>>
  FixedDeque& operator=(FixedDeque<T, kOtherCapacity>&& other) noexcept {
    this->clear();
    this->MoveItemsFrom(other);
    return *this;
  }

  /// Swaps the contents of this `FixedDeque` with another, in O(n) time.
  /// Crashes if one deque's size is greater than the other's capacity.
  template <size_t kOtherCapacity>
  void swap(FixedDeque<T, kOtherCapacity, SizeType>& other) {
    this->SwapValuesWith(other);
  }

 private:
  static_assert(kInlineCapacity <= std::numeric_limits<SizeType>::max(),
                "The capacity is too large for the size; use a large SizeType");
};

/// @ref FixedDeque<T, containers::kAllocatedStorage, SizeType>
/// "FixedDeque<T>" is a `Deque` that owns its dynamically allocated buffer. The
/// buffer is dynamically allocated during or before construction, but no
/// further allocations occur. The buffer never grows or shrinks.
///
/// To statically allocate the buffer, specify the capacity in the `FixedDeque`
/// declaration.
template <typename T, typename SizeType>
class FixedDeque<T, containers::kAllocatedStorage, SizeType> final
    : private containers::internal::DynamicStorage,
      public Deque<T, SizeType> {
 public:
  /// Allocates a buffer large enough to hold `capacity` items and uses it for a
  /// `FixedDeque`'s buffer. Crashes if unable to allocate space for `capacity`
  /// items.
  static FixedDeque Allocate(Allocator& allocator, const SizeType capacity) {
    FixedDeque deque = TryAllocate(allocator, capacity);
    PW_ASSERT(deque.capacity() == capacity);
    return deque;
  }

  /// Attempts to allocate a `T`-aligned buffer large enough to hold `capacity`
  /// items and use it for a `FixedDeque`'s buffer. If unable to allocate space
  /// for `capacity` items, returns a `FixedDeque` with a capacity of 0.
  static FixedDeque TryAllocate(Allocator& allocator, SizeType capacity) {
    const allocator::Layout layout = allocator::Layout::Of<T[]>(capacity);
    std::byte* array = static_cast<std::byte*>(allocator.Allocate(layout));
    return FixedDeque(allocator, array, array != nullptr ? layout.size() : 0u);
  }

  /// Constructs an empty `FixedDeque` that uses the provided `pw::UniquePtr`
  /// for its storage. If the `pw::UniquePtr` is not aligned as a `T`, an
  /// aligned subset of the buffer is used.
  explicit FixedDeque(UniquePtr<std::byte[]>&& storage) noexcept
      : containers::internal::DynamicStorage{std::move(storage)},
        Deque<T, SizeType>(
            span(storage_unique_ptr.get(), storage_unique_ptr.size())) {}

  /// Move construction/assignment from other `FixedDeque`s with dynamically
  /// allocated buffers is supported and infallible. To move items from a
  /// statically allocated `FixedDeque`, use `assign()` with
  /// `std::make_move_iterator`, or move items individually.
  FixedDeque(FixedDeque&& other) noexcept
      : containers::internal::DynamicStorage{std::move(
            other.storage_unique_ptr)},
        Deque<T, SizeType>({}) {
    this->MoveBufferFrom(other);
  }

  FixedDeque& operator=(FixedDeque&& other) noexcept {
    this->MoveBufferFrom(other);
    return *this;
  }

  /// Swaps this `FixedDeque` with another dynamically allocated `FixedDeque` in
  /// O(1) time. No allocations occur, and individual items are not moved.
  void swap(FixedDeque& other) noexcept {
    this->SwapBufferWith(other);
    std::swap(storage_unique_ptr, other.storage_unique_ptr);
  }

  /// Swaps the contents of this `FixedDeque` with a statically allocated one in
  /// O(n) time. No allocations occur, but items are moved individually. Crashes
  /// if the size of one deque exceeds the capacity of the other.
  template <size_t kInlineCapacity>
  void swap(FixedDeque<T, kInlineCapacity, SizeType>& other) {
    other.swap(*this);  // Use the swap impl for statically allocated deques.
  }

 private:
  FixedDeque(Allocator& allocator, std::byte* aligned_buffer, size_t size_bytes)
      : containers::internal::DynamicStorage{UniquePtr<std::byte[]>(
            aligned_buffer, size_bytes, allocator)},
        Deque<T, SizeType>(typename Deque<T, SizeType>::Aligned(
            storage_unique_ptr.get(), size_bytes)) {}
};

template <typename T, typename SizeType>
void Deque<T, SizeType>::SwapValuesWith(Deque& other) {
  Deque* smaller;
  Deque* larger;

  if (this->size() < other.size()) {
    smaller = this;
    larger = &other;
  } else {
    smaller = &other;
    larger = this;
  }

  const SizeType smaller_size = smaller->size();
  for (auto it =
           std::swap_ranges(smaller->begin(), smaller->end(), larger->begin());
       it != larger->end();
       ++it) {
    smaller->push_back(std::move(*it));
  }

  while (larger->size() > smaller_size) {
    larger->pop_back();
  }
}

}  // namespace pw
