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

#include <cstring>
#include <utility>

#include "public/pw_multibuf/multibuf_v2.h"
#include "pw_assert/check.h"
#include "pw_containers/algorithm.h"
#include "pw_multibuf/internal/byte_iterator.h"
#include "pw_multibuf/multibuf_v2.h"
#include "pw_status/try.h"

namespace pw::multibuf::internal {

GenericMultiBuf& GenericMultiBuf::operator=(GenericMultiBuf&& other) {
  deque_ = std::move(other.deque_);
  entries_per_chunk_ =
      std::exchange(other.entries_per_chunk_, Entry::kMinEntriesPerChunk);
  CopyMemoryContext(other);
  other.ClearMemoryContext();
  observer_ = std::exchange(other.observer_, nullptr);
  return *this;
}

bool GenericMultiBuf::TryReserveChunks(size_t num_chunks) {
  return TryReserveLayers(NumLayers(), num_chunks);
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos,
                                          const GenericMultiBuf& mb) {
  PW_CHECK(IsCompatible(mb));
  size_type entries_per_chunk = entries_per_chunk_;
  while (entries_per_chunk_ < mb.entries_per_chunk_) {
    if (!AddLayer(0)) {
      break;
    }
  }
  if (entries_per_chunk_ >= mb.entries_per_chunk_ &&
      TryReserveEntries(pos, entries_per_chunk_ * mb.num_chunks())) {
    return true;
  }
  while (entries_per_chunk_ > entries_per_chunk) {
    PopLayer();
  }
  return false;
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos, size_t size) {
  PW_CHECK_UINT_LE(size, Entry::kMaxSize);
  return TryReserveEntries(pos, entries_per_chunk_);
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos,
                                          size_t size,
                                          const Deallocator* deallocator) {
  PW_CHECK(IsCompatible(deallocator));
  return TryReserveForInsert(pos, size);
}

bool GenericMultiBuf::TryReserveForInsert(const_iterator pos,
                                          size_t size,
                                          const ControlBlock* control_block) {
  PW_CHECK(IsCompatible(control_block));
  return TryReserveForInsert(pos, size);
}

void GenericMultiBuf::Insert(const_iterator pos, GenericMultiBuf&& mb) {
  PW_CHECK(TryReserveForInsert(pos, mb));
  if (!has_deallocator()) {
    CopyMemoryContext(mb);
  }

  // Make room for the other object's entries.
  size_type chunk = InsertChunks(pos, mb.num_chunks());

  // Merge the entries into this object.
  size_t size = 0;
  while (!mb.empty()) {
    size_type index = chunk * entries_per_chunk_;
    size_type i = 0;
    size_type offset = mb.GetOffset(0);
    size_type length = mb.GetLength(0);
    for (; i < mb.entries_per_chunk_; ++i) {
      deque_[index + i] = mb.deque_.front();
      mb.deque_.pop_front();
    }

    // If this object is deeper than `mb`, pad it with extra entries.
    for (; i < entries_per_chunk_; ++i) {
      deque_[index + i].view = {
          .offset = offset,
          .sealed = false,
          .length = length,
          .boundary = true,
      };
    }
    size += size_t{length};
    ++chunk;
  }
  if (mb.observer_ != nullptr) {
    mb.observer_->Notify(Observer::Event::kBytesRemoved, size);
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesAdded, size);
  }
}

void GenericMultiBuf::Insert(const_iterator pos, ConstByteSpan bytes) {
  Insert(pos, bytes, 0, bytes.size());
}

void GenericMultiBuf::Insert(const_iterator pos,
                             ConstByteSpan bytes,
                             size_t offset,
                             size_t length,
                             Deallocator* deallocator) {
  PW_CHECK(TryReserveForInsert(pos, bytes.size(), deallocator));
  if (!has_deallocator()) {
    SetDeallocator(deallocator);
  }
  size_type chunk = Insert(pos, bytes, offset, length);
  deque_[base_view_index(chunk)].base_view.owned = true;
}

