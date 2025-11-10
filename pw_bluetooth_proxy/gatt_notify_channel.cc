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

#include "pw_bluetooth_proxy/gatt_notify_channel.h"

namespace pw::bluetooth::proxy {
StatusWithMultiBuf GattNotifyChannel::Write(FlatConstMultiBuf&& payload) {
  return internal_.Write(std::move(payload));
}

Status GattNotifyChannel::IsWriteAvailable() {
  return internal_.IsWriteAvailable();
}

uint16_t GattNotifyChannel::attribute_handle() const {
  return internal_.attribute_handle();
}

L2capChannel::State GattNotifyChannel::state() const {
  return internal_.state();
}

uint16_t GattNotifyChannel::local_cid() const { return internal_.local_cid(); }

uint16_t GattNotifyChannel::remote_cid() const {
  return internal_.remote_cid();
}

uint16_t GattNotifyChannel::connection_handle() const {
  return internal_.connection_handle();
}

AclTransportType GattNotifyChannel::transport() const {
  return internal_.transport();
}

GattNotifyChannel::GattNotifyChannel(
    internal::GattNotifyChannelInternal&& internal)
    : internal_(std::move(internal)) {}

}  // namespace pw::bluetooth::proxy
