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
#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>

#include "pw_bytes/span.h"
#include "pw_spi/responder.h"

namespace pw::spi {

class ZephyrResponder : public Responder {
 public:
  ZephyrResponder(const struct device* dev, const struct spi_config& config)
      : dev_(dev), config_(config) {}

  void HandleCompletion(Status status) {
    completion_callback_(rx_data_, status);
  }

 private:
  // pw::spi::Responder impl.
  void DoSetCompletionHandler(
      Function<void(ByteSpan, Status)> callback) override {
    completion_callback_ = std::move(callback);
  };
  Status DoWriteReadAsync(ConstByteSpan tx_data, ByteSpan rx_data) override;
  void DoCancel() override;

  const struct device* dev_;
  struct spi_config config_{};
  Function<void(ByteSpan, Status)> completion_callback_;
  ByteSpan rx_data_;
};

}  // namespace pw::spi
