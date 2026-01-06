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

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_containers/dynamic_deque.h"
#include "pw_multibuf/internal/entry.h"

namespace pw::multibuf {

// Forward declarations.
template <typename, typename>
class ChunksImpl;

class MultiBufV1Adapter;

namespace internal {

enum class ChunkContiguity {
  kKeepAll,
  kCoalesce,
};

enum class ChunkMutability {
  kMutable,
  kConst,
};

/// Type for iterating over the chunks added to a multibuf.
///
/// MultiBufs can be thought of as a sequence of "layers", where each layer
/// except the bottommost is comprised of subspans of the layer below it, and
/// the bottommost references the actual memory. This type can be used to
/// retrieve the contiguous byte spans of the topmost layer of a multibuf. It is
/// distinguished from `ByteIterator`, which iterates over individual bytes of
/// the topmost layer.
///
/// The iteration can be over the raw chunks for each layer, or it can be over
/// the contiguous non-empty chunks. The iteration can allow mutation of the
/// data in the chunk, or it can be a const iteration.
template <typename SizeType,
          ChunkContiguity kContiguity,
          ChunkMutability kMutability>
class ChunkIterator {
 private:
  using SpanType = std::conditional_t<kMutability == ChunkMutability::kConst,
                                      ConstByteSpan,
                                      ByteSpan>;
  using ByteType = typename SpanType::element_type;
  using Deque = std::conditional_t<kMutability == ChunkMutability::kConst,
                                   const DynamicDeque<Entry, SizeType>,
                                   DynamicDeque<Entry, SizeType>>;

 public:
  using size_type = SizeType;
  using difference_type = std::ptrdiff_t;
  using value_type = SpanType;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator_category = std::bidirectional_iterator_tag;

  constexpr ChunkIterator() = default;
  ~ChunkIterator() = default;
  constexpr ChunkIterator(const ChunkIterator& other) { *this = other; }
  constexpr ChunkIterator& operator=(const ChunkIterator& other);
  constexpr ChunkIterator(ChunkIterator&& other) = default;
  constexpr ChunkIterator& operator=(ChunkIterator&& other) = default;

  // Support converting non-const iterators to const_iterators.
  constexpr
  operator ChunkIterator<SizeType, kContiguity, ChunkMutability::kConst>()
      const {
    return {deque_, depth_, index_};
  }

  constexpr reference operator*() {
    PW_ASSERT(is_valid());
    return current_;
  }

  constexpr const_reference operator*() const {
    PW_ASSERT(is_valid());
    return current_;
  }

  constexpr pointer operator->() {
    PW_ASSERT(is_valid());
    return &current_;
  }

  constexpr const_pointer operator->() const {
    PW_ASSERT(is_valid());
    return &current_;
  }

  constexpr ChunkIterator& operator++();

  constexpr ChunkIterator operator++(int) {
    ChunkIterator previous(*this);
    operator++();
    return previous;
  }

  constexpr ChunkIterator& operator--();

  constexpr ChunkIterator operator--(int) {
    ChunkIterator previous(*this);
    operator--();
    return previous;
  }

  constexpr friend bool operator==(const ChunkIterator& lhs,
                                   const ChunkIterator& rhs) {
    return lhs.deque_ == rhs.deque_ && lhs.depth_ == rhs.depth_ &&
           lhs.index_ == rhs.index_;
  }

  constexpr friend bool operator!=(const ChunkIterator& lhs,
                                   const ChunkIterator& rhs) {
    return !(lhs == rhs);
  }

 private:
  // Iterators that point to something are created `Chunks` or `ConstChunks`.
  template <typename, ChunkContiguity, ChunkMutability>
  friend class ChunksImpl;

  // Allow internal conversions between iterator subtypes
  template <typename, ChunkContiguity, ChunkMutability>
  friend class ChunkIterator;

  // Byte iterators use chunk iterators to get contiguous spans.
  template <typename, ChunkMutability>
  friend class ByteIterator;

  friend MultiBufV1Adapter;

  constexpr ChunkIterator(Deque* deque, size_type depth, size_type index)
      : deque_(deque), depth_(depth), index_(index) {
    ResetCurrent();
  }

  [[nodiscard]] constexpr bool is_valid() const {
    return deque_ != nullptr && index_ < deque_->size();
  }

  constexpr ByteType* data(size_type index) const {
    return (*deque_)[index].data + (*deque_)[index + depth_ - 1].view.offset;
  }

  constexpr size_t size(size_type index) const {
    return (*deque_)[index + depth_ - 1].view.length;
  }

  constexpr void ResetCurrent();

  Deque* deque_ = nullptr;
  size_type depth_ = 0;
  size_type index_ = 0;
  SpanType current_;
};

// Template method implementations.

template <typename SizeType,
          ChunkContiguity kContiguity,
          ChunkMutability kMutability>
constexpr ChunkIterator<SizeType, kContiguity, kMutability>&
ChunkIterator<SizeType, kContiguity, kMutability>::operator=(
    const ChunkIterator& other) {
  deque_ = other.deque_;
  depth_ = other.depth_;
  index_ = other.index_;
  ResetCurrent();
  return *this;
}

template <typename SizeType,
          ChunkContiguity kContiguity,
          ChunkMutability kMutability>
constexpr ChunkIterator<SizeType, kContiguity, kMutability>&
ChunkIterator<SizeType, kContiguity, kMutability>::operator++() {
  PW_ASSERT(is_valid());

  if constexpr (kContiguity == ChunkContiguity::kKeepAll) {
    index_ += depth_;
    ResetCurrent();
    return *this;
  }

  size_t left = current_.size();
  while (left != 0) {
    left -= size(index_);
    index_ += depth_;
  }
  while (index_ < deque_->size() && size(index_) == 0) {
    index_ += depth_;
  }
  ResetCurrent();
  return *this;
}

template <typename SizeType,
          ChunkContiguity kContiguity,
          ChunkMutability kMutability>
constexpr ChunkIterator<SizeType, kContiguity, kMutability>&
ChunkIterator<SizeType, kContiguity, kMutability>::operator--() {
  PW_ASSERT(deque_ != nullptr);
  PW_ASSERT(index_ != 0);

  if constexpr (kContiguity == ChunkContiguity::kKeepAll) {
    index_ -= depth_;
    current_ = SpanType(data(index_), size(index_));
    return *this;
  }

  current_ = SpanType();
  while (index_ != 0) {
    SpanType prev(data(index_ - depth_), size(index_ - depth_));
    if (!current_.empty() && prev.data() + prev.size() != current_.data()) {
      break;
    }
    current_ = SpanType(prev.data(), prev.size() + current_.size());
    index_ -= depth_;
  }
  return *this;
}

template <typename SizeType,
          ChunkContiguity kContiguity,
          ChunkMutability kMutability>
constexpr void
ChunkIterator<SizeType, kContiguity, kMutability>::ResetCurrent() {
  if (!is_valid()) {
    current_ = SpanType();
    return;
  }

  if constexpr (kContiguity == ChunkContiguity::kKeepAll) {
    current_ = SpanType(data(index_), size(index_));
    return;
  }

  current_ = SpanType(data(index_), size(index_));
  for (size_type i = index_; i < deque_->size() - depth_; i += depth_) {
    SpanType next(data(i + depth_), size(i + depth_));
    if (current_.empty()) {
      current_ = next;
      index_ += depth_;
      continue;
    }
    if (current_.data() + current_.size() != next.data()) {
      break;
    }
    current_ = SpanType(current_.data(), current_.size() + next.size());
  }
}

}  // namespace internal
}  // namespace pw::multibuf
