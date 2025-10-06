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
#include <cstdint>
#include <cstring>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_containers/internal/var_len_entry.h"
#include "pw_containers/internal/var_len_entry_queue_iterator.h"
#include "pw_containers/internal/wrap.h"
#include "pw_span/span.h"
#include "pw_varint/varint.h"

namespace pw::containers::internal {

class GenericVarLenEntryQueueBase {
 protected:
  /// Returns the number of variable-length entries in the buffer.
  static constexpr size_t size(ConstByteSpan bytes,
                               uint32_t head,
                               uint32_t tail);

  /// Returns the combined size in bytes of all entries in the queue, excluding
  /// metadata. This is at most O(n) in the number of entries in the queue.
  static constexpr size_t size_bytes(ConstByteSpan bytes,
                                     uint32_t head,
                                     uint32_t tail);

  static constexpr bool Push(ConstByteSpan data,
                             bool overwrite,
                             ByteSpan bytes,
                             uint32_t& head,
                             uint32_t& tail);

  /// Copies data to the buffer, wrapping around the end if needed.
  static constexpr void CopyAndWrap(ConstByteSpan data,
                                    ByteSpan bytes,
                                    uint32_t& tail);

  /// Removes an entry from the head of the queue. Must not be empty.
  static constexpr size_t PopNonEmpty(ConstByteSpan bytes, uint32_t& head);
};

template <typename Derived, typename T>
class GenericVarLenEntryQueue : public GenericVarLenEntryQueueBase {
 private:
  using Base = GenericVarLenEntryQueueBase;

 public:
  using value_type = containers::internal::VarLenEntry<T>;
  using const_value_type = containers::internal::VarLenEntry<const T>;
  using iterator = VarLenEntryQueueIterator<T>;
  using const_iterator = VarLenEntryQueueIterator<const T>;

  /// Returns an iterator to the start of the `queue`.
  constexpr iterator begin() { return iterator(GetBuffer(), GetHead()); }
  constexpr const_iterator begin() const {
    return const_iterator(GetBuffer(), GetHead());
  }
  constexpr const_iterator cbegin() const { return begin(); }

  /// @copydoc pw_InlineVarLenEntryQueue_End
  constexpr iterator end() { return iterator(GetBuffer(), GetTail()); }
  constexpr const_iterator end() const {
    return const_iterator(GetBuffer(), GetTail());
  }
  constexpr const_iterator cend() const { return end(); }

  /// Returns true if the queue is empty, false if it has at least one entry.
  [[nodiscard]] constexpr bool empty() const { return GetHead() == GetTail(); }

  /// Returns the number of variable-length entries in the queue. This is at
  /// most O(n) in the number of entries in the queue.
  constexpr size_t size() const {
    return Base::size(GetBytes(), GetHead(), GetTail());
  }

  /// Returns the maximum number of entries in the queue. This is only
  /// attainable if all entries are empty.
  constexpr size_t max_size() const { return GetBuffer().size() - 1; }

  /// Returns the combined size in bytes of all entries in the queue, excluding
  /// metadata. This is at most O(n) in the number of entries in the queue.
  constexpr size_t size_bytes() const {
    return Base::size_bytes(GetBytes(), GetHead(), GetTail());
  }

  /// Returns the maximum number of bytes that can be stored in the queue.
  /// This is largest possible value of `size_bytes()`, and the size of the
  /// largest single entry that can be stored in this queue. Attempting to store
  /// a larger entry is invalid and results in a crash.
  constexpr size_t max_size_bytes() const {
    return max_size() - varint::EncodedSize(max_size());
  }

  /// Returns the first entry in the queue.
  /// @pre The queue must NOT empty (`empty()` is false).
  constexpr value_type front() { return *begin(); }
  constexpr const_value_type front() const { return *cbegin(); }

  /// Appends an entry to the end of the queue.
  ///
  /// @pre The entry MUST NOT be larger than `max_size_bytes()`.
  /// @pre There must be sufficient space in the queue for this entry.
  constexpr void push(span<const T> data) { PW_ASSERT(try_push(data)); }

  /// Appends an entry to the end of the queue, but only if there is sufficient
  /// space for it.
  ///
  /// @returns true if the data was added to the queue; false if it did not fit
  /// @pre The entry MUST NOT be larger than `max_size_bytes()`.
  [[nodiscard]] constexpr bool try_push(span<const T> data) {
    return Push(data, /* overwrite: */ false);
  }

  /// Appends an entry to the end of the queue, removing entries with `Pop`
  /// as necessary to make room. Calling this function drops old entries to make
  /// room for new; call `try_push()` to drop new entries instead.
  ///
  /// @pre The entry MUST NOT be larger than `max_size_bytes()`.
  constexpr void push_overwrite(span<const T> data) {
    Push(data, /* overwrite: */ true);
  }

  /// Removes the first entry from queue.
  ///
  /// @pre The queue MUST have at least one entry.
  constexpr void pop() {
    PW_ASSERT(!empty());
    uint32_t head = GetHead();
    PopNonEmpty(GetBytes(), head);
    SetHead(head);
  }

  /// Empties the queue.
  constexpr void clear() {
    SetHead(0);
    SetTail(0);
  }

 protected:
  constexpr GenericVarLenEntryQueue() = default;

