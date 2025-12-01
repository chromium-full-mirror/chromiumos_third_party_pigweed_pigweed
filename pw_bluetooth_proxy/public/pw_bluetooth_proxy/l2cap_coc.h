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

#include "pw_bluetooth_proxy/internal/generic_l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_coc_internal.h"
#include "pw_bluetooth_proxy/l2cap_coc_config.h"

namespace pw::bluetooth::proxy {

/// L2CAP connection-oriented channel that supports writing to and reading
/// from a remote peer.
class L2capCoc final
    : public internal::GenericL2capChannel<internal::L2capCocInternal> {
 private:
  using Base = internal::GenericL2capChannel<internal::L2capCocInternal>;

 public:
  // TODO: https://pwbug.dev/382783733 - Move downstream client to
  // `L2capChannelEvent` instead of `L2capCoc::Event` and delete this alias.
  using Event = L2capChannelEvent;

  using CocConfig = ConnectionOrientedChannelConfig;

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
  pw::Status SendAdditionalRxCredits(uint16_t additional_rx_credits) {
    return internal().SendAdditionalRxCredits(additional_rx_credits);
  }

  // Stop the channel in internal tests. DO NOT USE.
  void StopForTesting() { internal().Stop(); }

  // Close the channel in internal tests. DO NOT USE.
  void Close() { internal().Close(); }

 private:
  friend class L2capChannelManager;

  constexpr explicit L2capCoc(internal::L2capCocInternal&& internal)
      : Base(std::move(internal)) {}
};

}  // namespace pw::bluetooth::proxy
