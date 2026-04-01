// Copyright 2026 The Pigweed Authors
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

// DOCSTAG: [pw_rpc-examples-blinky]
#include "pw_rpc/examples/blinky.rpc.pb.h"
#include "pw_system/rpc_server.h"

namespace blinky {

class BlinkyService final
    : public blinky::pw_rpc::nanopb::Blinky::Service<BlinkyService> {
 public:
  pw::Status ToggleLed(const pw_protobuf_Empty&, pw_protobuf_Empty&) {
    // Turn the LED off if it's currently on and vice versa
    return pw::OkStatus();
  }

  pw::Status Blink(const blinky_BlinkRequest& request, pw_protobuf_Empty&) {
    if (request.blink_count == 0) {
      // Stop blinking
    }
    if (request.interval_ms > 0) {
      // Change the blink interval
    }
    if (request.has_blink_count) {
      // Blink request.blink_count times
    }
    return pw::OkStatus();
  }
};

BlinkyService blinky_service;

}  // namespace blinky

namespace pw::system {

void UserAppInit() {
  pw::system::GetRpcServer().RegisterService(blinky::blinky_service);
}

}  // namespace pw::system
// DOCSTAG: [pw_rpc-examples-blinky]
