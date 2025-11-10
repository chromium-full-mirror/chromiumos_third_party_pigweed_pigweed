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

#include "pw_bluetooth_proxy/internal/basic_l2cap_channel_internal.h"

namespace pw::bluetooth::proxy {

class BasicL2capChannel final {
 public:
  BasicL2capChannel(const BasicL2capChannel& other) = delete;
  BasicL2capChannel& operator=(const BasicL2capChannel& other) = delete;
  BasicL2capChannel(BasicL2capChannel&&) = default;
  BasicL2capChannel& operator=(BasicL2capChannel&& other) = default;
  ~BasicL2capChannel() = default;

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

  static constexpr size_t QueueCapacity() {
    return internal::BasicL2capChannelInternal::QueueCapacity();
  }

  L2capChannel::State state() const;

  uint16_t local_cid() const;

  uint16_t remote_cid() const;

  uint16_t connection_handle() const;

  AclTransportType transport() const;

  // Close the channel in internal tests. DO NOT USE.
  void CloseForTesting();

  // Returns the internal channel. DO NOT USE.
  internal::BasicL2capChannelInternal& InternalForTesting();

 private:
  friend class L2capChannelManager;

  BasicL2capChannel(internal::BasicL2capChannelInternal&& internal);

  internal::BasicL2capChannelInternal internal_;
};

}  // namespace pw::bluetooth::proxy
