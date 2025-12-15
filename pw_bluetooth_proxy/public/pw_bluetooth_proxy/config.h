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

#ifndef PW_BLUETOOTH_PROXY_ASYNC
/// Indicates whether the proxy should operate asynchronously or not.
///
/// When set, public API methods will be able to use `async2::Channel` to
/// communicate with tasks running on the dispatcher thread.
#define PW_BLUETOOTH_PROXY_ASYNC 0
#endif  // PW_BLUETOOTH_PROXY_ASYNC

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

#ifndef PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE
#if PW_BLUETOOTH_PROXY_ASYNC == 0

/// Sets the size of the internally provided memory region. If the proxy
/// container provides an allocator, the internal allocator is not used and this
/// should be set to 0.
///
/// TODO: https://pwbug.dev/369849508 - Fully migrate to container-provided
/// allocator and remove internal allocator.
#define PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE 13000

#else  // PW_BLUETOOTH_PROXY_ASYNC != 0

// No internal allocator when PW_BLUETOOTH_PROXY_ASYNC is enabled
#define PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE 0

#endif  // PW_BLUETOOTH_PROXY_ASYNC
#endif  // PW_BLUETOOTH_PROXY_INTERNAL_ALLOCATOR_SIZE