void GenericMultiBuf::Insert(const_iterator pos,
                             ConstByteSpan bytes,
                             size_t offset,
                             size_t length,
                             ControlBlock* control_block) {
  PW_CHECK(TryReserveForInsert(pos, bytes.size(), control_block));
  if (!has_control_block()) {
    SetControlBlock(control_block);
  }
  size_type chunk = Insert(pos, bytes, offset, length);
  deque_[base_view_index(chunk)].base_view.shared = true;
}

bool GenericMultiBuf::IsRemovable(const_iterator pos, size_t size) const {
  PW_CHECK(pos != cend());
  PW_CHECK_UINT_NE(size, 0u);
  if (static_cast<size_t>(cend() - pos) < size) {
    return false;
  }
  auto [chunk, offset] = GetChunkAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_chunk, end_offset] = GetChunkAndOffset(end);
  return (offset == 0 || !IsOwned(chunk)) &&
         (end_offset == 0 || !IsOwned(end_chunk));
}

Result<GenericMultiBuf> GenericMultiBuf::Remove(const_iterator pos,
                                                size_t size) {
  PW_CHECK(IsRemovable(pos, size));
  GenericMultiBuf out(deque_.get_allocator());
  if (!TryReserveForRemove(pos, size, &out)) {
    return Status::ResourceExhausted();
  }
  CopyRange(pos, size, out);
  EraseRange(pos, size);
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, size);
  }
  return Result<GenericMultiBuf>(std::move(out));
}

Result<GenericMultiBuf> GenericMultiBuf::PopFrontFragment() {
  PW_CHECK(!empty());
  size_t size = 0;
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    size_type length = GetLength(chunk);
    if (length == 0) {
      continue;
    }
    size += size_t{length};
    if (IsBoundary(chunk)) {
      break;
    }
  }
  return Remove(begin(), size);
}

Result<GenericMultiBuf::const_iterator> GenericMultiBuf::Discard(
    const_iterator pos, size_t size) {
  PW_CHECK_UINT_NE(size, 0u);
  difference_type out_offset = pos - begin();
  if (!TryReserveForRemove(pos, size, nullptr)) {
    return Status::ResourceExhausted();
  }
  ClearRange(pos, size);
  EraseRange(pos, size);
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, size);
  }
  return cbegin() + out_offset;
}

bool GenericMultiBuf::IsReleasable(const_iterator pos) const {
  PW_CHECK(pos != cend());
  auto [chunk, offset] = GetChunkAndOffset(pos);
  return IsOwned(chunk);
}

UniquePtr<std::byte[]> GenericMultiBuf::Release(const_iterator pos) {
  PW_CHECK(IsReleasable(pos));
  auto [chunk, offset] = GetChunkAndOffset(pos);
  ByteSpan bytes(GetData(chunk),
                 deque_[base_view_index(chunk)].base_view.length);
  auto* deallocator = GetDeallocator();
  EraseRange(pos - offset, size_t{GetLength(chunk)});
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, bytes.size());
  }
  return UniquePtr<std::byte[]>(bytes.data(), bytes.size(), *deallocator);
}

bool GenericMultiBuf::IsShareable(const_iterator pos) const {
  PW_CHECK(pos != cend());
  auto [chunk, offset] = GetChunkAndOffset(pos);
  return !IsOwned(chunk) && IsShared(chunk);
}

std::byte* GenericMultiBuf::Share(const_iterator pos) {
  PW_CHECK(IsShareable(pos));
  auto [chunk, offset] = GetChunkAndOffset(pos);
  GetControlBlock()->IncrementShared();
  return GetData(chunk);
}

size_t GenericMultiBuf::CopyTo(ByteSpan dst, size_t offset) const {
  return CopyToImpl(dst, offset, 0);
}

