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

#include <cstdint>

#include "pw_assert/assert.h"
#include "pw_bluetooth_proxy/direction.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_containers/inline_queue.h"
#include "pw_containers/intrusive_map.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_sync/thread_notification.h"

namespace pw::bluetooth::proxy {

class L2capChannel;
class L2capChannelManager;

namespace internal {

class GenericL2capChannel;

/// RAII helper type that guarantees an L2capChannel will not be destroyed as
/// long as an instance of this type remains in scope.
class BorrowedL2capChannel {
 public:
  constexpr BorrowedL2capChannel() = default;

  BorrowedL2capChannel(BorrowedL2capChannel&& other) {
    *this = std::move(other);
  }
  BorrowedL2capChannel& operator=(BorrowedL2capChannel&& other);

  ~BorrowedL2capChannel();

  L2capChannel* operator->() { return channel_; }
  L2capChannel& operator*() { return *channel_; }

 private:
  friend class GenericL2capChannel;

  BorrowedL2capChannel(L2capChannel& channel);

  L2capChannel* channel_ = nullptr;
};

}  // namespace internal

// Base class for peer-to-peer L2CAP-based channels.
//
// Protocol-dependent information that is fixed per channel, such as addresses,
// flags, handles, etc. should be provided at construction to derived channels.
class L2capChannel {
 public:
  static constexpr uint16_t kMaxValidConnectionHandle = 0x0EFF;

  enum class State {
    // Channel is new.
    kNew,
    kRunning,
    // Channel is stopped, but the L2CAP connection has not been closed.
    kStopped,
    // L2CAP connection has been closed as the result of an
    // HCI_Disconnection_Complete event, L2CAP_DISCONNECTION_RSP packet, or
    // HCI_Reset Command packet; or `ProxyHost` dtor has been called.
    kClosed,
  };

  virtual ~L2capChannel();

  // Complete initialization and registration of the channel. Must be called
  // after ctor for channel to be active.
  void Init();

  //-------------
  //  Status (internal public)
  //-------------

  // Helper since these operations should typically be coupled.
  void StopAndSendEvent(L2capChannelEvent event) {
    Stop();
    SendEvent(event);
  }

  // Enter `State::kStopped`. This means
  //   - Queue is cleared so pending sends will not complete.
  //   - Calls to `QueuePacket()` will return PW_STATUS_FAILED_PRECONDITION, so
  //     derived channels should not accept client writes.
  //   - Rx packets will be dropped & trigger `kRxWhileStopped` events.
  //   - Container is responsible for closing L2CAP connection & destructing
  //     the channel object to free its resources.
  void Stop();

  //-------------
  //  Tx (public)
  //-------------

  // TODO: https://pwbug.dev/388082771 - Most of these APIs should be in a
  // public header. Will probably do that as part of moving them to
  // ClientChannel.

  /// Send a payload to the remote peer.
  ///
  /// @param[in] payload The client payload to be sent. Payload will be
  /// destroyed once its data has been used.
  ///
  /// @returns A `StatusWithMultiBuf` with one of the statuses below. If status
  /// is not @OK then payload is also returned in `StatusWithMultiBuf`.
  /// * @OK: Packet was successfully queued for send.
  /// * @UNAVAILABLE: Channel could not acquire the resources to queue
  ///   the send at this time (transient error). If an `event_fn` has been
  ///   provided it will be called with `L2capChannelEvent::kWriteAvailable`
  ///   when there is queue space available again.
  /// * @INVALID_ARGUMENT: Payload is too large.
  /// * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  /// * @UNIMPLEMENTED: Channel does not support `Write(MultiBuf)`.
  // TODO: https://pwbug.dev/388082771 - Plan to eventually move this to
  // ClientChannel.
  StatusWithMultiBuf Write(FlatConstMultiBuf&& payload);

  /// Channels that need to send a payload during handling a received packet
  /// directly (for instance to replenish credits) should use this function
  /// which does not take the L2capChannelManager channels lock.
  StatusWithMultiBuf WriteDuringRx(FlatConstMultiBuf&& payload);

  /// Determine if channel is ready to accept one or more Write payloads.
  ///
  /// @returns
  /// * @OK: Channel is ready to accept one or more `Write` payloads.
  /// * @UNAVAILABLE: Channel does not yet have the resources to queue a Write
  ///   at this time (transient error). If an `event_fn` has been provided it
  ///   will be called with `L2capChannelEvent::kWriteAvailable` when there is
  ///   queue space available again.
  /// * @FAILED_PRECONDITION: Channel is not `State::kRunning`.
  Status IsWriteAvailable();

