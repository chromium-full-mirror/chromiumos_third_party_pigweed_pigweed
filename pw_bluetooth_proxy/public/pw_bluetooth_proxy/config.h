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

/// @module{pw_bluetooth_proxy}

/// Setting `PW_BLUETOOTH_PROXY_MULTIBUF` to this value builds the bt-proxy
/// library using MultiBuf v1.
#define PW_BLUETOOTH_PROXY_MULTIBUF_V1 10

/// Setting `PW_BLUETOOTH_PROXY_MULTIBUF` to this value builds the bt-proxy
/// library using MultiBuf v2.
#define PW_BLUETOOTH_PROXY_MULTIBUF_V2 20

#ifndef PW_BLUETOOTH_PROXY_MULTIBUF
/// Sets the version of MultiBuf used by pw_bluetooth_proxy.
#define PW_BLUETOOTH_PROXY_MULTIBUF PW_BLUETOOTH_PROXY_MULTIBUF_V1
#endif  // PW_BLUETOOTH_PROXY_MULTIBUF