size_t GenericMultiBuf::CopyFrom(ConstByteSpan src, size_t offset) {
  size_t total = 0;
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    if (src.empty()) {
      break;
    }
    ByteSpan view = GetView(chunk);
    if (offset < view.size()) {
      size_t size = std::min(view.size() - offset, src.size());
      std::memcpy(view.data() + offset, src.data(), size);
      src = src.subspan(size);
      offset = 0;
      total += size;
    } else {
      offset -= view.size();
    }
  }
  return total;
}

ConstByteSpan GenericMultiBuf::Get(ByteSpan copy, size_t offset) const {
  ByteSpan buffer;
  std::optional<size_type> start;
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    ByteSpan view = GetView(chunk);
    if (buffer.empty() && offset >= view.size()) {
      // Still looking for start of data.
      offset -= view.size();
    } else if (buffer.empty()) {
      // Found the start of data.
      buffer = view.subspan(offset);
      start = chunk;
    } else if (buffer.data() + buffer.size() == view.data()) {
      // Current view is contiguous with previous; append.
      buffer = ByteSpan(buffer.data(), buffer.size() + view.size());
    } else {
      // Span is discontiguous and needs to be copied.
      size_t copied = CopyToImpl(copy, offset, start.value());
      return copy.subspan(0, copied);
    }
  }
  // Requested span is contiguous and can be directly passed to the visitor.
  return buffer.size() <= copy.size() ? buffer : buffer.subspan(0, copy.size());
}

void GenericMultiBuf::Clear() {
  while (entries_per_chunk_ > Entry::kMinEntriesPerChunk) {
    UnsealTopLayer();
    PopLayer();
  }
  // Free any owned chunks.
  Deallocator* deallocator = has_deallocator() ? GetDeallocator() : nullptr;
  size_t num_bytes = 0;
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    num_bytes += size_t{GetLength(chunk)};
    if (!IsOwned(chunk)) {
      continue;
    }
    deallocator->Deallocate(GetData(chunk));
    if (!IsShared(chunk)) {
      continue;
    }
    for (size_type shared = FindShared(chunk, chunk + 1);
         shared != num_chunks();
         shared = FindShared(chunk, shared + 1)) {
      deque_[base_view_index(shared)].base_view.owned = false;
      deque_[base_view_index(shared)].base_view.shared = false;
    }
  }
  deque_.clear();
  ClearMemoryContext();
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesRemoved, num_bytes);
    observer_ = nullptr;
  }
}

void GenericMultiBuf::ShrinkToFit() { deque_.shrink_to_fit(); }

bool GenericMultiBuf::TryReserveLayers(size_t num_layers, size_t num_chunks) {
  if (num_layers == 0 || num_chunks == 0) {
    return true;
  }
  size_type num_entries = 0;
  PW_CHECK(CheckedIncrement(num_layers, Entry::kMinEntriesPerChunk - 1));
  PW_CHECK(CheckedMul(num_layers, num_chunks, num_entries));
  if (num_entries <= deque_.size()) {
    return true;
  }
  return TryReserveEntries(num_entries - deque_.size());
}

bool GenericMultiBuf::AddLayer(size_t offset, size_t length) {
  CheckRange(offset, length, size());
  size_t num_fragments = NumFragments();

  // Given entries with layers A and B, to which we want to add layer C:
  //     A1 B1 A2 B2 A3 B3 A4 B4
  // 1). Add `shift` empty buffers:
  //     A1 B1 A2 B2 A3 B3 A4 B4 -- -- -- --
  size_type shift = num_chunks();
  if (!TryReserveEntries(shift)) {
    return false;
  }
  ++entries_per_chunk_;
  for (size_t i = 0; i < shift; ++i) {
    deque_.push_back({.data = nullptr});
  }

  // 2). Shift the existing layers over. This is expensive, but slicing usually
  //     happens with `shift == 1`:
  for (size_type i = deque_.size(); i != 0; --i) {
    if (i % entries_per_chunk_ == 0) {
      --shift;
      deque_[i - 1].view = {
          .offset = 0,
          .sealed = false,
          .length = 0,
          .boundary = false,
      };
    } else {
      deque_[i - 1] = deque_[i - 1 - shift];
    }
  }

  // 3). Fill in the new layer C with subspans of layer B:
  //     A1 B1 C1 A2 B2 C2 A3 B3 C3 A4 B4 C4
  SetLayer(offset, length);

  // 4). Mark the end of the new layer.
  if (!deque_.empty()) {
    deque_.back().view.boundary = true;
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kLayerAdded, num_fragments);
  }
  return true;
}

