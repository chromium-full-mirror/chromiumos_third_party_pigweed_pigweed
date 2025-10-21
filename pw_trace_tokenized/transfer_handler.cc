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

#include "pw_containers/var_len_entry_queue.h"
#include "pw_log/log.h"
#include "pw_trace_tokenized/trace_buffer.h"

namespace pw::trace {
namespace {

TraceBufferReader trace_buffer_reader;

}  // namespace

TraceBufferReader& GetTraceBufferReader() { return trace_buffer_reader; }

StatusWithSize TraceBufferReader::DoRead(ByteSpan dest) {
  InlineVarLenEntryQueue<>& trace_buffer_queue = trace::GetBuffer()->queue();
  if (trace_buffer_queue.empty() && partial_transfer_.empty()) {
    return StatusWithSize::ResourceExhausted();
  }
  PW_LOG_DEBUG("Entry count is: %zu", trace_buffer_queue.size());
  size_t transferred = 0;
  while (true) {
    if (!partial_transfer_.empty()) {
      size_t partial_size = std::min(dest.size(), partial_transfer_.size());
      pw::copy(partial_transfer_.begin(),
               partial_transfer_.begin() + partial_size,
               dest.begin());
      partial_transfer_ = partial_transfer_.subspan(partial_size);
      transferred += partial_size;
    }

    // No more data or no room in destination.
    if (trace_buffer_queue.empty() || !partial_transfer_.empty()) {
      break;
    }
    VarLenEntryQueue dest_queue(dest);
    MoveVarLenEntries(trace_buffer_queue, dest_queue);
    transferred += dest_queue.encoded_size_bytes();

    if (!trace_buffer_queue.empty()) {
      VarLenEntryQueue partial_queue(transfer_buffer_);
      MoveVarLenEntries(trace_buffer_queue, partial_queue);
      size_t partial_size = partial_queue.encoded_size_bytes();
      partial_transfer_ = ConstByteSpan(transfer_buffer_, partial_size);
    }
  }
  return StatusWithSize(transferred);
}

}  // namespace pw::trace
