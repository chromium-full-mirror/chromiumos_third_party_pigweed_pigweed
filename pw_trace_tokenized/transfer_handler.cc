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

#include "pw_trace_tokenized/transfer_handler.h"

#include <climits>

#include "pw_assert/check.h"
#include "pw_containers/algorithm.h"
#include "pw_log/log.h"
#include "pw_trace_tokenized/trace_buffer.h"

namespace pw::trace {
namespace {

TraceBufferReader trace_buffer_reader;

}  // namespace

TraceBufferReader& GetTraceBufferReader() { return trace_buffer_reader; }

size_t TraceBufferReader::MoveFromBlockCache(ByteSpan dest) {
  size_t len = std::min(dest.size(), block_cache_.size());
  if (len != 0) {
    pw::copy(block_cache_.begin(), block_cache_.begin() + len, dest.begin());
    block_cache_ = block_cache_.subspan(len);
  }
  return len;
}

size_t TraceBufferReader::MoveFromTraceBuffer(ByteSpan dest) {
  if (dest.empty()) {
    return 0;
  }
  TraceBuffer* trace_buffer = trace::GetBuffer();
  size_t total_read = 0;
  while (!dest.empty()) {
    size_t bytes_read = 0;
    if (!trace_buffer->PeekFront(dest.subspan(1), &bytes_read).ok()) {
      break;
    }
    PW_CHECK_UINT_LE(bytes_read, std::numeric_limits<uint8_t>::max());
    dest[0] = static_cast<std::byte>(bytes_read);
    ++bytes_read;
    trace_buffer->PopFront().IgnoreError();
    total_read += bytes_read;
    dest = dest.subspan(bytes_read);
  }
  return total_read;
}

StatusWithSize TraceBufferReader::DoRead(ByteSpan dest) {
  PW_CHECK(!dest.empty());

  size_t total = 0;
  size_t moved = MoveFromBlockCache(dest);
  dest = dest.subspan(moved);
  total += moved;

  moved = MoveFromTraceBuffer(dest);
  dest = dest.subspan(moved);
  total += moved;

  if (!dest.empty()) {
    moved = MoveFromTraceBuffer(block_buffer_);
    block_cache_ = ByteSpan(block_buffer_, moved);
    total += MoveFromBlockCache(dest);
  }

  return total == 0 ? StatusWithSize::OutOfRange() : StatusWithSize(total);
}

}  // namespace pw::trace
