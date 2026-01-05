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

#include "pw_bluetooth_proxy/gatt/gatt.h"

#include <pw_bluetooth/l2cap_frames.emb.h>

#include "pw_allocator/libc_allocator.h"
#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_containers/vector.h"
#include "pw_span/span.h"
#include "pw_sync/no_lock.h"
#include "pw_unit_test/framework.h"

#if PW_BLUETOOTH_PROXY_MULTIBUF == PW_BLUETOOTH_PROXY_MULTIBUF_V1
#include "pw_multibuf/simple_allocator.h"
#else  // PW_BLUETOOTH_PROXY_MULTIBUF
#include "pw_allocator/synchronized_allocator.h"
#include "pw_allocator/testing.h"
#endif  // PW_BLUETOOTH_PROXY_MULTIBUF

namespace pw::bluetooth::proxy::gatt {

namespace {

/// Helper class that can produce an initialized MultiBufAllocator for either
/// Multibuf V1 or V2, depending on the `PW_BLUETOOTH_PROXY_MULTIBUF` config
/// option.
template <size_t kDataCapacity, typename Lock = sync::NoLock>
class MultiBufAllocatorContext {
 public:
  MultiBufAllocator& GetAllocator() { return allocator_; }

 private:
  // Use libc allocators so msan can detect use after frees.
#if PW_BLUETOOTH_PROXY_MULTIBUF == PW_BLUETOOTH_PROXY_MULTIBUF_V1

  std::array<std::byte, kDataCapacity> buffer_{};
  pw::multibuf::SimpleAllocator allocator_{
      /*data_area=*/buffer_,
      /*metadata_alloc=*/allocator::GetLibCAllocator()};

#elif PW_BLUETOOTH_PROXY_MULTIBUF == PW_BLUETOOTH_PROXY_MULTIBUF_V2
  using BlockType = allocator::test::AllocatorForTest<0>::BlockType;

  static constexpr size_t kDataSize =
      AlignUp(kDataCapacity + BlockType::kBlockOverhead, BlockType::kAlignment);

  allocator::test::AllocatorForTest<kDataSize> data_alloc_;
  allocator::SynchronizedAllocator<Lock> synced_{data_alloc_};
  MultiBufAllocator allocator_{
      /*data_alloc=*/synced_,
      /*metadata_alloc=*/allocator::GetLibCAllocator()};

#endif  // PW_BLUETOOTH_PROXY_MULTIBUF
};

constexpr ConnectionHandle kConnectionHandle1{0x0001};
constexpr ConnectionHandle kConnectionHandle2{0x0002};
constexpr AttributeHandle kAttributeHandle1{0x0001};
constexpr AttributeHandle kAttributeHandle2{0x0002};
constexpr uint16_t kAttFixedChannelId =
    static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL);

using SpanReceiveFunction = L2capChannelManagerInterface::SpanReceiveFunction;

class FakeChannel final : public ChannelProxy {
 private:
  StatusWithMultiBuf DoWrite(FlatConstMultiBuf&& buf) override {
    FlatConstMultiBufInstance _ = std::move(buf);
    return {Status::Unimplemented(), std::nullopt};
  }
  Status DoIsWriteAvailable() override { return Status::Unimplemented(); }
  Status DoSendAdditionalRxCredits(uint16_t) override {
    return Status::Unimplemented();
  }
};

struct Notification {
  ConnectionHandle connection_handle;
  AttributeHandle value_handle;
  FlatConstMultiBufInstance value;
};

class FakeClientDelegate final : public Client::Delegate {
 public:
  const Vector<Notification>& notifications() const { return notifications_; }
  const Vector<std::pair<Error, ConnectionHandle>>& errors() const {
    return errors_;
  }

 private:
  void DoHandleNotification(ConnectionHandle connection_handle,
                            AttributeHandle value_handle,
                            FlatConstMultiBuf&& value) override {
    notifications_.push_back(
        {connection_handle, value_handle, std::move(value)});
  }

  void DoHandleError(Error error, ConnectionHandle connection_handle) override {
    errors_.emplace_back(error, connection_handle);
  }

  Vector<Notification, 15> notifications_;
  Vector<std::pair<Error, ConnectionHandle>, 10> errors_;
};

class GattTest : public ::testing::Test, public L2capChannelManagerInterface {
 public:
  void SetUp() override {
    gatt_.emplace(*this, allocator_, allocator_context_.GetAllocator());
  }

  Gatt& gatt() { return gatt_.value(); }

  Vector<SpanReceiveFunction>& receive_from_controller_functions() {
    return payload_from_controller_fns_;
  }

  Vector<SpanReceiveFunction>& receive_from_host_functions() {
    return payload_from_host_fns_;
  }

