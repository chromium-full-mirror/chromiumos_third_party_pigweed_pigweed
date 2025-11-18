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

#include "pw_bluetooth_proxy/internal/basic_mode_rx_engine.h"

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy::internal {

RxEngine::HandlePduFromControllerReturnValue
BasicModeRxEngine::HandlePduFromController(pw::span<uint8_t> frame) {
  Result<emboss::BFrameWriter> bframe_view =
      MakeEmbossWriter<emboss::BFrameWriter>(frame);

  if (!bframe_view.ok()) {
    // TODO: https://pwbug.dev/360929142 - Stop channel on error.
    PW_LOG_ERROR("(CID: 0x%X) Received invalid B-frame. So will drop.",
                 local_cid_);
    return std::monostate();
  }

  pw::span payload(bframe_view->payload().BackingStorage().data(),
                   bframe_view->payload().SizeInBytes());

  if (rx_multibuf_allocator_ == nullptr) {
    return payload;
  }

  std::optional<FlatMultiBufInstance> multibuf =
      MultiBufAdapter::Create(*rx_multibuf_allocator_, payload.size());
  if (!multibuf) {
    PW_LOG_ERROR(
        "(CID %#x) Rx MultiBuf allocator out of memory. So stopping channel "
        "and reporting it needs to be closed.",
        local_cid_);
    return L2capChannelEvent::kRxOutOfMemory;
  }
  MultiBufAdapter::Copy(
      multibuf.value(), /*dst_offset=*/0, pw::as_bytes(payload));
  return std::move(multibuf.value());
}

}  // namespace pw::bluetooth::proxy::internal
