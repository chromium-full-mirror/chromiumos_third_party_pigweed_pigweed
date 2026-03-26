// Copyright 2026 The Pigweed Authors
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

#include "pw_spi_zephyr/responder.h"

#include <zephyr/drivers/spi.h>

#include "pw_assert/check.h"

namespace pw::spi {

namespace {

void SpiCompletionCallback(const struct device* dev,
                           int result,
                           void* userdata) {
  ZephyrResponder* responder = static_cast<ZephyrResponder*>(userdata);
  if (result < 0) {
    responder->HandleCompletion(Status::Unknown());
  } else {
    // For a SPI peripheral, if there's no error,
    // |result| is the number of received frames.
    responder->HandleCompletion(OkStatus());
  }
}

}  // namespace

Status ZephyrResponder::DoWriteReadAsync(ConstByteSpan tx_data,
                                         ByteSpan rx_data) {
  rx_data_ = rx_data;

  const spi_buf tx_bufs[] = {
      {.buf = (void*)tx_data.data(), .len = tx_data.size()}};
  const spi_buf_set tx = {.buffers = tx_bufs, .count = 1};
  const spi_buf rx_bufs[] = {{.buf = rx_data.data(), .len = rx_data.size()}};
  const spi_buf_set rx = {.buffers = rx_bufs, .count = 1};

  int result =
      spi_transceive_cb(dev_, &config_, &tx, &rx, SpiCompletionCallback, this);

  if (result < 0) {
    return Status::Unknown();
  }

  return OkStatus();
}

void ZephyrResponder::DoCancel() { PW_CRASH("Not yet implemented"); }

}  // namespace pw::spi
