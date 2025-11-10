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

#include "pw_bluetooth_proxy/internal/l2cap_coc_internal.h"
#include "pw_bluetooth_proxy/l2cap_coc_config.h"

namespace pw::bluetooth::proxy {

/// L2CAP connection-oriented channel that supports writing to and reading
/// from a remote peer.
class L2capCoc final {
 public:
  // TODO: https://pwbug.dev/382783733 - Move downstream client to
  // `L2capChannelEvent` instead of `L2capCoc::Event` and delete this alias.
  using Event = L2capChannelEvent;

  using CocConfig = ConnectionOrientedChannelConfig;

  L2capCoc(const L2capCoc& other) = delete;
  L2capCoc& operator=(const L2capCoc& other) = delete;
  /// Channel is moved on return from factory function, so client is responsible
  /// for storing channel.
  L2capCoc(L2capCoc&& other);
  // TODO: https://pwbug.dev/360929142 - Define move assignment operator so
  // `L2capCoc` can be erased from pw containers.
  L2capCoc& operator=(L2capCoc&& other) = delete;

  ~L2capCoc() = default;

  /// Send a payload to the remote peer.
  ///
  /// @param[in] payload The client payload to be sent. Payload will be
  /// destroyed once its data has been used.
  ///
  /// @returns A `StatusWithMultiBuf` with one of the statuses below. If status
  /// is not @OK then payload is also returned in `StatusWithMultiBuf`.
  /// * @OK: Packet was successfully queued for send.
  /// * @UNAVAILABLE: Channel could not acquire the resources to queue
  ///   the send at this time (transient error). If an `event_fn` has been
  ///   provided it will be called with `L2capChannelEvent::kWriteAvailable`
  ///   when there is queue space available again.
  /// * @INVALID_ARGUMENT: Payload is too large.
  /// * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  /// * @UNIMPLEMENTED: Channel does not support `Write(MultiBuf)`.
  // TODO: https://pwbug.dev/388082771 - Plan to eventually move this to
  // ClientChannel.
  StatusWithMultiBuf Write(FlatConstMultiBuf&& payload);

  /// Determine if channel is ready to accept one or more Write payloads.
  ///
  /// @returns
  /// * @OK: Channel is ready to accept one or more `Write` payloads.
  /// * @UNAVAILABLE: Channel does not yet have the resources to queue a Write
  ///   at this time (transient error). If an `event_fn` has been provided it
  ///   will be called with `L2capChannelEvent::kWriteAvailable` when there is
  ///   queue space available again.
  /// * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  Status IsWriteAvailable();

  // Max number of Tx L2CAP packets that can be waiting to send.
  static constexpr size_t QueueCapacity() {
    return internal::L2capCocInternal::QueueCapacity();
  }

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
  pw::Status SendAdditionalRxCredits(uint16_t additional_rx_credits);

  L2capChannel::State state() const;

  uint16_t local_cid() const;

  uint16_t remote_cid() const;

  uint16_t connection_handle() const;

  AclTransportType transport() const;

  // Stop the channel in internal tests. DO NOT USE.
  void StopForTesting();

  // Close the channel in internal tests. DO NOT USE.
  void CloseForTesting();

 private:
  friend class L2capChannelManager;

  L2capCoc(internal::L2capCocInternal&& internal);

  internal::L2capCocInternal internal_;
};

}  // namespace pw::bluetooth::proxy
