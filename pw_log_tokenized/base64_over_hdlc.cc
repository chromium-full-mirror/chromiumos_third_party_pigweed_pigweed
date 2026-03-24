// Copyright 2020 The Pigweed Authors
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

// This function serves as a backend for pw_tokenizer / pw_log_tokenized that
// encodes tokenized logs as Base64 and writes them using HDLC.

#include "pw_hdlc/encoder.h"
#include "pw_log_tokenized/base64.h"
#include "pw_log_tokenized/handler.h"
#include "pw_log_tokenized/light_handler.h"
#include "pw_span/span.h"
#include "pw_stream/sys_io_stream.h"
#include "pw_string/string.h"
#include "pw_tokenizer/base64.h"

namespace pw::log_tokenized {
namespace {

inline constexpr int kBase64LogHdlcAddress = 1;

stream::SysIoWriter writer;

}  // namespace

// Base64-encodes tokenized logs and writes them to pw::sys_io as HDLC frames.
extern "C" void pw_log_tokenized_HandleLog(
    uint32_t,  // TODO(hepler): Use the metadata for filtering.
    const uint8_t encoded_message[],
    size_t size_bytes) {
  // Encode the tokenized message as Base64.
  const pw::InlineBasicString base64_string =
      PrefixedBase64Encode(encoded_message, size_bytes);

  // HDLC-encode the Base64 string via a SysIoWriter. Ignore errors since we
  // cannot take any action anyway.
  hdlc::WriteUIFrame(
      kBase64LogHdlcAddress, as_bytes(span(base64_string)), writer)
      .IgnoreError();
}

extern "C" void pw_log_tokenized_HandleLogWithoutMetadata(
    const uint8_t encoded_message[], size_t size_bytes) {
  pw_log_tokenized_HandleLog(0, encoded_message, size_bytes);
}

}  // namespace pw::log_tokenized
