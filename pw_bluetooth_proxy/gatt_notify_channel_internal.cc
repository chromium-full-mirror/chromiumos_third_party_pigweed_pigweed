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

#include "pw_assert/check.h"
#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/gatt_notify_channel.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy::internal {

std::optional<H4PacketWithH4> GattNotifyChannelInternal::GenerateNextTxPacket(
    const FlatConstMultiBuf& attribute_value, bool& keep_payload) {
  keep_payload = true;
  std::optional<uint16_t> max_l2cap_payload_size = MaxL2capPayloadSize();
  // This should have been caught during Write.
  PW_CHECK(max_l2cap_payload_size);
  const uint16_t max_attribute_size =
      *max_l2cap_payload_size - emboss::AttHandleValueNtf::MinSizeInBytes();
  // This should have been caught during Write.
  PW_CHECK(attribute_value.size() < max_attribute_size);

  size_t att_frame_size =
      emboss::AttHandleValueNtf::MinSizeInBytes() + attribute_value.size();

  pw::Result<H4PacketWithH4> result = PopulateTxL2capPacket(att_frame_size);
  if (!result.ok()) {
    return std::nullopt;
  }
  H4PacketWithH4 h4_packet = std::move(result.value());

  // Write ATT PDU.
  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan());
  PW_CHECK_OK(acl);
  PW_CHECK(acl->Ok());

  Result<emboss::BFrameWriter> l2cap = MakeEmbossWriter<emboss::BFrameWriter>(
      acl->payload().BackingStorage().data(),
      acl->payload().BackingStorage().SizeInBytes());
  PW_CHECK_OK(l2cap);
  PW_CHECK(l2cap->Ok());

  PW_CHECK(att_frame_size == l2cap->payload().BackingStorage().SizeInBytes());
  Result<emboss::AttHandleValueNtfWriter> att_notify =
      MakeEmbossWriter<emboss::AttHandleValueNtfWriter>(
          attribute_value.size(),
          l2cap->payload().BackingStorage().data(),
          att_frame_size);
  PW_CHECK_OK(att_notify);

  att_notify->attribute_opcode().Write(emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);
  att_notify->attribute_handle().Write(attribute_handle_);
  MultiBufAdapter::Copy(att_notify->attribute_value(), attribute_value);
  PW_CHECK(att_notify->Ok());

  // All content has been copied from the front payload, so release it.
  keep_payload = false;
  return h4_packet;
}

bool GattNotifyChannelInternal::AreValidParameters(uint16_t connection_handle,
                                                   uint16_t attribute_handle) {
  constexpr auto local_cid =
      static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL);
  constexpr auto remote_cid =
      static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL);
  if (!L2capChannel::AreValidParameters(
          connection_handle, local_cid, remote_cid)) {
    return false;
  }
  if (attribute_handle == 0) {
    PW_LOG_ERROR("Attribute handle cannot be 0.");
    return false;
  }
  return true;
}

GattNotifyChannelInternal::GattNotifyChannelInternal(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t attribute_handle,
    ChannelEventCallback&& event_fn)
    : L2capChannel(
          /*l2cap_channel_manager=*/l2cap_channel_manager,
          /*rx_multibuf_allocator*/ nullptr,
          /*connection_handle=*/connection_handle,
          /*transport=*/AclTransportType::kLe,
          /*local_cid=*/
          static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL),
          /*remote_cid=*/
          static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL),
          /*payload_from_controller_fn=*/nullptr,
          /*payload_from_host_fn=*/nullptr,
          /*event_fn=*/std::move(event_fn)),
      attribute_handle_(attribute_handle) {
  PW_LOG_INFO("btproxy: GattNotifyChannelInternal ctor - attribute_handle: %u",
              attribute_handle_);
}

GattNotifyChannelInternal::~GattNotifyChannelInternal() {
  PW_LOG_INFO("btproxy: GattNotifyChannelInternal dtor - attribute_handle: %u",
              attribute_handle_);
}

}  // namespace pw::bluetooth::proxy::internal