  bool ReceiveFromController(span<std::byte> payload) {
    PW_CHECK(!payload_from_controller_fns_.empty());
    return payload_from_controller_fns_[0](
        payload, kConnectionHandle1, kAttFixedChannelId, kAttFixedChannelId);
  }

  bool ReceiveFromHost(span<std::byte> payload) {
    PW_CHECK(!payload_from_host_fns_.empty());
    return payload_from_host_fns_[0](
        payload, kConnectionHandle1, kAttFixedChannelId, kAttFixedChannelId);
  }

  const Vector<ChannelEventCallback>& event_callbacks() const {
    return event_callbacks_;
  }

  void SendEvent(L2capChannelEvent event) {
    PW_CHECK(!event_callbacks_.empty());
    event_callbacks_[0](event);
  }

  void set_intercept_channel_status(Status status) {
    intercept_channel_status_ = status;
  }

 private:
  Result<UniquePtr<ChannelProxy>> DoInterceptBasicModeChannel(
      ConnectionHandle /*connection_handle*/,
      uint16_t local_channel_id,
      uint16_t remote_channel_id,
      AclTransportType transport,
      BufferReceiveFunction&& payload_from_controller_fn,
      BufferReceiveFunction&& payload_from_host_fn,
      ChannelEventCallback&& event_fn) override {
    if (!intercept_channel_status_.ok()) {
      return intercept_channel_status_;
    }

    EXPECT_EQ(
        local_channel_id,
        static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL));
    EXPECT_EQ(
        remote_channel_id,
        static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL));
    EXPECT_EQ(transport, AclTransportType::kLe);

    UniquePtr<ChannelProxy> channel = allocator_.MakeUnique<FakeChannel>();
    PW_CHECK(channel != nullptr);
    payload_from_controller_fns_.emplace_back(
        std::move(std::get<SpanReceiveFunction>(payload_from_controller_fn)));
    payload_from_host_fns_.emplace_back(
        std::move(std::get<SpanReceiveFunction>(payload_from_host_fn)));
    event_callbacks_.emplace_back(std::move(event_fn));
    return channel;
  }

  allocator::test::AllocatorForTest<1000> allocator_;
  MultiBufAllocatorContext<500> allocator_context_;
  std::optional<Gatt> gatt_;
  Vector<SpanReceiveFunction, 10> payload_from_controller_fns_;
  Vector<SpanReceiveFunction, 10> payload_from_host_fns_;
  Vector<ChannelEventCallback, 10> event_callbacks_;
  Status intercept_channel_status_ = OkStatus();
};

TEST_F(
    GattTest,
    Receive2NotificationsForMatchingHandleAndIgnore1NotificationForOtherHandle) {
  FakeClientDelegate delegate;

  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);
  ASSERT_EQ(receive_from_controller_functions().size(), 1u);

  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));

  std::array<std::byte, 1> expected_value_0 = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet_0 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},     // handle
      expected_value_0[0]  // value
  };
  EXPECT_TRUE(ReceiveFromController(att_packet_0));
  ASSERT_EQ(delegate.notifications().size(), 1u);
  EXPECT_EQ(delegate.notifications()[0].connection_handle, kConnectionHandle1);
  EXPECT_EQ(delegate.notifications()[0].value_handle, kAttributeHandle1);

  span<const std::byte> value_0 =
      as_bytes(MultiBufAdapter::AsSpan(delegate.notifications()[0].value));
  EXPECT_TRUE(std::equal(value_0.begin(),
                         value_0.end(),
                         expected_value_0.begin(),
                         expected_value_0.end()));

  std::array<std::byte, 2> expected_value_1 = {std::byte{0x09},
                                               std::byte{0x0a}};
  std::array<std::byte, 5> att_packet_1 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},  // handle
      expected_value_1[0],
      expected_value_1[1]  // value
  };
  EXPECT_TRUE(ReceiveFromController(att_packet_1));
  ASSERT_EQ(delegate.notifications().size(), 2u);
  EXPECT_EQ(delegate.notifications()[1].connection_handle, kConnectionHandle1);
  EXPECT_EQ(delegate.notifications()[1].value_handle, kAttributeHandle1);

  span<const std::byte> value_1 =
      as_bytes(MultiBufAdapter::AsSpan(delegate.notifications()[1].value));
  EXPECT_TRUE(std::equal(value_1.begin(),
                         value_1.end(),
                         expected_value_1.begin(),
                         expected_value_1.end()));

  std::array<std::byte, 4> att_packet_2 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x09},
      std::byte{0x00},  // handle
      std::byte{0x99}   // value
  };
  EXPECT_FALSE(ReceiveFromController(att_packet_2));
  ASSERT_EQ(delegate.notifications().size(), 2u);
}

