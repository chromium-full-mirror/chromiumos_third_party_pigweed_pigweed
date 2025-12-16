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

add_subdirectory(gatt)

###############################################################################
##          Everything below here is intended to be emboss only              ##
##          and will be skipped if emboss isn't enabled.                     ##
###############################################################################
if("${dir_pw_third_party_emboss}" STREQUAL "")
  # Skip emboss defs if it's not configured
  return()
endif()

pw_add_library(pw_bluetooth_proxy STATIC

# LINT.IfChange
  HEADERS
    public/pw_bluetooth_proxy/basic_l2cap_channel.h
    public/pw_bluetooth_proxy/direction.h
    public/pw_bluetooth_proxy/gatt_notify_channel.h
    public/pw_bluetooth_proxy/h4_packet.h
    public/pw_bluetooth_proxy/internal/acl_data_channel.h
    public/pw_bluetooth_proxy/internal/basic_l2cap_channel_internal.h
    public/pw_bluetooth_proxy/internal/basic_mode_tx_engine.h
    public/pw_bluetooth_proxy/internal/basic_mode_rx_engine.h
    public/pw_bluetooth_proxy/internal/credit_based_flow_control_tx_engine.h
    public/pw_bluetooth_proxy/internal/credit_based_flow_control_rx_engine.h
    public/pw_bluetooth_proxy/internal/gatt_notify_channel_internal.h
    public/pw_bluetooth_proxy/internal/gatt_notify_rx_engine.h
    public/pw_bluetooth_proxy/internal/gatt_notify_tx_engine.h
    public/pw_bluetooth_proxy/internal/generic_l2cap_channel.h
    public/pw_bluetooth_proxy/internal/generic_l2cap_channel_sync.h
    public/pw_bluetooth_proxy/internal/generic_l2cap_channel_async.h
    public/pw_bluetooth_proxy/internal/hci_transport.h
    public/pw_bluetooth_proxy/internal/l2cap_channel_manager.h
    public/pw_bluetooth_proxy/internal/l2cap_channel_manager_sync.h
    public/pw_bluetooth_proxy/internal/l2cap_channel_manager_async.h
    public/pw_bluetooth_proxy/internal/l2cap_channel.h
    public/pw_bluetooth_proxy/internal/l2cap_channel_sync.h
    public/pw_bluetooth_proxy/internal/l2cap_channel_async.h
    public/pw_bluetooth_proxy/internal/l2cap_coc_internal.h
    public/pw_bluetooth_proxy/internal/l2cap_signaling_channel.h
    public/pw_bluetooth_proxy/internal/l2cap_status_tracker.h
    public/pw_bluetooth_proxy/internal/l2cap_logical_link.h
    public/pw_bluetooth_proxy/internal/locked_l2cap_channel.h
    public/pw_bluetooth_proxy/internal/logical_transport.h
    public/pw_bluetooth_proxy/internal/mutex.h
    public/pw_bluetooth_proxy/internal/proxy_host_sync.h
    public/pw_bluetooth_proxy/internal/proxy_host_async.h
    public/pw_bluetooth_proxy/internal/recombiner.h
    public/pw_bluetooth_proxy/internal/rx_engine.h
    public/pw_bluetooth_proxy/internal/tx_engine.h
    public/pw_bluetooth_proxy/l2cap_channel_common.h
    public/pw_bluetooth_proxy/l2cap_coc.h
    public/pw_bluetooth_proxy/l2cap_coc_config.h
    public/pw_bluetooth_proxy/l2cap_status_delegate.h
    public/pw_bluetooth_proxy/proxy_host.h
    public/pw_bluetooth_proxy/connection_handle.h
    public/pw_bluetooth_proxy/channel_proxy.h
    public/pw_bluetooth_proxy/l2cap_channel_manager_interface.h
# LINT.ThenChange(bt-proxy.bzl, BUILD.gn)

  PUBLIC_INCLUDES
    public

# LINT.IfChange

  PUBLIC_DEPS
    pw_allocator
    pw_allocator.best_fit
    pw_allocator.synchronized_allocator
    pw_async2.basic_dispatcher
    pw_async2.channel
    pw_async2.dispatcher
    pw_async2.poll
    pw_bluetooth.emboss_hci_data
    pw_bluetooth.emboss_hci_events
    pw_bluetooth.emboss_hci_h4
    pw_bluetooth.emboss_l2cap_frames
    pw_containers.flat_map
    pw_containers.inline_queue
    pw_containers.intrusive_map
    pw_containers.vector
    pw_function
    pw_bluetooth_proxy.multibuf
    pw_bluetooth_proxy.proxy_allocator
    pw_result
    pw_span
    pw_status
    pw_sync.lock_annotations
    pw_sync.mutex
    pw_sync.thread_notification
    pw_thread.id
  SOURCES
    acl_data_channel.cc
    basic_l2cap_channel.cc
    basic_l2cap_channel_internal.cc
    basic_mode_tx_engine.cc
    basic_mode_rx_engine.cc
    credit_based_flow_control_tx_engine.cc
    credit_based_flow_control_rx_engine.cc
    gatt_notify_channel.cc
    gatt_notify_channel_internal.cc
    gatt_notify_tx_engine.cc
    generic_l2cap_channel.cc
    generic_l2cap_channel_sync.cc
    l2cap_channel_manager.cc
    l2cap_channel_manager_sync.cc
    l2cap_channel.cc
    l2cap_channel_sync.cc
    l2cap_coc.cc
    l2cap_coc_internal.cc
    l2cap_signaling_channel.cc
    l2cap_status_tracker.cc
    l2cap_logical_link.cc
    proxy_host.cc
    proxy_host_sync.cc
    recombiner.cc
  PRIVATE_DEPS
    pw_containers.intrusive_map
    pw_allocator.best_fit
    pw_allocator.synchronized_allocator
    pw_bluetooth.emboss_att
    pw_bluetooth.emboss_hci_commands
    pw_bluetooth.emboss_util
    pw_containers.algorithm
    pw_log
    pw_span.cast

# LINT.ThenChange(Android.bp, bt-proxy.bzl, BUILD.gn)

)

