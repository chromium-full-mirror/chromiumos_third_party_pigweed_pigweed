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

#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_containers/intrusive_map.h"

namespace pw::bluetooth::proxy {
class L2capChannelManager;
}

namespace pw::bluetooth::proxy::internal {

// This class represents a logical link (ACL connection) in the L2CAP protocol.
// This class is not thread-safe and requires external locking.
class L2capLogicalLink final
    : public IntrusiveMap<uint16_t, L2capLogicalLink>::Pair {
 public:
  L2capLogicalLink(uint16_t connection_handle,
                   AclTransportType transport,
                   L2capChannelManager& l2cap_channel_manager);

  L2capLogicalLink(L2capLogicalLink&&) = delete;
  L2capLogicalLink& operator=(L2capLogicalLink&&) = delete;

  // Send L2CAP_FLOW_CONTROL_CREDIT_IND to indicate local endpoint `cid` is
  // capable of receiving a number of additional K-frames (`credits`).
  //
  // @returns
  // * @OK: `L2CAP_FLOW_CONTROL_CREDIT_IND` was sent.
  // * @UNAVAILABLE: Send could not be queued due to lack of memory in the
  //   client-provided `multibuf_allocator` (transient error).
  // * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  Status SendFlowControlCreditInd(uint16_t channel_id,
                                  uint16_t credits,
                                  MultiBufAllocator& multibuf_allocator) {
    return signaling_channel_.SendFlowControlCreditInd(
        channel_id, credits, multibuf_allocator);
  }

  uint16_t connection_handle() const { return connection_handle_; }

  AclTransportType transport() const { return transport_; }

 private:
  const uint16_t connection_handle_;
  const AclTransportType transport_;
  L2capSignalingChannel signaling_channel_;
};

}  // namespace pw::bluetooth::proxy::internal
