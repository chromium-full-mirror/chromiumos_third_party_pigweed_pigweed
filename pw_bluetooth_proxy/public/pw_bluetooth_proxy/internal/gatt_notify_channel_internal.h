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

#include <cstdint>

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"

namespace pw::bluetooth::proxy::internal {

/// `GattNotifyChannelInternal` supports sending GATT characteristic
/// notifications to a remote peer.
class GattNotifyChannelInternal final : public L2capChannel {
 public:
  [[nodiscard]] static bool AreValidParameters(uint16_t connection_handle,
                                               uint16_t attribute_handle);

  explicit GattNotifyChannelInternal(L2capChannelManager& l2cap_channel_manager,
                                     uint16_t connection_handle,
                                     uint16_t attribute_handle,
                                     ChannelEventCallback&& event_fn);

  // Internal channels are not copyable or movable.
  GattNotifyChannelInternal(const GattNotifyChannelInternal& other) = delete;
  GattNotifyChannelInternal& operator=(const GattNotifyChannelInternal& other) =
      delete;

  ~GattNotifyChannelInternal() override;

  /// Return the attribute handle of this GattNotify channel.
  uint16_t attribute_handle() const { return attribute_handle_; }

 private:
  bool DoHandlePduFromController(pw::span<uint8_t>) override {
    // Forward all packets to host.
    return false;
  }

  bool HandlePduFromHost(pw::span<uint8_t>) override {
    // Forward all packets to controller.
    return false;
  }

  std::optional<H4PacketWithH4> GenerateNextTxPacket(
      const FlatConstMultiBuf& attribute_value, bool& keep_payload) override;

  uint16_t attribute_handle_;
};

}  // namespace pw::bluetooth::proxy::internal