TEST_F(GattTest, IgnoreReceivingUnsupportedOpcode) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);
  ASSERT_EQ(receive_from_controller_functions().size(), 1u);
  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));

  std::array<std::byte, 1> expected_value = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet = {
      std::byte{0x12},  // opcode (write request)
      std::byte{0x01},
      std::byte{0x00},   // handle
      expected_value[0]  // value
  };
  EXPECT_FALSE(ReceiveFromController(att_packet));
  ASSERT_EQ(delegate.notifications().size(), 0u);
}

TEST_F(GattTest, TwoClientsReceiveSameNotification) {
  FakeClientDelegate delegate_0;
  Result<Client> client_0 = gatt().CreateClient(kConnectionHandle1, delegate_0);
  PW_TEST_ASSERT_OK(client_0);
  ASSERT_EQ(receive_from_controller_functions().size(), 1u);
  PW_TEST_ASSERT_OK(client_0->InterceptNotification(kAttributeHandle1));

  FakeClientDelegate delegate_1;
  Result<Client> client_1 = gatt().CreateClient(kConnectionHandle1, delegate_1);
  PW_TEST_ASSERT_OK(client_1);
  ASSERT_EQ(receive_from_controller_functions().size(), 1u);
  PW_TEST_ASSERT_OK(client_1->InterceptNotification(kAttributeHandle1));

  std::array<std::byte, 1> expected_value_0 = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet_0 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},     // handle
      expected_value_0[0]  // value
  };
  EXPECT_TRUE(ReceiveFromController(att_packet_0));
  ASSERT_EQ(delegate_0.notifications().size(), 1u);
  EXPECT_EQ(delegate_0.notifications()[0].connection_handle,
            kConnectionHandle1);
  EXPECT_EQ(delegate_0.notifications()[0].value_handle, kAttributeHandle1);
  ASSERT_EQ(delegate_1.notifications().size(), 1u);
  EXPECT_EQ(delegate_1.notifications()[0].connection_handle,
            kConnectionHandle1);
  EXPECT_EQ(delegate_1.notifications()[0].value_handle, kAttributeHandle1);
}

TEST_F(GattTest, PayloadFromHostIgnored) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);
  ASSERT_EQ(receive_from_host_functions().size(), 1u);
  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));

  std::array<std::byte, 1> expected_value_0 = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet_0 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},     // handle
      expected_value_0[0]  // value
  };
  EXPECT_FALSE(ReceiveFromHost(att_packet_0));
  ASSERT_EQ(delegate.notifications().size(), 0u);
}

TEST_F(GattTest, CancelInterceptNotification) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);

  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));

  std::array<std::byte, 1> expected_value_0 = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet_0 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},     // handle
      expected_value_0[0]  // value
  };
  EXPECT_TRUE(ReceiveFromController(att_packet_0));
  ASSERT_EQ(delegate.notifications().size(), 1u);
  EXPECT_EQ(delegate.notifications()[0].connection_handle, kConnectionHandle1);
  EXPECT_EQ(delegate.notifications()[0].value_handle, kAttributeHandle1);

  span<const std::byte> value_0 =
      as_bytes(MultiBufAdapter::AsSpan(delegate.notifications()[0].value));
  EXPECT_TRUE(std::equal(value_0.begin(),
                         value_0.end(),
                         expected_value_0.begin(),
                         expected_value_0.end()));

  PW_TEST_ASSERT_OK(client->CancelInterceptNotification(kAttributeHandle1));

  std::array<std::byte, 2> expected_value_1 = {std::byte{0x09},
                                               std::byte{0x0a}};
  std::array<std::byte, 5> att_packet_1 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},  // handle
      expected_value_1[0],
      expected_value_1[1]  // value
  };
  EXPECT_FALSE(ReceiveFromController(att_packet_1));
  ASSERT_EQ(delegate.notifications().size(), 1u);
}

TEST_F(GattTest, CloseClient) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);

  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));
  client->Close();
  EXPECT_EQ(delegate.errors().size(), 1u);
  client->Close();  // Should be no-op
  EXPECT_EQ(delegate.errors().size(), 1u);

  std::array<std::byte, 1> expected_value_0 = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet_0 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},     // handle
      expected_value_0[0]  // value
  };
  EXPECT_FALSE(ReceiveFromController(att_packet_0));
  ASSERT_EQ(delegate.notifications().size(), 0u);
}

TEST_F(GattTest, MovingIntoClientClosesClient) {
  FakeClientDelegate delegate;
  Result<Client> result = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(result);
  Client client = std::move(result.value());

  PW_TEST_ASSERT_OK(client.InterceptNotification(kAttributeHandle1));
  Client client2;
  client = std::move(client2);
  EXPECT_EQ(delegate.errors().size(), 1u);

  std::array<std::byte, 1> expected_value_0 = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet_0 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},     // handle
      expected_value_0[0]  // value
  };
  EXPECT_FALSE(ReceiveFromController(att_packet_0));
  ASSERT_EQ(delegate.notifications().size(), 0u);
}