  // Dequeue a packet if one is available to send.
  [[nodiscard]] virtual std::optional<H4PacketWithH4> DequeuePacket();

  // Max number of Tx L2CAP packets that can be waiting to send.
  static constexpr size_t QueueCapacity() { return kQueueCapacity; }

  // Returns the maximum size supported for Tx L2CAP PDU payloads with a Basic
  // header.
  //
  // Returns std::nullopt if the ACL data packet length is not yet known.
  std::optional<uint16_t> MaxL2capPayloadSize() const;

  //-------------
  //  Rx (public)
  //-------------

  // Handle a Tx L2CAP PDU.
  //
  // Implementations should call `SendPayloadFromHostToClient` after processing
  // the PDU.
  //
  // Return true if the PDU was consumed by the channel. Otherwise, return false
  // and the PDU will be forwarded by `ProxyHost` on to the Bluetooth
  // controller.
  [[nodiscard]] virtual bool HandlePduFromHost(pw::span<uint8_t> l2cap_pdu) = 0;

  // Called when an L2CAP PDU is received on this channel. If channel is
  // `kRunning`, returns `HandlePduFromController(l2cap_pdu)`. If channel is not
  // `State::kRunning`, sends `kRxWhileStopped` event to client and drops PDU.
  // This function will call DoHandlePduFromController on its subclass.
  [[nodiscard]] bool HandlePduFromController(pw::span<uint8_t> l2cap_pdu);

  //--------------
  //  Accessors:
  //--------------

  State state() const;

  constexpr uint16_t local_cid() const { return local_handle_.channel_id(); }

  constexpr uint16_t remote_cid() const { return remote_handle_.channel_id(); }

  constexpr uint16_t connection_handle() const {
    return local_handle_.connection_handle();
  }

  constexpr AclTransportType transport() const { return transport_; }

 protected:
  friend class internal::GenericL2capChannel;
  friend class L2capChannelManager;

  //----------------------
  //  Creation (protected)
  //----------------------

  explicit L2capChannel(
      L2capChannelManager& l2cap_channel_manager,
      MultiBufAllocator* rx_multibuf_allocator,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      OptionalPayloadReceiveCallback&& payload_from_controller_fn,
      OptionalPayloadReceiveCallback&& payload_from_host_fn,
      ChannelEventCallback&& event_fn);

  // Returns whether or not ACL connection handle & L2CAP channel identifiers
  // are valid parameters for a packet.
  [[nodiscard]] static bool AreValidParameters(uint16_t connection_handle,
                                               uint16_t local_cid,
                                               uint16_t remote_cid);

  //-------------------
  //  Other (protected)
  //-------------------

  // Send `event` to client if an event callback was provided.
  void SendEvent(L2capChannelEvent event) PW_LOCKS_EXCLUDED(static_mutex_);

  // Enter `State::kClosed` and disconnects the client. This has all the same
  // effects as stopping the channel and triggers `event`. No-op if channel is
  // already `State::kClosed`.
  void Close(L2capChannelEvent event = L2capChannelEvent::kChannelClosedByOther)
      PW_LOCKS_EXCLUDED(static_mutex_);

  L2capChannelManager& channel_manager() const {
    return l2cap_channel_manager_;
  }

  //----------------
  //  Tx (protected)
  //----------------

  // Reserve an L2CAP over ACL over H4 packet, with those three headers
  // populated for an L2CAP PDU payload of `data_length` bytes addressed to
  // `connection_handle_`.
  //
  // Returns PW_STATUS_INVALID_ARGUMENT if payload is too large for a buffer.
  // Returns PW_STATUS_UNAVAILABLE if all buffers are currently occupied.
  pw::Result<H4PacketWithH4> PopulateTxL2capPacket(uint16_t data_length);

  // Alert `L2capChannelManager` that queued packets may be ready to send.
  void ReportNewTxPacketsOrCredits();

  // Tell `L2capChannelManager` to try and send all available queued
  // packets. When calling this method, ensure no locks are held that are
  // also acquired in `Dequeue()` overrides, and that the channels lock is
  // not held either.
  void DrainChannelQueuesIfNewTx() PW_LOCKS_EXCLUDED(mutex_);

  // Remove all packets from queue.
  void ClearQueue() PW_LOCKS_EXCLUDED(mutex_);

  //-------
  //  Rx (protected)
  //-------

  // Returns false if payload should be forwarded to controller instead.
  // Allows client to modify the payload to be forwarded.
  virtual bool SendPayloadFromHostToClient(pw::span<uint8_t> payload);

