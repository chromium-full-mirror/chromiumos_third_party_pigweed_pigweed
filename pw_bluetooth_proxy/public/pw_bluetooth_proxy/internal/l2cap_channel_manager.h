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

#pragma once

#include <optional>

#include "pw_allocator/best_fit.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_bluetooth_proxy/basic_l2cap_channel.h"
#include "pw_bluetooth_proxy/gatt_notify_channel.h"
#include "pw_bluetooth_proxy/internal/acl_data_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_logical_link.h"
#include "pw_bluetooth_proxy/internal/l2cap_status_tracker.h"
#include "pw_bluetooth_proxy/internal/locked_l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/proxy_allocator.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_bluetooth_proxy/l2cap_coc.h"
#include "pw_containers/intrusive_map.h"
#include "pw_function/function.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_sync/thread_notification.h"

// This include is not used but a downstream project transitively depends on it.
#include "pw_containers/flat_map.h"

namespace pw::bluetooth::proxy {

class L2capChannelManager;

// `L2capChannelManager` mediates between `ProxyHost` and the L2CAP-based
// channels held by clients of `ProxyHost`, such as L2CAP connection-oriented
// channels, GATT Notify channels, and RFCOMM channels.
//
// ACL packet transmission is subject to data control flow, managed by
// `AclDataChannel`. `L2capChannelManager` handles queueing Tx packets when
// credits are unavailable and sending Tx packets as credits become available,
// dequeueing packets in FIFO order per channel and in round robin fashion
// around channels.
class L2capChannelManager {
 public:
  /// @param[in] allocator - General purpose allocator to use for internal
  /// packet buffers and objects. If null, an internal allocator and memory pool
  /// will be used.
  L2capChannelManager(AclDataChannel& acl_data_channel,
                      pw::Allocator* allocator);

  ~L2capChannelManager();

  pw::Result<L2capCoc> AcquireL2capCoc(
      MultiBufAllocator& rx_multibuf_allocator,
      uint16_t connection_handle,
      ConnectionOrientedChannelConfig rx_config,
      ConnectionOrientedChannelConfig tx_config,
      Function<void(FlatConstMultiBuf&& payload)>&& receive_fn,
      ChannelEventCallback&& event_fn)
      PW_LOCKS_EXCLUDED(links_mutex_, channels_mutex_);

  pw::Result<BasicL2capChannel> AcquireBasicL2capChannel(
      MultiBufAllocator& rx_multibuf_allocator,
      uint16_t connection_handle,
      uint16_t local_cid,
      uint16_t remote_cid,
      AclTransportType transport,
      OptionalPayloadReceiveCallback&& payload_from_controller_fn,
      OptionalPayloadReceiveCallback&& payload_from_host_fn,
      ChannelEventCallback&& event_fn)
      PW_LOCKS_EXCLUDED(links_mutex_, channels_mutex_);

  pw::Result<GattNotifyChannel> AcquireGattNotifyChannel(
      uint16_t connection_handle,
      uint16_t attribute_handle,
      ChannelEventCallback&& event_fn)
      PW_LOCKS_EXCLUDED(links_mutex_, channels_mutex_);

  // Start proxying L2CAP packets addressed to `channel` arriving from
  // the controller and allow `channel` to send & queue Tx L2CAP packets.
  void RegisterChannel(L2capChannel& channel)
      PW_LOCKS_EXCLUDED(channels_mutex_);

  // Stop proxying L2CAP packets addressed to `channel` and stop sending L2CAP
  // packets queued in `channel`, if `channel` is currently registered.
  void DeregisterChannel(L2capChannel& channel)
      PW_LOCKS_EXCLUDED(channels_mutex_);

  // Deregister and close all channels then propagate `event` to clients.
  void DeregisterAndCloseChannels(L2capChannelEvent event)
      PW_LOCKS_EXCLUDED(links_mutex_, channels_mutex_);

  // Get an `H4PacketWithH4` backed by a buffer able to hold `size` bytes of
  // data.
  //
  // Returns PW_STATUS_UNAVAILABLE if a buffer could not be allocated.
  pw::Result<H4PacketWithH4> GetAclH4Packet(uint16_t size);

  // Report that new tx packets have been queued or new tx credits have been
  // received since the last DrainChannelQueuesIfNewTx.
  void ReportNewTxPacketsOrCredits() PW_LOCKS_EXCLUDED(drain_status_mutex_);

  // Send L2CAP packets queued in registered channels. Since this function takes
  // the channels_mutex_ lock, it can't be directly called while handling a
  // received packet on a channel. Instead call ReportPacketsMayBeReadyToSend().
  // Rx processing will then call this function when complete.
  void DrainChannelQueuesIfNewTx()
      PW_LOCKS_EXCLUDED(channels_mutex_, drain_status_mutex_);

  // Drain channel queues even if no channel explicitly requested it. Should be
  // used for events triggering queue space at the ACL level.
  void ForceDrainChannelQueues() PW_LOCKS_EXCLUDED(channels_mutex_);

  std::optional<LockedL2capChannel> FindChannelByLocalCid(
      uint16_t connection_handle, uint16_t local_cid)
      PW_LOCKS_EXCLUDED(channels_mutex_);

  std::optional<LockedL2capChannel> FindChannelByRemoteCid(
      uint16_t connection_handle, uint16_t remote_cid)
      PW_LOCKS_EXCLUDED(channels_mutex_);

  L2capChannel* FindChannelByLocalCidLocked(uint16_t connection_handle,
                                            uint16_t local_cid)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  L2capChannel* FindChannelByRemoteCidLocked(uint16_t connection_handle,
                                             uint16_t remote_cid)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // Register for notifications of connection and disconnection for a
  // particular L2cap service identified by its PSM.
  void RegisterStatusDelegate(L2capStatusDelegate& delegate);