void GenericMultiBuf::SealTopLayer() {
  PW_CHECK_UINT_GT(entries_per_chunk_, Entry::kMinEntriesPerChunk);
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    deque_[top_view_index(chunk)].view.sealed = true;
  }
}

void GenericMultiBuf::UnsealTopLayer() {
  PW_CHECK_UINT_GT(entries_per_chunk_, Entry::kMinEntriesPerChunk);
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    deque_[top_view_index(chunk)].view.sealed = false;
  }
}

void GenericMultiBuf::TruncateTopLayer(size_t length) {
  PW_CHECK_UINT_GT(entries_per_chunk_, Entry::kMinEntriesPerChunk);
  PW_CHECK_UINT_LE(length, size());
  PW_CHECK(!IsTopLayerSealed(),
           "MultiBuf::TruncateTopLayer() was called on a sealed layer; call "
           "UnsealTopLayer first");
  if (length == size()) {
    return;
  }
  size_t offset = GetRelativeOffset(0);
  Entry& current = deque_[entries_per_chunk_ - 1];
  CheckRange(offset, length, current.view.offset + size());
  SetLayer(offset, length);
}

void GenericMultiBuf::PopLayer() {
  PW_CHECK_UINT_GT(entries_per_chunk_, Entry::kMinEntriesPerChunk);
  PW_CHECK(!IsTopLayerSealed(),
           "MultiBuf::PopLayer() was called on a sealed layer; call "
           "UnsealTopLayer first");
  size_t num_fragments = NumFragments();

  // Given entries with layers A, B, and C, to remove layer C:
  //     A1 B1 C1 A2 B2 C2 A3 B3 C3 A4 B4 C4
  // 1). Check that the layer is not sealed.
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    PW_CHECK(!IsSealed(chunk));
  }

  // 2). Compress lower layers backward.
  //     -- -- -- -- A1 B1 A2 B2 A3 B3 A4 B4
  size_type shift = 0;
  size_type discard = deque_.size() / entries_per_chunk_;
  size_type keep = deque_.size() - discard;
  --entries_per_chunk_;
  for (size_type i = 1; i <= keep; ++i) {
    size_type j = deque_.size() - i;
    if ((i - 1) % entries_per_chunk_ == 0) {
      ++shift;
    }
    deque_[j] = deque_[j - shift];
    if ((j - discard) % entries_per_chunk_ != entries_per_chunk_ - 1) {
      continue;
    }
  }

  // 3). Discard the first elements
  //     A1 B1 A2 B2 A3 B3 A4 B4
  for (size_type i = 0; i < discard; ++i) {
    deque_.pop_front();
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kLayerRemoved, num_fragments);
  }
}

// Implementation methods

size_t GenericMultiBuf::CheckRange(size_t offset, size_t length, size_t size) {
  PW_CHECK_UINT_LE(offset, size);
  if (length == dynamic_extent) {
    return size - offset;
  }
  PW_CHECK_UINT_LE(length, size - offset);
  return length;
}

Deallocator* GenericMultiBuf::GetDeallocator() const {
  switch (memory_tag_) {
    case MemoryTag::kDeallocator:
      return memory_context_.deallocator;
    case MemoryTag::kControlBlock:
      return memory_context_.control_block->allocator();
    case MemoryTag::kEmpty:
      PW_CRASH("Invalid memory tag");
  }
  PW_UNREACHABLE;
}