  // Returns false if payload should be forwarded to host instead.
  // Allows client to modify the payload to be forwarded.
  virtual bool SendPayloadFromControllerToClient(pw::span<uint8_t> payload);

  constexpr MultiBufAllocator* rx_multibuf_allocator() const {
    return rx_multibuf_allocator_;
  }

 private:
  friend class internal::BorrowedL2capChannel;

  // TODO: https://pwbug.dev/349700888 - Make capacity configurable.
  static constexpr size_t kQueueCapacity = 5;

  // Returns false if payload should be forwarded to host instead.
  // Allows client to modify the payload to be forwarded.
  bool SendPayloadToClient(pw::span<uint8_t> payload,
                           const OptionalPayloadReceiveCallback& callback);

  // Reserve an L2CAP packet over ACL over H4 packet.
  pw::Result<H4PacketWithH4> PopulateL2capPacket(uint16_t data_length);

  //--------------
  //  Tx (private)
  //--------------

  // Return the next Tx H4 based on the client's queued payloads. If the
  // returned PDU will complete the transmission of a payload, that payload
  // should be popped from the queue. If no payloads are queued, return
  // std::nullopt.
  // Subclasses should override to generate correct H4 packet from their
  // payload.
  virtual std::optional<H4PacketWithH4> GenerateNextTxPacket(
      const FlatConstMultiBuf& attribute_value, bool& keep_payload) = 0;

  // Stores client Tx payload buffers.
  InlineQueue<FlatConstMultiBufInstance, kQueueCapacity> payload_queue_
      PW_GUARDED_BY(mutex_);

  // True if the last queue attempt didn't have space. Will be cleared on
  // successful dequeue.
  bool notify_on_dequeue_ PW_GUARDED_BY(mutex_) = false;

  //--------------
  //  Rx (private)
  //--------------

  // Handle an Rx L2CAP PDU.
  //
  // Implementations should call `SendPayloadFromControllerToClient` after
  // recombining/processing the PDU (e.g. after updating channel state and
  // screening out certain PDUs).
  //
  // Return true if the PDU was consumed by the channel. Otherwise, return false
  // and the PDU will be forwarded by `ProxyHost` on to the Bluetooth host.
  [[nodiscard]] virtual bool DoHandlePduFromController(
      pw::span<uint8_t> l2cap_pdu) = 0;

  //-------------
  //  Rx recombine - private, used by friend Recombiner only
  //-------------
  // TODO: https://pwbug.dev/404094475 - This section (and underlying data
  // member) could eventually move up into a AclChannel class.

  // The Rx combine functions private and only used by friend Recombiner.
  friend class Recombiner;

  // Create MultiBuf that Recombiner can use to store in-progress payload when
  // being recombined.
  // Allocate extra_header_size at front of the multibuf for possible use in
  // headers if needed. That extra is discarded so it will not be part of
  // initial MultiBuf view returned by TakeBuf.
  pw::Status StartRecombinationBuf(Direction direction,
                                   size_t payload_size,
                                   size_t extra_header_size);

  // Returns true if this channel has a recombination MultiBuf (which means
  // recombination is active for this channel).
  bool HasRecombinationBuf(Direction direction) {
    return GetRecombinationBufOptRef(direction).has_value();
  }

  // Return the recombination buf to the caller.
  //
  // Channel no longer has buf after this call.
  MultiBufInstance TakeRecombinationBuf(Direction direction) {
    PW_ASSERT(GetRecombinationBufOptRef(direction).has_value());
    return std::exchange(GetRecombinationBufOptRef(direction), std::nullopt)
        .value();
  }

  // Copy the passed span to the recombination buf.
  //
  // Precondition: `HasCombinationBuf()`.
  pw::Status CopyToRecombinationBuf(Direction direction,
                                    ConstByteSpan data,
                                    uint16_t write_offset) {
    PW_ASSERT(HasRecombinationBuf(direction));
    auto& bufopt_ref = GetRecombinationBufOptRef(direction);
    MultiBuf& mbuf = MultiBufAdapter::Unwrap(bufopt_ref.value());
    size_t copied = MultiBufAdapter::Copy(mbuf, write_offset, as_bytes(data));
    return copied < data.size() ? Status::ResourceExhausted() : OkStatus();
  }

  // Return reference to the recombination MultiBuf.
  //
  // Intended for use just within L2capChannel functions.
  std::optional<MultiBufInstance>& GetRecombinationBufOptRef(
      Direction direction) {
    return recombination_mbufs_[cpp23::to_underlying(direction)];
  }

