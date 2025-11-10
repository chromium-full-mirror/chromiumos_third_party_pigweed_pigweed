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

#include "pw_bluetooth_proxy/basic_l2cap_channel.h"

namespace pw::bluetooth::proxy {

StatusWithMultiBuf BasicL2capChannel::Write(FlatConstMultiBuf&& payload) {
  return internal_.Write(std::move(payload));
}

Status BasicL2capChannel::IsWriteAvailable() {
  return internal_.IsWriteAvailable();
}

L2capChannel::State BasicL2capChannel::state() const {
  return internal_.state();
}

uint16_t BasicL2capChannel::local_cid() const { return internal_.local_cid(); }

uint16_t BasicL2capChannel::remote_cid() const {
  return internal_.remote_cid();
}

uint16_t BasicL2capChannel::connection_handle() const {
  return internal_.connection_handle();
}

AclTransportType BasicL2capChannel::transport() const {
  return internal_.transport();
}

void BasicL2capChannel::CloseForTesting() { return internal_.Close(); }

internal::BasicL2capChannelInternal& BasicL2capChannel::InternalForTesting() {
  return internal_;
}

BasicL2capChannel::BasicL2capChannel(
    internal::BasicL2capChannelInternal&& internal)
    : internal_(std::move(internal)) {}

}  // namespace pw::bluetooth::proxy