void GenericMultiBuf::SetDeallocator(Deallocator* deallocator) {
  memory_tag_ = MemoryTag::kDeallocator;
  memory_context_.deallocator = deallocator;
}

GenericMultiBuf::ControlBlock* GenericMultiBuf::GetControlBlock() const {
  PW_DCHECK_UINT_EQ(memory_tag_, MemoryTag::kControlBlock);
  return memory_context_.control_block;
}

void GenericMultiBuf::SetControlBlock(ControlBlock* control_block) {
  memory_tag_ = MemoryTag::kControlBlock;
  memory_context_.control_block = control_block;
  memory_context_.control_block->IncrementShared();
}

void GenericMultiBuf::CopyMemoryContext(const GenericMultiBuf& other) {
  memory_tag_ = other.memory_tag_;
  memory_context_ = other.memory_context_;
}

void GenericMultiBuf::ClearMemoryContext() {
  if (memory_tag_ == MemoryTag::kControlBlock) {
    memory_context_.control_block->DecrementShared();
  }
  memory_tag_ = MemoryTag::kEmpty;
  memory_context_.deallocator = nullptr;
}

bool GenericMultiBuf::IsCompatible(const GenericMultiBuf& other) const {
  if (other.has_control_block()) {
    return IsCompatible(other.GetControlBlock());
  }
  if (other.has_deallocator()) {
    return IsCompatible(other.GetDeallocator());
  }
  return true;
}

bool GenericMultiBuf::IsCompatible(const Deallocator* other) const {
  return !has_deallocator() || GetDeallocator() == other;
}

bool GenericMultiBuf::IsCompatible(const ControlBlock* other) const {
  return has_control_block() ? (GetControlBlock() == other)
                             : IsCompatible(other->allocator());
}

GenericMultiBuf::size_type GenericMultiBuf::NumFragments() const {
  size_type num_fragments = 0;
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    if (GetLength(chunk) != 0 && IsBoundary(chunk)) {
      ++num_fragments;
    }
  }
  return num_fragments;
}

std::pair<GenericMultiBuf::size_type, GenericMultiBuf::size_type>
GenericMultiBuf::GetChunkAndOffset(const_iterator pos) const {
  size_type chunk = pos.chunk();
  size_type offset = 0;
  size_t left = pos.offset_;
  while (left != 0 && chunk < num_chunks()) {
    size_t length = size_t{GetLength(chunk)};
    if (left < length) {
      offset = static_cast<size_type>(left);
      left = 0;
      break;
    }
    left -= length;
    ++chunk;
  }
  PW_CHECK_UINT_EQ(left, 0u);
  return std::make_pair(chunk, offset);
}

bool GenericMultiBuf::TryReserveEntries(const_iterator pos,
                                        size_type num_entries) {
  auto [chunk, offset] = GetChunkAndOffset(pos);
  return TryReserveEntries(num_entries, offset != 0);
}

bool GenericMultiBuf::TryReserveEntries(size_type num_entries, bool split) {
  if (split) {
    PW_CHECK(CheckedAdd(num_entries, entries_per_chunk_, num_entries));
  }
  PW_CHECK(CheckedAdd(num_entries, deque_.size(), num_entries));
  return deque_.try_reserve_exact(num_entries);
}

