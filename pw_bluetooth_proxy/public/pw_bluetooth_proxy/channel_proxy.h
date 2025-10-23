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

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"

namespace pw::bluetooth::proxy {

// While we are transitioning from channel proxies inheriting from L2capChannel
// to them instead composing L2capChannel (https://pwbug.dev/388082771), this
// class will continue to inherit from L2capChannel for client-facing
// functionality we have yet to move. The end goal is all of that client-facing
// functionality will be on ChannelProxy at which point we will not need to
// inherit from L2capChannel.
// Inheriting from L2capChannel::Holder provides this ChannelProxy the
// ability to access its underlying channel.
// TODO: https://pwbug.dev/388082771 - Switch to composing L2capChannel rather
// than inheriting from it.
/// ChannelProxy allows a client to write, read, and receive events from an
/// underlying Bluetooth channel.
class ChannelProxy : public L2capChannel, public L2capChannel::Holder {
 public:
  explicit ChannelProxy(
      L2capChannelManager& l2cap_channel_manager,
      MultiBufAllocator* rx_multibuf_allocator,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      OptionalPayloadReceiveCallback&& payload_from_controller_fn,
      OptionalPayloadReceiveCallback&& payload_from_host_fn,
      ChannelEventCallback&& event_fn);
  ~ChannelProxy() override;

  ChannelProxy(const ChannelProxy& other) = delete;
  ChannelProxy& operator=(const ChannelProxy& other) = delete;
  ChannelProxy(ChannelProxy&& other);
  ChannelProxy& operator=(ChannelProxy&& other);

 protected:
  void SendEventToClient(L2capChannelEvent event);

  // Handle event from underlying channel by sending event to client if an event
  // callback was provided.
  void HandleUnderlyingChannelEvent(L2capChannelEvent event) override;

  // Stop the underlying channel with the provided event.
  // TODO: https://pwbug.dev/388082771 - Look at if we can remove this reverse
  // event flow to L2capChannel.
  void StopUnderlyingChannelWithEvent(L2capChannelEvent event) {
    if (GetUnderlyingChannel()) {
      GetUnderlyingChannel()->StopAndSendEvent(event);
    }
  }

 private:
  // Used to notify clients of events.
  ChannelEventCallback event_fn_;
};

}  // namespace pw::bluetooth::proxy
