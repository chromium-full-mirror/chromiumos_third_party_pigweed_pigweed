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

#include "pw_bluetooth_proxy/internal/gatt_notify_tx_engine.h"

#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy_private/test_utils.h"
#include "pw_containers/inline_queue.h"
#include "pw_unit_test/framework.h"

namespace pw::bluetooth::proxy::internal {

namespace {

constexpr uint16_t kConnectionHandle = 0x40;
constexpr uint16_t kRemoteCid = 0x50;
constexpr uint16_t kAttributeHandle = 5;
constexpr std::array<std::byte, 3> kPayload0 = {
    std::byte{0x00}, std::byte{0x01}, std::byte{0x02}};
constexpr std::array<std::byte, 3> kPayload1 = {
    std::byte{0x03}, std::byte{0x04}, std::byte{0x05}};

class GattNotifyTxEngineTest : public ::testing::Test,
                               public TxEngine::Delegate {
 public:
  void SetUp() override {
    // Queue 2 packets
    MultiBufAllocator& allocator = packet_allocator_context_.GetAllocator();
    std::optional<FlatMultiBufInstance> buffer_0 =
        MultiBufAdapter::Create(allocator, kPayload0.size());
    MultiBufAdapter::Copy(buffer_0.value(), /*dst_offset=*/0, kPayload0);
    payload_queue_.emplace(
        std::move(MultiBufAdapter::Unwrap(buffer_0.value())));

    std::optional<FlatMultiBufInstance> buffer_1 =
        MultiBufAdapter::Create(allocator, kPayload1.size());
    MultiBufAdapter::Copy(buffer_1.value(), /*dst_offset=*/0, kPayload1);
    payload_queue_.emplace(
        std::move(MultiBufAdapter::Unwrap(buffer_1.value())));

    max_payload_size_ = 20;
    engine_.emplace(kConnectionHandle, kRemoteCid, kAttributeHandle, *this);
  }

  void TearDown() override { engine_.reset(); }

  GattNotifyTxEngine& engine() { return engine_.value(); }

  MultiBufAllocator& multibuf_allocator() {
    return packet_allocator_context_.GetAllocator();
  }

  void set_max_payload_size(std::optional<uint16_t> max_payload_size) {
    max_payload_size_ = max_payload_size;
  }

  // TxEngine::Delegate overrides:

  std::optional<uint16_t> MaxL2capPayloadSize() override {
    return max_payload_size_;
  }

  const FlatConstMultiBuf& FrontPayload() {
    PW_CHECK(!payload_queue_.empty());
    return MultiBufAdapter::Unwrap(payload_queue_.front());
  }

  void PopFrontPayload() {
    ASSERT_FALSE(payload_queue_.empty());
    payload_queue_.pop();
  }

  pw::Result<H4PacketWithH4> AllocateH4(uint16_t length) override {
    void* allocation =
        allocator_.Allocate(allocator::Layout(length, alignof(uint8_t)));
    PW_ASSERT(allocation);
    return H4PacketWithH4(
        pw::span<uint8_t>(static_cast<uint8_t*>(allocation), length),
        [this](const uint8_t* buffer) {
          allocator_.Deallocate(const_cast<uint8_t*>(buffer));
        });
  }

 private:
  pw::allocator::test::AllocatorForTest<500> allocator_;
  MultiBufAllocatorContext<500> packet_allocator_context_;
  std::optional<uint16_t> max_payload_size_;
  InlineQueue<FlatConstMultiBufInstance, 2> payload_queue_;
  std::optional<GattNotifyTxEngine> engine_;
};

TEST_F(GattNotifyTxEngineTest, GenerateNextPacket) {
  bool keep_payload = true;
  Result<H4PacketWithH4> packet_0 =
      engine().GenerateNextPacket(FrontPayload(), keep_payload);
  PW_TEST_ASSERT_OK(packet_0);
  EXPECT_FALSE(keep_payload);
  PopFrontPayload();
  pw::span<uint8_t> packet_0_span = packet_0->GetH4Span();
  EXPECT_EQ(packet_0_span.size(), 15u);
  const std::array<uint8_t, 15> kExpectedH4_0 = {0x02,  // H4 type: ACL
                                                        // ACL header:
                                                 0x40,
                                                 0x00,  // Handle
                                                 0x0a,
                                                 0x00,  // Data Total Length
                                                        // L2cap B-Frame:
                                                 0x06,
                                                 0x00,  // PDU length
                                                 0x50,
                                                 0x00,  // Remote Channel ID
                                                        // ATT notify header:
                                                 0x1b,  // opcode
                                                 0x05,
                                                 0x00,  // handle
                                                        // payload:
                                                 0x00,
                                                 0x01,
                                                 0x02};
  EXPECT_TRUE(std::equal(packet_0_span.begin(),
                         packet_0_span.end(),
                         kExpectedH4_0.begin(),
                         kExpectedH4_0.end()));

  Result<H4PacketWithH4> packet_1 =
      engine().GenerateNextPacket(FrontPayload(), keep_payload);
  PW_TEST_ASSERT_OK(packet_1);
  EXPECT_FALSE(keep_payload);
  PopFrontPayload();
  pw::span<uint8_t> packet_1_span = packet_1->GetH4Span();
  EXPECT_EQ(packet_1_span.size(), 15u);
  const std::array<uint8_t, 15> kExpectedH4_1 = {0x02,  // H4 type: ACL
                                                        // ACL header:
                                                 0x40,
                                                 0x00,  // Handle
                                                 0x0a,
                                                 0x00,  // Data Total Length
                                                        // L2cap B-Frame:
                                                 0x06,
                                                 0x00,  // PDU length
                                                 0x50,
                                                 0x00,  // Remote Channel ID
                                                        // ATT notify header:
                                                 0x1b,  // opcode
                                                 0x05,
                                                 0x00,  // handle
                                                        // payload:
                                                 0x03,
                                                 0x04,
                                                 0x05};
  EXPECT_TRUE(std::equal(packet_1_span.begin(),
                         packet_1_span.end(),
                         kExpectedH4_1.begin(),
                         kExpectedH4_1.end()));
}

TEST_F(GattNotifyTxEngineTest, CheckWriteParameterNoSizeYet) {
  std::optional<FlatMultiBufInstance> buffer =
      MultiBufAdapter::Create(multibuf_allocator(), 1);
  ASSERT_TRUE(buffer.has_value());
  const FlatConstMultiBuf& write_param =
      MultiBufAdapter::Unwrap(buffer.value());

  set_max_payload_size(std::nullopt);
  Status status = engine().CheckWriteParameter(write_param);
  EXPECT_EQ(status, Status::FailedPrecondition());
}

TEST_F(GattNotifyTxEngineTest, CheckWriteParameterSizeTooSmallForHeader) {
  std::optional<FlatMultiBufInstance> buffer =
      MultiBufAdapter::Create(multibuf_allocator(), 1);
  ASSERT_TRUE(buffer.has_value());
  const FlatConstMultiBuf& write_param =
      MultiBufAdapter::Unwrap(buffer.value());

  set_max_payload_size(1);
  Status status = engine().CheckWriteParameter(write_param);
  EXPECT_EQ(status, Status::FailedPrecondition());
}

TEST_F(GattNotifyTxEngineTest, CheckWriteParameterSizeTooSmallForPayload) {
  std::optional<FlatMultiBufInstance> buffer =
      MultiBufAdapter::Create(multibuf_allocator(), 2);
  ASSERT_TRUE(buffer.has_value());
  const FlatConstMultiBuf& write_param =
      MultiBufAdapter::Unwrap(buffer.value());

  set_max_payload_size(4);
  Status status = engine().CheckWriteParameter(write_param);
  EXPECT_EQ(status, Status::InvalidArgument());
}

}  // namespace

}  // namespace pw::bluetooth::proxy::internal