GenericMultiBuf::size_type GenericMultiBuf::InsertChunks(const_iterator pos,
                                                         size_type num_chunks) {
  auto [chunk, offset] = GetChunkAndOffset(pos);
  if (offset != 0) {
    num_chunks++;
  }
  size_type num_entries = num_chunks * entries_per_chunk_;
  Entry entry;
  entry.data = nullptr;
  for (size_type i = 0; i < num_entries; ++i) {
    deque_.push_back(entry);
  }
  size_type index = chunk * entries_per_chunk_;
  for (size_type i = deque_.size() - 1; i >= index + num_entries; --i) {
    deque_[i] = deque_[i - num_entries];
  }

  if (offset == 0) {
    // New chunk falls between existing chunks.
    return chunk;
  }
  // New chunk within an existing chunk, which must be split.
  SplitAfter(chunk, offset, deque_, chunk + num_chunks);
  SplitBefore(chunk, offset);
  return chunk + 1;
}

GenericMultiBuf::size_type GenericMultiBuf::Insert(const_iterator pos,
                                                   ConstByteSpan bytes,
                                                   size_t offset,
                                                   size_t length) {
  length = CheckRange(offset, length, bytes.size());
  size_type chunk = InsertChunks(pos, 1);
  deque_[data_index(chunk)].data = const_cast<std::byte*>(bytes.data());
  auto offset_ = static_cast<Entry::size_type>(offset);
  auto length_ = static_cast<Entry::size_type>(length);
  deque_[base_view_index(chunk)].base_view = {
      .offset = offset_,
      .owned = false,
      .length = length_,
      .shared = false,
  };
  for (size_type layer = 2; layer <= NumLayers(); ++layer) {
    deque_[view_index(chunk, layer)].view = {
        .offset = offset_,
        .sealed = false,
        .length = length_,
        .boundary = true,
    };
  }
  if (observer_ != nullptr) {
    observer_->Notify(Observer::Event::kBytesAdded, length);
  }
  return chunk;
}

void GenericMultiBuf::SplitBase(size_type chunk,
                                Deque& out_deque,
                                size_type out_chunk) {
  if (IsOwned(chunk)) {
    PW_CHECK_PTR_EQ(&deque_, &out_deque);
    deque_[base_view_index(chunk)].base_view.shared = true;
  }
  if (&deque_ == &out_deque && chunk == out_chunk) {
    return;
  }
  size_type index = chunk * entries_per_chunk_;
  size_type out_index = out_chunk * entries_per_chunk_;
  for (size_type i = 0; i < entries_per_chunk_; ++i) {
    out_deque[out_index + i] = deque_[index + i];
  }
}

void GenericMultiBuf::SplitBefore(size_type chunk,
                                  size_type split,
                                  Deque& out_deque,
                                  size_type out_chunk) {
  SplitBase(chunk, out_deque, out_chunk);
  split += GetOffset(chunk);
  Entry::BaseView src_base_view = deque_[base_view_index(chunk)].base_view;
  Entry::BaseView& dst_base_view =
      out_deque[base_view_index(out_chunk)].base_view;
  dst_base_view.offset = src_base_view.offset;
  dst_base_view.length = split - src_base_view.offset;
  for (size_type layer = 2; layer <= NumLayers(); ++layer) {
    Entry::View src_view = deque_[view_index(chunk, layer)].view;
    Entry::View& dst_view = out_deque[view_index(out_chunk, layer)].view;
    dst_view.offset = src_view.offset;
    dst_view.length = split - src_view.offset;
  }
}

void GenericMultiBuf::SplitBefore(size_type chunk, size_type split) {
  SplitBefore(chunk, split, deque_, chunk);
}

void GenericMultiBuf::SplitAfter(size_type chunk,
                                 size_type split,
                                 Deque& out_deque,
                                 size_type out_chunk) {
  SplitBase(chunk, out_deque, out_chunk);
  split += GetOffset(chunk);
  Entry::BaseView src_base_view = deque_[base_view_index(chunk)].base_view;
  Entry::BaseView& dst_base_view =
      out_deque[base_view_index(out_chunk)].base_view;
  dst_base_view.offset = split;
  dst_base_view.length = src_base_view.offset + src_base_view.length - split;
  for (size_type layer = 2; layer <= NumLayers(); ++layer) {
    Entry::View src_view = deque_[view_index(chunk, layer)].view;
    Entry::View& dst_view = out_deque[view_index(out_chunk, layer)].view;
    dst_view.offset = split;
    dst_view.length = src_view.offset + src_view.length - split;
  }
}

