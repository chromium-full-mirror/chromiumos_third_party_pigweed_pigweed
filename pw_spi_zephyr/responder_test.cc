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

#include <zephyr/device.h>
#include <zephyr/spi.h>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_sync/binary_semaphore.h"

namespace pw::spi {
namespace {

// These variables are used to share state between the test body and the
// responder under test.
std::vector<uint8_t> expected_tx_data;
std::vector<uint8_t> rx_data_to_provide;
sync::BinarySemaphore completion_semaphore;

}  // namespace
}  // namespace pw::spi

extern "C" {

// Mock implementation of the Zephyr SPI API.
int spi_transceive_cb(const struct device* dev,
                      const struct spi_config* config,
                      const struct spi_buf_set* tx_bufs,
                      const struct spi_buf_set* rx_bufs,
                      spi_transceive_cb_t cb,
                      void* userdata) {
  if (tx_bufs != nullptr && tx_bufs->buffers != nullptr) {
    const auto* tx_buf = tx_bufs->buffers;
    if (tx_buf->buf != nullptr) {
      std::vector<uint8_t> actual_tx_data(
          static_cast<uint8_t*>(tx_buf->buf),
          static_cast<uint8_t*>(tx_buf->buf) + tx_buf->len);
      EXPECT_EQ(actual_tx_data, pw::spi::expected_tx_data);
    }
  }

  if (rx_bufs != nullptr && rx_bufs->buffers != nullptr) {
    auto* rx_buf = rx_bufs->buffers;
    if (rx_buf->buf != nullptr) {
      memcpy(rx_buf->buf, pw::spi::rx_data_to_provide.data(), rx_buf->len);
    }
  }

  if (cb != nullptr) {
    cb(dev, 0, userdata);
  }
  return 0;
}

}  // extern "C"

namespace pw::spi {
namespace {

class ZephyrResponderTest : public ::testing::Test {
 protected:
  ZephyrResponderTest() : dev_{}, config_{}, responder_(&dev_, config_) {}

  const struct device dev_;
  const struct spi_config config_;
  ZephyrResponder responder_;
};

TEST_F(ZephyrResponderTest, Write) {
  expected_tx_data = {0x01, 0x02, 0x03};
  rx_data_to_provide.clear();

  std::array<uint8_t, 3> tx_buffer = {0x01, 0x02, 0x03};
  std::array<uint8_t, 3> rx_buffer;

  responder_.SetCompletionHandler([&](ByteSpan rx_data, Status status) {
    EXPECT_TRUE(status.ok());
    completion_semaphore.release();
  });

  ASSERT_TRUE(responder_.WriteReadAsync(tx_buffer, rx_buffer).ok());
  completion_semaphore.acquire();
}

TEST_F(ZephyrResponderTest, Read) {
  expected_tx_data.clear();
  rx_data_to_provide = {0x04, 0x05, 0x06};

  std::array<uint8_t, 3> tx_buffer;
  std::array<uint8_t, 3> rx_buffer;

  responder_.SetCompletionHandler([&](ByteSpan rx_data, Status status) {
    EXPECT_TRUE(status.ok());
    std::vector<uint8_t> actual_rx_data(rx_data.begin(), rx_data.end());
    EXPECT_EQ(actual_rx_data, rx_data_to_provide);
    completion_semaphore.release();
  });

  ASSERT_TRUE(responder_.WriteReadAsync(tx_buffer, rx_buffer).ok());
  completion_semaphore.acquire();
}

TEST_F(ZephyrResponderTest, WriteRead) {
  expected_tx_data = {0x07, 0x08, 0x09};
  rx_data_to_provide = {0x0a, 0x0b, 0x0c};

  std::array<uint8_t, 3> tx_buffer = {0x07, 0x08, 0x09};
  std::array<uint8_t, 3> rx_buffer;

  responder_.SetCompletionHandler([&](ByteSpan rx_data, Status status) {
    EXPECT_TRUE(status.ok());
    std::vector<uint8_t> actual_rx_data(rx_data.begin(), rx_data.end());
    EXPECT_EQ(actual_rx_data, rx_data_to_provide);
    completion_semaphore.release();
  });

  ASSERT_TRUE(responder_.WriteReadAsync(tx_buffer, rx_buffer).ok());
  completion_semaphore.acquire();
}

}  // namespace
}  // namespace pw::spi
