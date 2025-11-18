// Copyright 2024 The Pigweed Authors
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

#include <optional>

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_bluetooth_proxy/l2cap_coc_config.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy::internal {

/// L2CAP connection-oriented channel that supports writing to and reading
/// from a remote peer.

class L2capCocInternal final : public L2capChannel {
 public:
  // TODO: https://pwbug.dev/382783733 - Move downstream client to
  // `L2capChannelEvent` instead of `L2capCoc::Event` and delete this alias.
  using Event = L2capChannelEvent;

  static pw::Result<L2capCocInternal> Create(
      MultiBufAllocator& rx_multibuf_allocator,
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      ConnectionOrientedChannelConfig rx_config,
      ConnectionOrientedChannelConfig tx_config,
      ChannelEventCallback&& event_fn,
      Function<void(FlatConstMultiBuf&& payload)>&& receive_fn);

  L2capCocInternal(const L2capCocInternal& other) = delete;
  L2capCocInternal& operator=(const L2capCocInternal& other) = delete;
  /// Channel is moved on return from factory function, so client is responsible
  /// for storing channel.
  L2capCocInternal(L2capCocInternal&& other);
  // TODO: https://pwbug.dev/360929142 - Define move assignment operator so
  // `L2capCocInternal` can be erased from pw containers.
  L2capCocInternal& operator=(L2capCocInternal&& other) = delete;
  ~L2capCocInternal() override;

  /// Check if the passed Write parameter is acceptable.
  Status DoCheckWriteParameter(const FlatConstMultiBuf& payload) override;

  /// Send an L2CAP_FLOW_CONTROL_CREDIT_IND signaling packet to dispense the
  /// remote peer additional L2CAP connection-oriented channel credits for this
  /// channel.
  ///
  /// @param[in] additional_rx_credits Number of credits to dispense.
  ///
  /// @returns
  /// * @OK: The packet was sent.
  /// * @UNAVAILABLE: Send could not be queued due to lack of memory in the
  ///   client-provided rx_multibuf_allocator (transient error).
  /// * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  pw::Status SendAdditionalRxCredits(uint16_t additional_rx_credits)
      PW_LOCKS_EXCLUDED(rx_mutex_);

  // Increment tx credits by `credits`.
  void AddTxCredits(uint16_t credits) PW_LOCKS_EXCLUDED(tx_mutex_);

 protected:
  // `SendPayloadFromControllerToClient` with the information payload contained
  // in `kframe`.
  bool DoHandlePduFromController(pw::span<uint8_t> kframe) override
      PW_LOCKS_EXCLUDED(rx_mutex_);

  bool HandlePduFromHost(pw::span<uint8_t> kframe) override;

  void DoClose() override {}

 private:
  explicit L2capCocInternal(
      MultiBufAllocator& rx_multibuf_allocator,
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      ConnectionOrientedChannelConfig rx_config,
      ConnectionOrientedChannelConfig tx_config,
      ChannelEventCallback&& event_fn,
      Function<void(FlatConstMultiBuf&& payload)>&& receive_fn);

  // Returns max size of L2CAP PDU payload supported by this channel.
  //
  // Returns std::nullopt if ACL data channel is not yet initialized.
  std::optional<uint16_t> MaxBasicL2capPayloadSize() const;

  std::optional<H4PacketWithH4> GenerateNextTxPacket()
      PW_LOCKS_EXCLUDED(tx_mutex_)
          PW_EXCLUSIVE_LOCKS_REQUIRED(l2cap_tx_mutex()) override;

  // Replenish some of the remote's credits.
  pw::Status ReplenishRxCredits(uint16_t additional_rx_credits)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rx_mutex_);

  uint16_t rx_mtu_;
  uint16_t rx_mps_;
  uint16_t tx_mtu_;
  uint16_t tx_mps_;

  Function<void(FlatConstMultiBuf&& payload)> receive_fn_;

  sync::Mutex rx_mutex_;
  std::optional<FlatMultiBufInstance> rx_sdu_ PW_GUARDED_BY(rx_mutex_) =
      std::nullopt;
  uint16_t rx_sdu_offset_ PW_GUARDED_BY(rx_mutex_) = 0;
  uint16_t rx_sdu_bytes_remaining_ PW_GUARDED_BY(rx_mutex_) = 0;
  uint16_t rx_remaining_credits_ PW_GUARDED_BY(rx_mutex_);
  uint16_t rx_total_credits_ PW_GUARDED_BY(rx_mutex_);

  sync::Mutex tx_mutex_;
  uint16_t tx_credits_ PW_GUARDED_BY(tx_mutex_);
  uint16_t tx_sdu_offset_ PW_GUARDED_BY(tx_mutex_) = 0;
  bool is_continuing_segment_ PW_GUARDED_BY(tx_mutex_) = false;
};

}  // namespace pw::bluetooth::proxy::internal