void GenericMultiBuf::SplitAfter(size_type chunk, size_type split) {
  SplitAfter(chunk, split, deque_, chunk);
}

bool GenericMultiBuf::TryReserveForRemove(const_iterator pos,
                                          size_t size,
                                          GenericMultiBuf* out) {
  auto [chunk, offset] = GetChunkAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_chunk, end_offset] = GetChunkAndOffset(end);
  size_type shift = end_chunk - chunk;
  if (shift == 0 && offset != 0) {
    return (out == nullptr || out->TryReserveEntries(entries_per_chunk_)) &&
           TryReserveEntries(0, offset != 0);
  }
  if (out == nullptr) {
    return true;
  }
  if (shift == 0 && offset == 0) {
    return out->TryReserveEntries(entries_per_chunk_);
  }
  if (end_offset != 0) {
    ++shift;
  }
  return out == nullptr || out->TryReserveEntries(shift * entries_per_chunk_);
}

void GenericMultiBuf::CopyRange(const_iterator pos,
                                size_t size,
                                GenericMultiBuf& out) {
  out.entries_per_chunk_ = entries_per_chunk_;
  out.CopyMemoryContext(*this);

  auto [chunk, offset] = GetChunkAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_chunk, end_offset] = GetChunkAndOffset(end);

  // Determine how many entries needs to be moved.
  size_type shift = end_chunk - chunk;

  // Are we removing the prefix of a single chunk?
  if (shift == 0 && offset == 0) {
    out.InsertChunks(begin(), 1);
    SplitBefore(chunk, end_offset, out.deque_, 0);
    return;
  }

  // Are we removing a sub-chunk? If so, split the chunk in two.
  if (shift == 0) {
    out.InsertChunks(begin(), 1);
    SplitBefore(end_chunk, end_offset, out.deque_, 0);
    out.SplitAfter(0, offset);
    return;
  }

  // Otherwise, start by copying entries to the new deque, if provided.
  size_type out_chunk = 0;
  size_type reserve = end_offset == 0 ? shift : shift + 1;
  out.InsertChunks(cend(), reserve);

  // Copy the suffix of the first chunk.
  if (offset != 0) {
    SplitAfter(chunk, offset, out.deque_, out_chunk);
    --shift;
    ++chunk;
    ++out_chunk;
  }

  // Copy the complete chunks.
  pw::copy(deque_.begin() + (chunk * entries_per_chunk_),
           deque_.begin() + (end_chunk * entries_per_chunk_),
           out.deque_.begin() + (out_chunk * entries_per_chunk_));

  chunk += shift;
  out_chunk += shift;
  if (has_control_block()) {
    GetControlBlock()->IncrementShared();
  }

  // Copy the prefix of the last chunk.
  if (end_offset != 0) {
    SplitBefore(end_chunk, end_offset, out.deque_, out_chunk);
  }
}

void GenericMultiBuf::ClearRange(const_iterator pos, size_t size) {
  auto [chunk, offset] = GetChunkAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_chunk, end_offset] = GetChunkAndOffset(end);

  // Deallocate any owned memory that was not moved.
  if (!has_deallocator()) {
    return;
  }
  Deallocator* deallocator = GetDeallocator();
  if (offset != 0) {
    ++chunk;
  }
  for (; chunk < end_chunk; ++chunk) {
    if (!IsOwned(chunk)) {
      continue;
    }
    if (IsShared(chunk) && (FindShared(chunk, 0) != chunk ||
                            FindShared(chunk, end_chunk) != num_chunks())) {
      // There is at least one shared reference being kept.
      continue;
    }
    deallocator->Deallocate(GetData(chunk));
  }
}

