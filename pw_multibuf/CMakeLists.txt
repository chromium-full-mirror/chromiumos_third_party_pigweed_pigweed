# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_multibuf/backend.cmake)

# LINT.IfChange
pw_add_facade(pw_multibuf INTERFACE
  BACKEND
    pw_multibuf_BACKEND
  HEADERS
    public/pw_multibuf/allocator.h
    public/pw_multibuf/chunk.h
    public/pw_multibuf/from_span.h
    public/pw_multibuf/header_chunk_region_tracker.h
    public/pw_multibuf/multibuf.h
    public/pw_multibuf/observer.h
    public/pw_multibuf/simple_allocator.h
    public/pw_multibuf/single_chunk_region_tracker.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_multibuf.config
)

# allocator_async is kept as a separate facade to keep its dep on pw_thread via
# pw_async2 from propagating to incompatible target platforms, e.g. fuchsia,
# stm32xx, etc.
pw_add_facade(pw_multibuf.allocator_async INTERFACE
  BACKEND
    pw_multibuf_allocator_async_BACKEND
  HEADERS
    public/pw_multibuf/allocator_async.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_multibuf.allocator
    pw_multibuf.config
)
# LINT.ThenChange(BUILD.bazel, BUILD.gn)

### Module configuration

pw_add_module_config(pw_multibuf_CONFIG)

pw_add_library(pw_multibuf.config INTERFACE
  HEADERS
    public/pw_multibuf/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_multibuf_CONFIG}
)

### V1 Libraries

pw_add_library(pw_multibuf.allocator INTERFACE
  PUBLIC_DEPS
    pw_multibuf
)

pw_add_library(pw_multibuf.chunk INTERFACE
  PUBLIC_DEPS
    pw_multibuf
)

pw_add_library(pw_multibuf.from_span INTERFACE
  PUBLIC_DEPS
    pw_multibuf
)

pw_add_library(pw_multibuf.header_chunk_region_tracker INTERFACE
  PUBLIC_DEPS
    pw_multibuf
)

pw_add_library(pw_multibuf.simple_allocator INTERFACE
  PUBLIC_DEPS
    pw_multibuf
)

pw_add_library(pw_multibuf.single_chunk_region_tracker INTERFACE
  PUBLIC_DEPS
    pw_multibuf
)

# LINT.IfChange
pw_add_library(pw_multibuf.testing INTERFACE
  HEADERS
    public/pw_multibuf/simple_allocator_for_test.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.synchronized_allocator
    pw_allocator.testing
    pw_assert.assert
    pw_multibuf.config
    pw_multibuf.simple_allocator
    pw_sync.mutex
)

pw_add_library(pw_multibuf._chunk_testing INTERFACE
  HEADERS
    private/pw_multibuf_private/chunk_test_base.h
  PUBLIC_INCLUDES
    private
  PUBLIC_DEPS
    pw_unit_test
)

pw_add_library(pw_multibuf._test_utils INTERFACE
  HEADERS
    private/pw_multibuf_private/test_utils.h
  PUBLIC_INCLUDES
    private
)

# LINT.ThenChange(BUILD.bazel, BUILD.gn)

add_subdirectory(v1)
add_subdirectory(v1_adapter)
add_subdirectory(v2)
