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

#pragma once

#include "pw_bluetooth_proxy/channel_proxy.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"

namespace pw::bluetooth::proxy::internal {

class BasicL2capChannelInternal final : public ChannelProxy {
 public:
  using PayloadSpanReceiveCallback = Function<bool(pw::span<uint8_t>)>;

  // TODO: https://pwbug.dev/360929142 - Take the MTU. Signaling channels would
  // provide MTU_SIG.
  static pw::Result<BasicL2capChannelInternal> Create(
      L2capChannelManager& l2cap_channel_manager,
      MultiBufAllocator* rx_multibuf_allocator,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      OptionalPayloadReceiveCallback&& payload_from_controller_fn,
      OptionalPayloadReceiveCallback&& payload_from_host_fn,
      ChannelEventCallback&& event_fn);

  /// @param payload_span_from_controller_fn Function to call with paylods of
  /// basic frames from the controller as spans. This is an optimization over
  /// allocating MultiBufs for payload_from_controller_fn.
  /// @param payload_span_from_host_fn Function to call with paylods of basic
  /// frames from the host as spans. This is an optimization over allocating
  /// MultiBufs for payload_from_host_fn.
  explicit BasicL2capChannelInternal(
      L2capChannelManager& l2cap_channel_manager,
      MultiBufAllocator* rx_multibuf_allocator,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      OptionalPayloadReceiveCallback&& payload_from_controller_fn,
      OptionalPayloadReceiveCallback&& payload_from_host_fn,
      ChannelEventCallback&& event_fn,
      PayloadSpanReceiveCallback&& payload_span_from_controller_fn,
      PayloadSpanReceiveCallback&& payload_span_from_host_fn);

  BasicL2capChannelInternal(const BasicL2capChannelInternal& other) = delete;
  BasicL2capChannelInternal& operator=(const BasicL2capChannelInternal& other) =
      delete;
  BasicL2capChannelInternal(BasicL2capChannelInternal&&);
  BasicL2capChannelInternal& operator=(BasicL2capChannelInternal&& other);
  ~BasicL2capChannelInternal() override;

 private:
  void Move(BasicL2capChannelInternal&& other);

  /// Check if the passed Write parameter is acceptable.
  Status DoCheckWriteParameter(const FlatConstMultiBuf& payload) override;

  bool HandlePduFromHost(pw::span<uint8_t> bframe) override;

  void DoClose() override {}

  bool DoHandlePduFromController(pw::span<uint8_t> bframe) override;

  [[nodiscard]] std::optional<H4PacketWithH4> GenerateNextTxPacket()
      PW_EXCLUSIVE_LOCKS_REQUIRED(l2cap_tx_mutex()) override;

  sync::Mutex mutex_;

  // TODO: https://pwbug.dev/388082771 -  This is an optimization to avoid
  // allocating & copying in L2capSignalingChannel. It is temporary until
  // MultiBufv2 migration is complete, MultiBufs are used end-to-end, or the
  // channel refactor makes this obsolete.
  PayloadSpanReceiveCallback payload_span_from_controller_fn_
      PW_GUARDED_BY(mutex_);
  PayloadSpanReceiveCallback payload_span_from_host_fn_ PW_GUARDED_BY(mutex_);
};

}  // namespace pw::bluetooth::proxy::internal