# Module configuration

pw_add_module_config(pw_bluetooth_proxy_CONFIG)

pw_add_library(pw_bluetooth_proxy.config INTERFACE
  HEADERS
    public/pw_bluetooth_proxy/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_bluetooth_proxy_CONFIG}
)

# Libraries

pw_add_library(pw_bluetooth_proxy.multibuf STATIC

# LINT.IfChange
  HEADERS
    public/pw_bluetooth_proxy/internal/multibuf.h
  PUBLIC_DEPS
    pw_allocator
    pw_bluetooth_proxy.config
    pw_bytes
    pw_multibuf.allocator
    pw_multibuf.multibuf_v1
    pw_multibuf.multibuf_v2
  PRIVATE_DEPS
    pw_assert.check
    pw_span.cast
  SOURCES
    multibuf_v1.cc
    multibuf_v2.cc
# LINT.ThenChange(Android.bp, bt-proxy.bzl, BUILD.gn)

)

pw_add_library(pw_bluetooth_proxy.proxy_allocator INTERFACE

# LINT.IfChange
  HEADERS
    public/pw_bluetooth_proxy/internal/proxy_allocator.h
  PUBLIC_DEPS
    pw_allocator.best_fit
    pw_allocator.synchronized_allocator
    pw_assert
    pw_bluetooth_proxy.config
    pw_sync.mutex
# LINT.ThenChange(Android.bp, bt-proxy.bzl, BUILD.gn)

)

# Tests

pw_add_test(pw_bluetooth_proxy.pw_bluetooth_proxy_test

# LINT.IfChange
  PRIVATE_DEPS
    pw_allocator.libc_allocator
    pw_allocator.null_allocator
    pw_allocator.synchronized_allocator
    pw_allocator.testing
    pw_assert.check
    pw_bluetooth_proxy
    pw_bluetooth.emboss_att
    pw_bluetooth.emboss_hci_commands
    pw_bluetooth.emboss_hci_common
    pw_bluetooth.emboss_hci_events
    pw_bluetooth.emboss_hci_h4
    pw_bluetooth.emboss_util
    pw_containers.to_array
    pw_multibuf.simple_allocator
    pw_span.cast
    pw_sync.mutex
    pw_sync.thread_notification
    pw_third_party.fuchsia.stdcompat
    pw_thread.test_thread_context
    pw_thread.thread
    pw_unit_test
  SOURCES
    pw_bluetooth_proxy_private/test_utils.h
    basic_mode_tx_engine_test.cc
    basic_mode_rx_engine_test.cc
    channel_proxy_test.cc
    credit_based_flow_control_tx_engine_test.cc
    credit_based_flow_control_rx_engine_test.cc
    gatt_notify_test.cc
    gatt_notify_tx_engine_test.cc
    h4_packet_test.cc
    l2cap_coc_test.cc
    proxy_host_test.cc
    recombiner_test.cc
    test_utils.cc
    test_utils_async.cc
    test_utils_sync.cc
    utils_test.cc
# LINT.ThenChange(bt-proxy.bzl, BUILD.gn)

  GROUPS
    modules
)
