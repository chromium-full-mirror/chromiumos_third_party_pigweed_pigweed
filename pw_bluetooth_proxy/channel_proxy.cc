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

#include "pw_bluetooth_proxy/channel_proxy.h"

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

ChannelProxy::ChannelProxy(
    L2capChannelManager& l2cap_channel_manager,
    MultiBufAllocator* rx_multibuf_allocator,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    OptionalPayloadReceiveCallback&& payload_from_controller_fn,
    OptionalPayloadReceiveCallback&& payload_from_host_fn,
    ChannelEventCallback&& event_fn)
    : L2capChannel(
          l2cap_channel_manager,
          rx_multibuf_allocator,
          /*connection_handle=*/connection_handle,
          /*transport=*/transport,
          /*local_cid=*/local_cid,
          /*remote_cid=*/remote_cid,
          /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
          /*payload_from_host_fn=*/std::move(payload_from_host_fn)),
      L2capChannel::Holder(/*underlying_channel=*/this),
      event_fn_(std::move(event_fn)) {
  // TODO: https://pwbug.dev/388082771 - Adjust log parameters once we are done
  // with transition.
  PW_LOG_INFO("btproxy: ChannelProxy ctor - this: %p, this(Holder): %p",
              (void*)this,
              (void*)(L2capChannel::Holder*)this);

  // Verify L2capChannel::holder_ and Holder::underlying_channel_ were properly
  // set.
  // TODO: https://pwbug.dev/388082771 - Being used for testing during
  // transition. Delete when done.
  CheckHolder(this);
  CheckUnderlyingChannel(this);
}

ChannelProxy::ChannelProxy(ChannelProxy&& other)
    : L2capChannel(std::move(static_cast<L2capChannel&>(other))),
      L2capChannel::Holder(
          std::move(static_cast<L2capChannel::Holder&>(other))),
      event_fn_(std::move(other.event_fn_)) {
  // TODO: https://pwbug.dev/388082771 - To delete. Just using during
  // transition.
  PW_LOG_INFO(
      "btproxy: ChannelProxy move ctor after - this: %p, this(Holder): "
      "%p",
      (void*)this,
      (void*)(L2capChannel::Holder*)this);
  // Verify L2capChannel::holder_ and Holder::underlying_channel_ were properly
  // set.
  // TODO: https://pwbug.dev/388082771 - Being used for testing during
  // transition. Delete when done.
  CheckHolder(this);
  CheckUnderlyingChannel(this);
}

ChannelProxy& ChannelProxy::operator=(ChannelProxy&& other) {
  if (this != &other) {
    PW_LOG_INFO(
        "btproxy: ChannelProxy move= other - this: %p, this(Holder): "
        "%p",
        (void*)&other,
        (void*)(L2capChannel::Holder*)&other);

    L2capChannel::operator=(std::move(static_cast<L2capChannel&>(other)));
    L2capChannel::Holder::operator=(
        std::move(static_cast<L2capChannel::Holder&>(other)));

    event_fn_ = std::move(other.event_fn_);
    other.event_fn_ = nullptr;

    PW_LOG_INFO(
        "btproxy: ChannelProxy move= after - this: %p, this(Holder): "
        "%p",
        (void*)this,
        (void*)(L2capChannel::Holder*)this);
  }
  // Verify L2capChannel::holder_ and Holder::underlying_channel_ are properly
  // set.
  // TODO: https://pwbug.dev/388082771 - Being used for testing during
  // transition. Delete when done.
  CheckHolder(this);
  CheckUnderlyingChannel(this);
  return *this;
}

ChannelProxy::~ChannelProxy() {
  // Log dtor unless this is a moved from object.
  if (state() != State::kUndefined) {
    PW_LOG_INFO("btproxy: ChannelProxy dtor");
  }
}

void ChannelProxy::SendEventToClient(L2capChannelEvent event) {
  // We don't log kWriteAvailable since they happen often. Optimally we would
  // just debug log them also, but one of our downstreams logs all levels.
  if (event != L2capChannelEvent::kWriteAvailable) {
    // TODO: https://pwbug.dev/388082771 - Add channel identifying information
    // here once/if ChannelProxy has access to it (e.g. via L2capChannel ref).
    PW_LOG_INFO("btproxy: ChannelProxy::SendEventToClient - event: %u",
                cpp23::to_underlying(event));
  }

  if (event_fn_) {
    event_fn_(event);
  }
}

void ChannelProxy::HandleUnderlyingChannelEvent(L2capChannelEvent event) {
  SendEventToClient(event);
}

}  // namespace pw::bluetooth::proxy