TEST_F(GattTest, NotifyClientOnChannelClose) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);
  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));
  ASSERT_EQ(delegate.errors().size(), 0u);

  SendEvent(L2capChannelEvent::kChannelClosedByOther);
  ASSERT_EQ(delegate.errors().size(), 1u);
  EXPECT_EQ(delegate.errors()[0].first, Error::kDisconnection);
  EXPECT_EQ(delegate.errors()[0].second, kConnectionHandle1);
}

TEST_F(GattTest, NotifyClientOnReset) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);
  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));
  ASSERT_EQ(delegate.errors().size(), 0u);

  SendEvent(L2capChannelEvent::kReset);
  ASSERT_EQ(delegate.errors().size(), 1u);
  EXPECT_EQ(delegate.errors()[0].first, Error::kReset);
  EXPECT_EQ(delegate.errors()[0].second, kConnectionHandle1);
}

TEST_F(GattTest, IgnoreEmptyAttPacket) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);
  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));

  EXPECT_FALSE(ReceiveFromController(ByteSpan()));
  ASSERT_EQ(delegate.notifications().size(), 0u);
}

TEST_F(GattTest, IgnoreReceivedTooSmallAttNotificationPacket) {
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  PW_TEST_ASSERT_OK(client);
  PW_TEST_ASSERT_OK(client->InterceptNotification(kAttributeHandle1));

  std::array<std::byte, 2> invalid_att_packet = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01}   // truncated handle
  };
  EXPECT_FALSE(ReceiveFromController(invalid_att_packet));
  ASSERT_EQ(delegate.notifications().size(), 0u);
}

TEST_F(GattTest, InterceptNotificationOn2Connections) {
  FakeClientDelegate delegate_0;
  Result<Client> client_0 = gatt().CreateClient(kConnectionHandle1, delegate_0);
  PW_TEST_ASSERT_OK(client_0);
  PW_TEST_ASSERT_OK(client_0->InterceptNotification(kAttributeHandle1));

  FakeClientDelegate delegate_1;
  Result<Client> client_1 = gatt().CreateClient(kConnectionHandle2, delegate_1);
  PW_TEST_ASSERT_OK(client_1);
  PW_TEST_ASSERT_OK(client_1->InterceptNotification(kAttributeHandle2));

  std::array<std::byte, 1> expected_value_0 = {std::byte{0x08}};
  std::array<std::byte, 4> att_packet_0 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x01},
      std::byte{0x00},     // handle
      expected_value_0[0]  // value
  };
  EXPECT_TRUE(ReceiveFromController(att_packet_0));
  ASSERT_EQ(delegate_0.notifications().size(), 1u);
  EXPECT_EQ(delegate_0.notifications()[0].connection_handle,
            kConnectionHandle1);
  EXPECT_EQ(delegate_0.notifications()[0].value_handle, kAttributeHandle1);

  span<const std::byte> value_0 =
      as_bytes(MultiBufAdapter::AsSpan(delegate_0.notifications()[0].value));
  EXPECT_TRUE(std::equal(value_0.begin(),
                         value_0.end(),
                         expected_value_0.begin(),
                         expected_value_0.end()));

  std::array<std::byte, 2> expected_value_1 = {std::byte{0x09},
                                               std::byte{0x0a}};
  std::array<std::byte, 5> att_packet_1 = {
      std::byte{0x1b},  // opcode (notification)
      std::byte{0x02},
      std::byte{0x00},  // handle
      expected_value_1[0],
      expected_value_1[1]  // value
  };
  EXPECT_TRUE(receive_from_controller_functions()[1](att_packet_1,
                                                     kConnectionHandle2,
                                                     kAttFixedChannelId,
                                                     kAttFixedChannelId));
  ASSERT_EQ(delegate_0.notifications().size(), 1u);
  ASSERT_EQ(delegate_1.notifications().size(), 1u);
  EXPECT_EQ(delegate_1.notifications()[0].connection_handle,
            kConnectionHandle2);
  EXPECT_EQ(delegate_1.notifications()[0].value_handle, kAttributeHandle2);
}

TEST_F(GattTest, InterceptBasicModeChannelReturnsError) {
  set_intercept_channel_status(Status::InvalidArgument());
  FakeClientDelegate delegate;
  Result<Client> client = gatt().CreateClient(kConnectionHandle1, delegate);
  EXPECT_EQ(client.status(), Status::Unavailable());
}

}  // namespace
}  // namespace pw::bluetooth::proxy::gatt