  // Unregister a service delegate.
  void UnregisterStatusDelegate(L2capStatusDelegate& delegate);

  // Called when a l2cap channel connection successfully made.
  void HandleConnectionComplete(const L2capChannelConnectionInfo& info);

  // Called when a l2cap channel configuration is done.
  void HandleConfigurationChanged(const L2capChannelConfigurationInfo& info);

  // Called when an ACL connection is disconnected.
  void HandleAclDisconnectionComplete(uint16_t connection_handle)
      PW_LOCKS_EXCLUDED(channels_mutex_, links_mutex_);

  // Called when a l2cap channel connection is disconnected.
  //
  // Must be called under channels_mutex_ but we can't use proper lock
  // annotation here since the call comes via signaling channel.
  // TODO: https://pwbug.dev/390511432 - Figure out way to add annotations to
  // enforce this invariant.
  void HandleDisconnectionCompleteLocked(
      const L2capStatusTracker::DisconnectParams& params);

  // Deliver any pending connection events. Should not be called while holding
  // channels_mutex_.
  void DeliverPendingEvents() PW_LOCKS_EXCLUDED(channels_mutex_);

  // Register a logical link with the L2CAP layer.
  //
  // @returns
  // * @OK: The link was successfully added.
  // * @ALREADY_EXISTS: The link is already registered.
  // * @RESOURCE_EXHAUSTED: There is no memory left to allocate state for the
  // link.
  Status AddConnection(uint16_t connection_handle, AclTransportType transport);

  // Send L2CAP_FLOW_CONTROL_CREDIT_IND to indicate local endpoint `cid` is
  // capable of receiving a number of additional K-frames (`credits`).
  //
  // @returns
  // * @OK: `L2CAP_FLOW_CONTROL_CREDIT_IND` was sent.
  // * @UNAVAILABLE: Send could not be queued due to lack of memory in the
  //   client-provided `multibuf_allocator` (transient error).
  // * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  Status SendFlowControlCreditInd(uint16_t connection_handle,
                                  uint16_t channel_id,
                                  uint16_t credits,
                                  MultiBufAllocator& multibuf_allocator);

  // Returns the max ACL payload size if the Read Buffer Size command complete
  // event was received.
  std::optional<uint16_t> MaxDataPacketLengthForTransport(
      AclTransportType transport) const {
    return acl_data_channel_.MaxDataPacketLengthForTransport(transport);
  }

 private:
  using L2capChannelPredicate = pw::Function<bool(L2capChannel&)>;
  using L2capChannelIterator =
      IntrusiveMap<uint32_t, L2capChannel::Handle>::iterator;

  // Circularly advance `it`, wrapping around to front if `it` reaches the
  // end.
  void Advance(L2capChannelIterator& it)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // RegisterChannel with channels_mutex_ already acquired.
  //
  // @returns
  // * @OK: Channel was registered.
  // * @ALREADY_EXISTS: An active channel with the same connection handle, local
  // CID, and remote CID is already registered.
  Status RegisterChannelLocked(L2capChannel& channel)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // Stop proxying L2CAP packets addressed to `channel` and stop sending L2CAP
  // packets queued in `channel`, if `channel` is currently registered.
  void DeregisterChannelLocked(L2capChannel& channel)
      PW_EXCLUSIVE_LOCKS_REQUIRED(channels_mutex_);

  // Deletes any channels that have been observed to be stale.
  void DeleteStaleChannels() PW_LOCKS_EXCLUDED(channels_mutex_);

  void ResetLogicalLinksLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(links_mutex_);

  // Reference to the ACL data channel owned by the proxy.
  AclDataChannel& acl_data_channel_;

  // Allocator for links and channels.
  internal::ProxyAllocator allocator_;

  // Enforce mutual exclusion of all operations on channels.
  // This is ACQUIRED_BEFORE AclDataChannel::credit_mutex_ which is annotated
  // on that member variable.
  sync::Mutex channels_mutex_;

  // List of registered L2CAP channels.
  IntrusiveMap<uint32_t, L2capChannel::Handle> channels_by_local_cid_
      PW_GUARDED_BY(channels_mutex_);
  IntrusiveMap<uint32_t, L2capChannel::Handle> channels_by_remote_cid_
      PW_GUARDED_BY(channels_mutex_);

  // Stale L2CAP channels awaiting deletion.
  IntrusiveMap<uint32_t, L2capChannel::Handle> stale_
      PW_GUARDED_BY(channels_mutex_);

  // Iterator to "least recently drained" channel.
  L2capChannelIterator lrd_channel_ PW_GUARDED_BY(channels_mutex_);

  // Iterator to final channel to be visited in ongoing round robin.
  L2capChannelIterator round_robin_terminus_ PW_GUARDED_BY(channels_mutex_);

  // Guard access to tx related state flags.
  sync::Mutex drain_status_mutex_ PW_ACQUIRED_AFTER(channels_mutex_);

  // True if new tx packets are queued or new tx resources have become
  // available.
  bool drain_needed_ PW_GUARDED_BY(drain_status_mutex_) = false;

  // True if a drain is running.
  bool drain_running_ PW_GUARDED_BY(drain_status_mutex_) = false;

  // Channel connection status tracker and delegate holder.
  L2capStatusTracker status_tracker_;

  // A separate links mutex is required so that the channels owned by the links
  // can be destroyed without deadlock.
  sync::Mutex links_mutex_ PW_ACQUIRED_BEFORE(channels_mutex_);
  IntrusiveMap<uint16_t, internal::L2capLogicalLinkInterface> logical_links_
      PW_GUARDED_BY(links_mutex_);
};

}  // namespace pw::bluetooth::proxy
