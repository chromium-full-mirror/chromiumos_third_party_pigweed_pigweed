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

#include "pw_bluetooth_proxy/l2cap_coc.h"

namespace pw::bluetooth::proxy {

L2capCoc::L2capCoc(internal::L2capCocInternal&& internal)
    : internal_(std::move(internal)) {}

L2capCoc::L2capCoc(L2capCoc&& other) : internal_(std::move(other.internal_)) {}

void L2capCoc::StopForTesting() { return internal_.Stop(); }

void L2capCoc::CloseForTesting() { return internal_.Close(); }

StatusWithMultiBuf L2capCoc::Write(FlatConstMultiBuf&& payload) {
  return internal_.Write(std::move(payload));
}

Status L2capCoc::IsWriteAvailable() { return internal_.IsWriteAvailable(); }

pw::Status L2capCoc::SendAdditionalRxCredits(uint16_t additional_rx_credits) {
  return internal_.SendAdditionalRxCredits(additional_rx_credits);
}

L2capChannel::State L2capCoc::state() const { return internal_.state(); }

uint16_t L2capCoc::local_cid() const { return internal_.local_cid(); }

uint16_t L2capCoc::remote_cid() const { return internal_.remote_cid(); }

uint16_t L2capCoc::connection_handle() const {
  return internal_.connection_handle();
}

AclTransportType L2capCoc::transport() const { return internal_.transport(); }

}  // namespace pw::bluetooth::proxy
