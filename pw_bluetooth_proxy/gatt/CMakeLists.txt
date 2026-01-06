# Copyright 2025 The Pigweed Authors
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

###############################################################################
##          Everything below here is intended to be emboss only              ##
##          and will be skipped if emboss isn't enabled.                     ##
###############################################################################
if("${dir_pw_third_party_emboss}" STREQUAL "")
  # Skip emboss defs if it's not configured
  return()
endif()


pw_add_library(pw_bluetooth_proxy.gatt STATIC
# LINT.IfChange
  HEADERS
    public/pw_bluetooth_proxy/gatt/gatt.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator
    pw_assert.check
    pw_bluetooth_proxy
    pw_containers.dynamic_vector
    pw_containers.intrusive_map
    pw_log
  PRIVATE_DEPS
    pw_bluetooth.emboss_att
    pw_bluetooth.emboss_util
  SOURCES
    gatt.cc
# LINT.ThenChange(Android.bp, gatt.bzl, BUILD.gn)
)

pw_add_library(pw_bluetooth_proxy.gatt._async_mbv2 STATIC
# LINT.IfChange
  HEADERS
    public/pw_bluetooth_proxy/gatt/gatt.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator
    pw_assert.check
    pw_bluetooth_proxy._async_mbv2
    pw_containers.dynamic_vector
    pw_containers.intrusive_map
    pw_log
  PRIVATE_DEPS
    pw_bluetooth.emboss_att
    pw_bluetooth.emboss_util
  SOURCES
    gatt.cc
# LINT.ThenChange(Android.bp, gatt.bzl, BUILD.gn)
)

pw_add_test(pw_bluetooth_proxy.gatt.gatt_test
# LINT.IfChange
  PRIVATE_DEPS
    pw_bluetooth_proxy.gatt
    pw_allocator.libc_allocator
    pw_allocator.testing
    pw_allocator.synchronized_allocator
    pw_containers.vector
    pw_multibuf.simple_allocator
    pw_sync.no_lock
  SOURCES
    gatt_test.cc
  GROUPS
    modules
# LINT.ThenChange(Android.bp, BUILD.bazel, BUILD.gn)
)

pw_add_test(pw_bluetooth_proxy.gatt.gatt_integration_test
# LINT.IfChange
  PRIVATE_DEPS
    pw_bluetooth_proxy.gatt._async_mbv2
    pw_allocator.testing
    pw_containers.vector
    pw_bluetooth_proxy._test_utils_async_mbv2
  SOURCES
    gatt_integration_test.cc
  GROUPS
    modules
# LINT.ThenChange(Android.bp, BUILD.bazel, BUILD.gn)
)
