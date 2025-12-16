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

#include <variant>

#include "pw_allocator/unique_ptr.h"
#include "pw_bluetooth_proxy/channel_proxy.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_bytes/span.h"
#include "pw_function/function.h"

namespace pw::bluetooth::proxy {

class L2capChannelManagerInterface {
 public:
  virtual ~L2capChannelManagerInterface() = default;

  // Return false to intercept/take the packet, or true to forward the packet.
  // This is an optimization to avoid allocating and copying on every H4 packet.
  // TODO: https://pwbug.dev/411168474 - Use multibuf for H4 packets and delete
  // this.
  using SpanReceiveFunction = Function<bool(
      ConstByteSpan, uint16_t connection_handle, uint16_t channel_id)>;

  using OptionalBufferReceiveFunction =
      Function<std::optional<FlatConstMultiBufInstance>(
          FlatMultiBuf&& payload,
          uint16_t connection_handle,
          uint16_t channel_id)>;

  using BufferReceiveFunction =
      std::variant<OptionalBufferReceiveFunction, SpanReceiveFunction>;

  Result<UniquePtr<ChannelProxy>> InterceptBasicModeChannel(
      uint16_t connection_handle,
      uint16_t local_channel_id,
      uint16_t remote_channel_id,
      AclTransportType transport,
      BufferReceiveFunction&& payload_from_controller_fn,
      BufferReceiveFunction&& payload_from_host_fn,
      ChannelEventCallback&& event_fn) {
    return DoInterceptBasicModeChannel(connection_handle,
                                       local_channel_id,
                                       remote_channel_id,
                                       transport,
                                       std::move(payload_from_controller_fn),
                                       std::move(payload_from_host_fn),
                                       std::move(event_fn));
  }

 private:
  virtual Result<UniquePtr<ChannelProxy>> DoInterceptBasicModeChannel(
      uint16_t connection_handle,
      uint16_t local_channel_id,
      uint16_t remote_channel_id,
      AclTransportType transport,
      BufferReceiveFunction&& payload_from_controller_fn,
      BufferReceiveFunction&& payload_from_host_fn,
      ChannelEventCallback&& event_fn) = 0;
};

}  // namespace pw::bluetooth::proxy