  // The queue metadata and data is part of the derived type, and can be
  // accessed and modified by downcasting to that type.
  constexpr uint32_t GetHead() const { return derived().head(); }
  constexpr void SetHead(uint32_t head) { return derived().set_head(head); }
  constexpr uint32_t GetTail() const { return derived().tail(); }
  constexpr void SetTail(uint32_t tail) { return derived().set_tail(tail); }
  constexpr span<T> GetBuffer() { return derived().buffer(); }
  constexpr span<const T> GetBuffer() const { return derived().buffer(); }

  constexpr ByteSpan GetBytes() {
    if constexpr (std::is_same_v<T, std::byte>) {
      return GetBuffer();
    } else {
      return as_writable_bytes(GetBuffer());
    }
  }

  constexpr ConstByteSpan GetBytes() const {
    if constexpr (std::is_same_v<T, std::byte>) {
      return GetBuffer();
    } else {
      return as_bytes(GetBuffer());
    }
  }

 private:
  static_assert(std::is_integral_v<T> || std::is_same_v<T, std::byte>);
  static_assert(sizeof(T) == sizeof(std::byte));

  constexpr Derived& derived() { return *static_cast<Derived*>(this); }
  constexpr const Derived& derived() const {
    return *static_cast<const Derived*>(this);
  }

  constexpr bool Push(span<const T> data, bool overwrite) {
    uint32_t head = GetHead();
    uint32_t tail = GetTail();
    bool result = Base::Push(as_bytes(data), overwrite, GetBytes(), head, tail);
    SetHead(head);
    SetTail(tail);
    return result;
  }
};

////////////////////////////////////////////////////////////////////////////////
// Constexpr method implementations.

constexpr size_t GenericVarLenEntryQueueBase::size(ConstByteSpan bytes,
                                                   uint32_t head,
                                                   uint32_t tail) {
  size_t num_entries = 0;
  uint32_t bytes_size = static_cast<uint32_t>(bytes.size());
  while (head != tail) {
    uint32_t entry_size = ReadVarLenEntryEncodedSize(bytes, head);
    IncrementWithWrap(head, entry_size, bytes_size);
    ++num_entries;
  }
  return num_entries;
}

constexpr size_t GenericVarLenEntryQueueBase::size_bytes(ConstByteSpan bytes,
                                                         uint32_t head,
                                                         uint32_t tail) {
  size_t total_entry_size_bytes = 0;
  uint32_t bytes_size = static_cast<uint32_t>(bytes.size());
  while (head != tail) {
    auto [prefix_size, data_size] = ReadVarLenEntrySize(bytes, head);
    IncrementWithWrap(head, prefix_size + data_size, bytes_size);
    total_entry_size_bytes += data_size;
  }
  return total_entry_size_bytes;
}

constexpr bool GenericVarLenEntryQueueBase::Push(ConstByteSpan data,
                                                 bool overwrite,
                                                 ByteSpan bytes,
                                                 uint32_t& head,
                                                 uint32_t& tail) {
  // Encode the data size in a temporary buffer.
  std::byte tmp[varint::kMaxVarint32SizeBytes] = {};
  auto size_u32 = static_cast<uint32_t>(data.size());
  size_t encoded = varint::Encode(size_u32, ByteSpan(tmp, sizeof(tmp)));
  ConstByteSpan prefix(tmp, encoded);

  // Calculate how much space is needed.
  size_t needed_bytes = prefix.size() + data.size();
  PW_ASSERT(needed_bytes < bytes.size());

  // Calculate how much space is available.
  uint32_t available_bytes = head - tail - 1;
  if (head <= tail) {
    available_bytes += static_cast<uint32_t>(bytes.size());
  }

  // Check if sufficient space is available, or can be made available.
  if (overwrite) {
    while (needed_bytes > available_bytes) {
      available_bytes += PopNonEmpty(bytes, head);
    }
  } else {
    if (needed_bytes > available_bytes) {
      return false;
    }
  }

  // Write out the prefix and data.
  CopyAndWrap(prefix, bytes, tail);
  CopyAndWrap(data, bytes, tail);
  return true;
}

// Private methods

constexpr void GenericVarLenEntryQueueBase::CopyAndWrap(ConstByteSpan data,
                                                        ByteSpan bytes,
                                                        uint32_t& tail) {
  if (data.empty()) {
    return;
  }
  // Copy the new data in one or two chunks. The first chunk is copied to the
  // byte after the tail, the second from the beginning of the buffer. Both may
  // be zero bytes.
  uint32_t data_size = static_cast<uint32_t>(data.size());
  uint32_t bytes_size = static_cast<uint32_t>(bytes.size());
  uint32_t first_chunk = bytes_size - tail;
  if (first_chunk >= data_size) {
    first_chunk = data_size;
  } else {  // Copy 2nd chunk from the beginning of the buffer (may be 0 bytes).
    pw::copy(data.begin() + first_chunk, data.end(), bytes.begin());
  }
  pw::copy(data.begin(), data.begin() + first_chunk, bytes.begin() + tail);
  IncrementWithWrap(tail, data_size, bytes_size);
}

constexpr size_t GenericVarLenEntryQueueBase::PopNonEmpty(ConstByteSpan bytes,
                                                          uint32_t& head) {
  uint32_t entry_size = ReadVarLenEntryEncodedSize(bytes, head);
  IncrementWithWrap(head, entry_size, static_cast<uint32_t>(bytes.size()));
  return entry_size;
}

}  // namespace pw::containers::internal
