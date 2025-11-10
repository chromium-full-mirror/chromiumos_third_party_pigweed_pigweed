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

#include "pw_bluetooth_proxy/internal/basic_l2cap_channel_internal.h"

#include "pw_assert/check.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy::internal {

pw::Result<BasicL2capChannelInternal> BasicL2capChannelInternal::Create(
    L2capChannelManager& l2cap_channel_manager,
    MultiBufAllocator* rx_multibuf_allocator,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    OptionalPayloadReceiveCallback&& payload_from_controller_fn,
    OptionalPayloadReceiveCallback&& payload_from_host_fn,
    ChannelEventCallback&& event_fn) {
  if (!AreValidParameters(/*connection_handle=*/connection_handle,
                          /*local_cid=*/local_cid,
                          /*remote_cid=*/remote_cid)) {
    return pw::Status::InvalidArgument();
  }

  BasicL2capChannelInternal channel(
      l2cap_channel_manager,
      rx_multibuf_allocator,
      /*connection_handle=*/connection_handle,
      /*transport=*/transport,
      /*local_cid=*/local_cid,
      /*remote_cid=*/remote_cid,
      /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
      /*payload_from_host_fn=*/std::move(payload_from_host_fn),
      /*event_fn=*/std::move(event_fn));
  channel.Init();
  return channel;
}

Status BasicL2capChannelInternal::DoCheckWriteParameter(
    const FlatConstMultiBuf& payload) {
  std::optional<uint16_t> max_l2cap_length = MaxL2capPayloadSize();
  if (!max_l2cap_length) {
    return Status::FailedPrecondition();
  }
  if (payload.size() > max_l2cap_length) {
    PW_LOG_WARN("Payload (%zu bytes) is too large. So will not process.",
                payload.size());
    return Status::InvalidArgument();
  }
  return OkStatus();
}

std::optional<H4PacketWithH4>
BasicL2capChannelInternal::GenerateNextTxPacket() {
  if (state() != State::kRunning || PayloadQueueEmpty()) {
    return std::nullopt;
  }

  const FlatConstMultiBuf& payload = GetFrontPayload();

  pw::Result<H4PacketWithH4> result = PopulateTxL2capPacket(payload.size());
  if (!result.ok()) {
    return std::nullopt;
  }
  H4PacketWithH4 h4_packet = std::move(result.value());

  Result<emboss::AclDataFrameWriter> result2 =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  PW_CHECK(result2.ok());
  emboss::AclDataFrameWriter acl = result2.value();

  emboss::BFrameWriter bframe = emboss::MakeBFrameView(
      acl.payload().BackingStorage().data(), acl.payload().SizeInBytes());
  PW_CHECK(bframe.IsComplete());

  MultiBufAdapter::Copy(bframe.payload(), payload);
  PW_CHECK(acl.Ok());
  PW_CHECK(bframe.Ok());

  // All content has been copied from the front payload, so release it.
  PopFrontPayload();

  return h4_packet;
}

BasicL2capChannelInternal::BasicL2capChannelInternal(
    L2capChannelManager& l2cap_channel_manager,
    MultiBufAllocator* rx_multibuf_allocator,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    OptionalPayloadReceiveCallback&& payload_from_controller_fn,
    OptionalPayloadReceiveCallback&& payload_from_host_fn,
    ChannelEventCallback&& event_fn)
    : ChannelProxy(
          l2cap_channel_manager,
          rx_multibuf_allocator,
          /*connection_handle=*/connection_handle,
          /*transport=*/transport,
          /*local_cid=*/local_cid,
          /*remote_cid=*/remote_cid,
          /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
          /*payload_from_host_fn=*/std::move(payload_from_host_fn),
          /*event_fn=*/std::move(event_fn)) {
  PW_LOG_INFO("btproxy: BasicL2capChannelInternal ctor");
}

BasicL2capChannelInternal::~BasicL2capChannelInternal() {
  // Don't log dtor of moved-from channels.
  if (state() != State::kUndefined) {
    PW_LOG_INFO("btproxy: BasicL2capChannelInternal dtor");
  }
}

bool BasicL2capChannelInternal::DoHandlePduFromController(
    pw::span<uint8_t> bframe) {
  Result<emboss::BFrameWriter> bframe_view =
      MakeEmbossWriter<emboss::BFrameWriter>(bframe);

  if (!bframe_view.ok()) {
    // TODO: https://pwbug.dev/360929142 - Stop channel on error.
    PW_LOG_ERROR("(CID: 0x%X) Received invalid B-frame. So will drop.",
                 local_cid());
    return true;
  }

  return SendPayloadFromControllerToClient(
      span(bframe_view->payload().BackingStorage().data(),
           bframe_view->payload().SizeInBytes()));
}

bool BasicL2capChannelInternal::HandlePduFromHost(pw::span<uint8_t> bframe) {
  Result<emboss::BFrameWriter> bframe_view =
      MakeEmbossWriter<emboss::BFrameWriter>(bframe);

  if (!bframe_view.ok()) {
    // TODO: https://pwbug.dev/360929142 - Stop channel on error.
    PW_LOG_ERROR("(CID: 0x%X) Host transmitted invalid B-frame. So will drop.",
                 local_cid());
    return true;
  }

  return SendPayloadFromHostToClient(
      span(bframe_view->payload().BackingStorage().data(),
           bframe_view->payload().SizeInBytes()));
}

}  // namespace pw::bluetooth::proxy::internal
