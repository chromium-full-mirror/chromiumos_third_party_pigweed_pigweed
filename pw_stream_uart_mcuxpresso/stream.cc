// Copyright 2023 The Pigweed Authors
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

#include "pw_stream_uart_mcuxpresso/stream.h"

#include "pw_function/scope_guard.h"

namespace pw::stream {

UartStreamMcuxpresso::~UartStreamMcuxpresso() { USART_RTOS_Deinit(&handle_); }

Status UartStreamMcuxpresso::Init(uint32_t srcclk) {
  config_.srcclk = srcclk;

  // Acquire the clock_tree element. Note that this function only requires the
  // IP clock and not the functional clock. However, ClockMcuxpressoClockIp
  // only provides the combined element, so that's what we use here.
  // Make sure it's released on any function exits through a scoped guard.
  PW_TRY(clock_tree_element_.Acquire());
  pw::ScopeGuard guard([this] { clock_tree_element_.Release().IgnoreError(); });

  if (USART_RTOS_Init(&handle_, &uart_handle_, &config_) != kStatus_Success) {
    return Status::Internal();
  }

  return OkStatus();
}

StatusWithSize UartStreamMcuxpresso::DoRead(ByteSpan data) {
  // Acquire the clock_tree_element. Use a scoped guard so it's released from
  // any function return.
  PW_TRY_WITH_SIZE(clock_tree_element_.Acquire());
  pw::ScopeGuard guard([this] { clock_tree_element_.Release().IgnoreError(); });

  size_t read = 0;
  if (const auto status =
          USART_RTOS_Receive(&handle_,
                             reinterpret_cast<uint8_t*>(data.data()),
                             data.size(),
                             &read);
      status != kStatus_Success) {
    USART_TransferAbortReceive(base_, &uart_handle_);
    return StatusWithSize(Status::Internal(), 0);
  }

  return StatusWithSize(read);
}

Status UartStreamMcuxpresso::DoWrite(ConstByteSpan data) {
  // Acquire the clock_tree_element. Use a scoped guard so it's released from
  // any function return.
  PW_TRY(clock_tree_element_.Acquire());
  pw::ScopeGuard guard([this] { clock_tree_element_.Release().IgnoreError(); });

  if (USART_RTOS_Send(
          &handle_,
          reinterpret_cast<uint8_t*>(const_cast<std::byte*>(data.data())),
          data.size()) != kStatus_Success) {
    return Status::Internal();
  }

  return OkStatus();
}

}  // namespace pw::stream