void GenericMultiBuf::EraseRange(const_iterator pos, size_t size) {
  auto [chunk, offset] = GetChunkAndOffset(pos);
  auto end = pos + static_cast<difference_type>(size);
  auto [end_chunk, end_offset] = GetChunkAndOffset(end);

  // Are we removing a sub-chunk? If so, split the chunk in two.
  if (chunk == end_chunk && offset != 0) {
    size_type new_chunk = InsertChunks(pos, 0);
    SplitAfter(new_chunk, end_offset - offset);
    return;
  }

  // Discard suffix of first chunk.
  if (offset != 0) {
    SplitBefore(chunk, offset);
    ++chunk;
  }

  // Discard prefix of last chunk.
  if (end_offset != 0) {
    SplitAfter(end_chunk, end_offset);
  }

  // Discard complete chunks.
  if (chunk < end_chunk) {
    deque_.erase(deque_.begin() + (chunk * entries_per_chunk_),
                 deque_.begin() + (end_chunk * entries_per_chunk_));
  }

  // Check if the memory context is still needed.
  if (!has_deallocator()) {
    return;
  }
  Deallocator* deallocator = GetDeallocator();
  bool needs_deallocator = false;
  for (chunk = 0; chunk < num_chunks(); ++chunk) {
    if (IsOwned(chunk)) {
      needs_deallocator = true;
    } else if (IsShared(chunk)) {
      return;
    }
  }
  ClearMemoryContext();
  if (needs_deallocator) {
    SetDeallocator(deallocator);
  }
}

GenericMultiBuf::size_type GenericMultiBuf::FindShared(size_type chunk,
                                                       size_type start) {
  std::byte* data = GetData(chunk);
  for (chunk = start; chunk < num_chunks(); ++chunk) {
    if (IsShared(chunk) && data == GetData(chunk)) {
      break;
    }
  }
  return chunk;
}

size_t GenericMultiBuf::CopyToImpl(ByteSpan dst,
                                   size_t offset,
                                   size_type start) const {
  size_t total = 0;
  for (size_type chunk = start; chunk < num_chunks(); ++chunk) {
    if (dst.empty()) {
      break;
    }
    ConstByteSpan view = GetView(chunk);
    if (offset < view.size()) {
      size_t size = std::min(view.size() - offset, dst.size());
      std::memcpy(dst.data(), view.data() + offset, size);
      dst = dst.subspan(size);
      offset = 0;
      total += size;
    } else {
      offset -= view.size();
    }
  }
  return total;
}

bool GenericMultiBuf::IsTopLayerSealed() const {
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    if (deque_[top_view_index(chunk)].view.sealed) {
      return true;
    }
  }
  return false;
}

void GenericMultiBuf::SetLayer(size_t offset, size_t length) {
  for (size_type chunk = 0; chunk < num_chunks(); ++chunk) {
    Entry& lower = deque_[top_view_index(chunk) - 1];
    size_type lower_offset, lower_length;
    if (entries_per_chunk_ - 1 == Entry::kMinEntriesPerChunk) {
      lower_offset = lower.base_view.offset;
      lower_length = lower.base_view.length;
    } else {
      lower_offset = lower.view.offset;
      lower_length = lower.view.length;
    }

    // Skip over entries until we reach `offset`.
    Entry& entry = deque_[top_view_index(chunk)];
    if (offset >= lower_length) {
      offset -= size_t{lower_length};
      entry.view.offset = 0;
      entry.view.length = 0;
      continue;
    }
    entry.view.offset = lower_offset + static_cast<size_type>(offset);
    lower_length -= static_cast<size_type>(offset);

    if (length == dynamic_extent) {
      entry.view.length = lower_length;
    } else {
      entry.view.length =
          static_cast<size_type>(std::min(size_t{lower_length}, length));
      length -= size_t{entry.view.length};
    }
    offset = 0;
  }
}

}  // namespace pw::multibuf::internal