  // Destroy the recombination MultiBuf.
  void EndRecombinationBuf(Direction direction);

  //-------------
  // Link - private, used by friends only
  //-------------

  // Static lock used to guard channel registrations by the channel manager and
  // disconnections by the client channels.
  static constexpr sync::Mutex& mutex() PW_LOCK_RETURNED(static_mutex_) {
    return static_mutex_;
  }

  // Returns whether this object is stale, that is, whether it is an acquired
  // channel whose client has disconnected.
  [[nodiscard]] bool IsStale() const PW_LOCKS_EXCLUDED(static_mutex_);

  // Returns whether this object has no more pending payloads.
  [[nodiscard]] bool PayloadQueueEmpty() const PW_LOCKS_EXCLUDED(static_mutex_);

  // Like `Close`, but requires holding the lock and does not send an event to
  // the clients.
  void CloseLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(static_mutex_);

  //-------------
  // Handles and keys for maps
  //-------------

  // Mappable handle to the internal channel.
  //
  // An intrusively mapped item can only be in one map at a time. Since channels
  // are mapped by keys derived from both local and remote CIDs, the channels
  // are not mappable items theselves, but have a handle for each map.
  class Handle : public IntrusiveMap<uint32_t, Handle>::Pair {
   public:
    constexpr Handle(L2capChannel& channel, uint32_t key)
        : IntrusiveMap<uint32_t, Handle>::Pair(key), channel_(channel) {}

    constexpr uint16_t connection_handle() const {
      return static_cast<uint16_t>(key() >> 16);
    }

    constexpr uint16_t channel_id() const {
      return static_cast<uint16_t>(key() & 0xFFFF);
    }

    constexpr L2capChannel* get() { return &channel_; }
    constexpr const L2capChannel* get() const { return &channel_; }

    constexpr L2capChannel& operator*() { return *get(); }
    constexpr const L2capChannel& operator*() const { return *get(); }

    constexpr L2capChannel* operator->() { return get(); }
    constexpr const L2capChannel* operator->() const { return get(); }

   private:
    L2capChannel& channel_;
  };

  // Produces a key from a connection handle and local or remote CID that can be
  // used to look up a channel handle in the manager's local or remote channel
  // map, respectively.
  static constexpr uint32_t MakeKey(uint16_t connection_handle, uint16_t cid) {
    return (uint32_t(connection_handle) << 16) | cid;
  }

  Handle& local_handle() { return local_handle_; }
  Handle& remote_handle() { return remote_handle_; }

  //--------------
  //  Data members
  //--------------

  /// Mutex for guarding internal and client channel pointers to each other.
  /// This mutex is shared between all internal and client channel objects.
  static sync::Mutex static_mutex_ PW_ACQUIRED_BEFORE(mutex_);

  // Mutex for guarding state.
  mutable sync::Mutex mutex_ PW_ACQUIRED_AFTER(static_mutex_);

  L2capChannelManager& l2cap_channel_manager_;

  State state_ PW_GUARDED_BY(mutex_) = State::kNew;

  const AclTransportType transport_;

  // L2CAP channel handle using the ID of local endpoint.
  Handle local_handle_;

  // L2CAP channel handle using the ID of remote endpoint.
  Handle remote_handle_;

  // Notify clients of asynchronous events encountered such as errors.
  const ChannelEventCallback event_fn_;

  // Optional client-provided allocator for MultiBufs.
  MultiBufAllocator* rx_multibuf_allocator_ = nullptr;

  // Client-provided controller read callback.
  const OptionalPayloadReceiveCallback payload_from_controller_fn_;
  // Client-provided host read callback.
  const OptionalPayloadReceiveCallback payload_from_host_fn_;

  // Recombination MultiBufs used by Recombiner to store in-progress
  // payloads when they are being recombined.
  // They are stored here so that they can be allocated with the channel's
  // allocator and also properly destroyed with the channel.
  std::array<std::optional<MultiBufInstance>, kNumDirections>
      recombination_mbufs_{};

  // Holds a pointer to the acquired client channel. Channels that are not
  // acquired, e.g. signaling channels, will not have a value. Channels that
  // have been disconnected will have a null value.
  std::optional<internal::GenericL2capChannel*> client_
      PW_GUARDED_BY(static_mutex_);

  /// The number of outstanding `BorrowedL2capChannel` objects for this object.
  uint8_t num_borrows_ PW_GUARDED_BY(mutex_) = 0;

  /// Used to unblock the destructor when the number of borrows drops to zero.
  sync::ThreadNotification notification_;
};

}  // namespace pw::bluetooth::proxy
