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

#include <cstddef>
#include <cstdint>

#include "pw_allocator/unique_ptr.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

class L2capChannelManager;

namespace internal {

/// Common base class for client-facing L2CAP channels.
///
/// L2CAP channels are implemented using two types: an "internal" type that is
/// and managed by the L2capChannelManager, and a client-facing channel that
/// provides the public channel interface.
class GenericL2capChannel {
 public:
  GenericL2capChannel(const GenericL2capChannel& other) = delete;
  GenericL2capChannel& operator=(const GenericL2capChannel& other) = delete;

  GenericL2capChannel(GenericL2capChannel&& other) { *this = std::move(other); }
  GenericL2capChannel& operator=(GenericL2capChannel&& other);

  virtual ~GenericL2capChannel();

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
  StatusWithMultiBuf Write(FlatConstMultiBuf&& payload)
      PW_LOCKS_EXCLUDED(L2capChannel::mutex());

  /// Determine if channel is ready to accept one or more Write payloads.
  ///
  /// @returns
  /// * @OK: Channel is ready to accept one or more `Write` payloads.
  /// * @UNAVAILABLE: Channel does not yet have the resources to queue a Write
  ///   at this time (transient error). If an `event_fn` has been provided it
  ///   will be called with `L2capChannelEvent::kWriteAvailable` when there is
  ///   queue space available again.
  /// * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  Status IsWriteAvailable() PW_LOCKS_EXCLUDED(L2capChannel::mutex());

  static constexpr size_t QueueCapacity() {
    return L2capChannel::QueueCapacity();
  }

  uint16_t local_cid() const { return local_cid_; }

  uint16_t remote_cid() const { return remote_cid_; }

  uint16_t connection_handle() const { return connection_handle_; }

  AclTransportType transport() const { return transport_; }

  // Stop the channel.
  //
  // This typically is only used in internal tests.
  void Stop() PW_LOCKS_EXCLUDED(L2capChannel::mutex());

  // Close the channel.
  //
  // This typically is only used in internal tests.
  //
  // Unlike `L2capChannel::Close`, this will send an event to the client while
  // holding the lock to ensure the channel isn't concurrently removed.
  void Close() PW_LOCKS_EXCLUDED(L2capChannel::mutex());

  /// Returns the internal channel. Unsafe; only meant for tests. DO NOT USE.
  L2capChannel* InternalForTesting() const
      PW_LOCKS_EXCLUDED(L2capChannel::mutex());

 protected:
  explicit GenericL2capChannel(L2capChannel& channel);

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
  Status SendAdditionalRxCredits(uint16_t additional_rx_credits)
      PW_LOCKS_EXCLUDED(L2capChannel::mutex());

 private:
  friend class pw::bluetooth::proxy::L2capChannel;
  friend class pw::bluetooth::proxy::L2capChannelManager;

  /// Check if the passed Write parameter is acceptable.
  virtual Status DoCheckWriteParameter(const FlatConstMultiBuf& payload) = 0;

  /// Returns a borrowed L2CAP channel, which can be dereferenced to get the
  /// L2CAP channel. The channel is guaranteed not to be destroyed as long as
  /// the `BorrowedL2capChannel` is in scope.
  ///
  /// @returns
  /// * @OK: Returns the channel.
  /// * @FAILED_PRECONDITION: This object is not connected to an L2CAP channel.
  Result<BorrowedL2capChannel> BorrowL2capChannel() const;

  L2capChannel* channel_ PW_GUARDED_BY(L2capChannel::mutex()) = nullptr;

  // ACL connection handle.
  uint16_t connection_handle_ = 0;

  AclTransportType transport_ = AclTransportType::kBrEdr;

  // L2CAP channel ID of local endpoint.
  uint16_t local_cid_ = 0;

  // L2CAP channel ID of remote endpoint.
  uint16_t remote_cid_ = 0;
};

}  // namespace internal
}  // namespace pw::bluetooth::proxy
