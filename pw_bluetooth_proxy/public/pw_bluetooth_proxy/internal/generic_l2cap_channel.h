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

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy::internal {

/// Common base class for client-facing L2CAP channels.
///
/// L2CAP channels are implemented using two types: an "internal" type that is
/// and managed by the L2capChannelManager, and a client-facing channel that
/// provides the public channel interface.
///
/// @tparam   InternalChannel   Internal channel object managed by the
///                             L2capChannelManager.
template <typename InternalChannel>
class GenericL2capChannel {
 public:
  GenericL2capChannel(const GenericL2capChannel& other) = delete;
  GenericL2capChannel& operator=(const GenericL2capChannel& other) = delete;

  GenericL2capChannel(GenericL2capChannel&&) = default;
  GenericL2capChannel& operator=(GenericL2capChannel&& other) = default;

  ~GenericL2capChannel() = default;

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
  StatusWithMultiBuf Write(FlatConstMultiBuf&& payload) {
    return internal_.Write(std::move(payload));
  }

  /// Determine if channel is ready to accept one or more Write payloads.
  ///
  /// @returns
  /// * @OK: Channel is ready to accept one or more `Write` payloads.
  /// * @UNAVAILABLE: Channel does not yet have the resources to queue a Write
  ///   at this time (transient error). If an `event_fn` has been provided it
  ///   will be called with `L2capChannelEvent::kWriteAvailable` when there is
  ///   queue space available again.
  /// * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  Status IsWriteAvailable() { return internal_.IsWriteAvailable(); }

  static constexpr size_t QueueCapacity() {
    return InternalChannel::QueueCapacity();
  }

  constexpr L2capChannel::State state() const { return internal_.state(); }

  constexpr uint16_t local_cid() const { return internal_.local_cid(); }

  constexpr uint16_t remote_cid() const { return internal_.remote_cid(); }

  constexpr uint16_t connection_handle() const {
    return internal_.connection_handle();
  }

  constexpr AclTransportType transport() const { return internal_.transport(); }

 protected:
  constexpr explicit GenericL2capChannel(InternalChannel&& internal)
      : internal_(std::move(internal)) {}

  constexpr InternalChannel& internal() { return internal_; }
  constexpr const InternalChannel& internal() const { return internal_; }

 private:
  InternalChannel internal_;
};

}  // namespace pw::bluetooth::proxy::internal
