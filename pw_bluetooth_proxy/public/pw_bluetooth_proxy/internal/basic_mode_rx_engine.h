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

#include "pw_bluetooth_proxy/internal/rx_engine.h"

namespace pw::bluetooth::proxy::internal {

/// RxEngine that implements L2CAP Basic mode (B-Frame headers).
/// This class is not thread-safe and requires external synchronization.
class BasicModeRxEngine final : public RxEngine {
 public:
  // @param rx_multibuf_allocator The allocator to use when allocating SDU
  // multibufs. If nullptr, spans will be returned from HandlePduFromController.
  BasicModeRxEngine(uint16_t local_cid,
                    MultiBufAllocator* rx_multibuf_allocator)
      : rx_multibuf_allocator_(rx_multibuf_allocator), local_cid_(local_cid) {}

  BasicModeRxEngine(BasicModeRxEngine&& other) = default;
  BasicModeRxEngine& operator=(BasicModeRxEngine&& other) = default;

  HandlePduFromControllerReturnValue HandlePduFromController(
      pw::span<uint8_t> frame) override;

 private:
  MultiBufAllocator* rx_multibuf_allocator_;
  uint16_t local_cid_;
};

}  // namespace pw::bluetooth::proxy::internal
