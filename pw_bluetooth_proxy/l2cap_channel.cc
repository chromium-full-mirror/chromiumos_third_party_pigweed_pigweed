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

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"

#include <mutex>
#include <optional>

#include "lib/stdcompat/utility.h"
#include "pw_assert/check.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/internal/generic_l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::bluetooth::proxy {
namespace internal {

BorrowedL2capChannel::BorrowedL2capChannel(L2capChannel& channel)
    : channel_(&channel) {
  std::lock_guard lock(channel_->mutex_);
  ++channel_->num_borrows_;
  PW_CHECK_UINT_NE(channel_->num_borrows_, 0);
}

BorrowedL2capChannel& BorrowedL2capChannel::operator=(
    BorrowedL2capChannel&& other) {
  if (this != &other) {
    std::swap(channel_, other.channel_);
  }
  return *this;
}

BorrowedL2capChannel::~BorrowedL2capChannel() {
  if (channel_ == nullptr) {
    return;
  }
  std::lock_guard lock(channel_->mutex_);
  PW_CHECK_UINT_NE(channel_->num_borrows_, 0);
  if (--channel_->num_borrows_ == 0) {
    channel_->notification_.release();
  }
}

}  // namespace internal

sync::Mutex L2capChannel::static_mutex_;

L2capChannel::~L2capChannel() {
  // Block until there are no outstanding borrows. Callers (namely
  // L2capChannelManager) MUST NOT be holding the `static_mutex_` when this
  // destructor is called.
  std::unique_lock lock(mutex_);
  while (num_borrows_ != 0) {
    lock.unlock();
    notification_.acquire();
    lock.lock();
  }

  PW_LOG_INFO(
      "btproxy: L2capChannel dtor - transport_: %u, connection_handle_ : "
      "%#x, local_cid_: %#x, remote_cid_: %#x, state_: %u",
      cpp23::to_underlying(transport_),
      connection_handle(),
      local_cid(),
      remote_cid(),
      cpp23::to_underlying(state_));

  // Most channels are explicitly closed, with the exception of the signaling
  // channels. Ensure those that are not are deregistered before destruction.
  if (state_ != State::kClosed) {
    l2cap_channel_manager_.DeregisterChannel(*this);
  }
  payload_queue_.clear();
}

void L2capChannel::Stop() {
  std::lock_guard lock(mutex_);
  PW_LOG_INFO(
      "btproxy: L2capChannel::Stop - transport_: %u, connection_handle_: %#x, "
      "local_cid_: %#x, remote_cid_: %#x, previous state_: %u",
      cpp23::to_underlying(transport_),
      connection_handle(),
      local_cid(),
      remote_cid(),
      cpp23::to_underlying(state_));

  PW_CHECK(state_ != State::kNew && state_ != State::kClosed);
  state_ = State::kStopped;
  payload_queue_.clear();
}

void L2capChannel::Close(L2capChannelEvent event) {
  {
    std::lock_guard lock(mutex_);
    PW_LOG_INFO(
        "btproxy: L2capChannel::Close - transport_: %u, "
        "connection_handle_: %#x, local_cid_: %#x, remote_cid_: %#x, previous "
        "state_: %u",
        cpp23::to_underlying(transport_),
        connection_handle(),
        local_cid(),
        remote_cid(),
        cpp23::to_underlying(state_));

    PW_CHECK(state_ != State::kNew);
    if (state_ == State::kClosed) {
      return;
    }
    state_ = State::kClosed;
    payload_queue_.clear();
  }

  SendEvent(event);

  {
    std::lock_guard lock(L2capChannel::mutex());
    if (!client_.has_value() || *client_ == nullptr) {
      return;
    }
    (*client_)->channel_ = nullptr;
    client_ = nullptr;
  }
}

StatusWithMultiBuf L2capChannel::Write(FlatConstMultiBuf&& payload) {
  StatusWithMultiBuf result = WriteDuringRx(std::move(payload));
  DrainChannelQueuesIfNewTx();
  return result;
}

StatusWithMultiBuf L2capChannel::WriteDuringRx(FlatConstMultiBuf&& payload) {
  std::lock_guard lock(mutex_);
  if (state_ != L2capChannel::State::kRunning) {
    PW_LOG_WARN(
        "btproxy: L2capChannel::Write called when not running. "
        "local_cid: %#x, remote_cid: %#x, state: %u",
        local_cid(),
        remote_cid(),
        cpp23::to_underlying(state_));
    return {Status::FailedPrecondition(), std::move(payload)};
  }

  if (payload_queue_.full()) {
    notify_on_dequeue_ = true;
    return {Status::Unavailable(), std::move(payload)};
  }
  payload_queue_.push(std::move(payload));
  ReportNewTxPacketsOrCredits();
  return {OkStatus(), std::nullopt};
}

Status L2capChannel::IsWriteAvailable() {
  if (state() != State::kRunning) {
    return Status::FailedPrecondition();
  }

  std::lock_guard lock(mutex_);

  if (payload_queue_.full()) {
    notify_on_dequeue_ = true;
    return Status::Unavailable();
  }

  notify_on_dequeue_ = false;
  return OkStatus();
}

std::optional<H4PacketWithH4> L2capChannel::DequeuePacket() {
  std::optional<H4PacketWithH4> packet;
  bool should_notify = false;
  {
    // Get a payload.
    std::lock_guard lock(mutex_);
    if (state_ != L2capChannel::State::kRunning) {
      payload_queue_.clear();
    }
    if (payload_queue_.empty()) {
      return std::nullopt;
    }
    const FlatConstMultiBuf& payload = payload_queue_.front();

    // Create a packet from the payload.
    bool keep_payload = false;
    packet = GenerateNextTxPacket(payload, keep_payload);

    // If no additional packets can be created from the payload, remove it.
    if (!keep_payload) {
      payload_queue_.pop();
      should_notify = notify_on_dequeue_;
      notify_on_dequeue_ = false;
    }
  }

  // Notify the client if there is now room in the queue.
  if (should_notify) {
    SendEvent(L2capChannelEvent::kWriteAvailable);
  }

  return packet;
}

bool L2capChannel::HandlePduFromController(pw::span<uint8_t> l2cap_pdu) {
  if (state() != State::kRunning) {
    PW_LOG_ERROR(
        "btproxy: L2capChannel::OnPduReceivedFromController on non-running "
        "channel. local_cid: %#x, remote_cid: %#x, state: %u",
        local_cid(),
        remote_cid(),
        cpp23::to_underlying(state()));
    SendEvent(L2capChannelEvent::kRxWhileStopped);
    return true;
  }
  return DoHandlePduFromController(l2cap_pdu);
}

L2capChannel::State L2capChannel::state() const {
  std::lock_guard lock(mutex_);
  return state_;
}

L2capChannel::L2capChannel(
    L2capChannelManager& l2cap_channel_manager,
    MultiBufAllocator* rx_multibuf_allocator,
    uint16_t connection_handle,
    AclTransportType transport,
    uint16_t local_cid,
    uint16_t remote_cid,
    OptionalPayloadReceiveCallback&& payload_from_controller_fn,
    OptionalPayloadReceiveCallback&& payload_from_host_fn,
    ChannelEventCallback&& event_fn)
    : l2cap_channel_manager_(l2cap_channel_manager),
      transport_(transport),
      local_handle_(*this, MakeKey(connection_handle, local_cid)),
      remote_handle_(*this, MakeKey(connection_handle, remote_cid)),
      event_fn_(std::move(event_fn)),
      rx_multibuf_allocator_(rx_multibuf_allocator),
      payload_from_controller_fn_(std::move(payload_from_controller_fn)),
      payload_from_host_fn_(std::move(payload_from_host_fn)) {
  PW_LOG_INFO(
      "btproxy: L2capChannel ctor - transport_: %u, connection_handle_ : %u, "
      "local_cid_ : %#x, remote_cid_: %#x",
      cpp23::to_underlying(transport_),
      connection_handle,
      local_cid,
      remote_cid);
}

void L2capChannel::Init() {
  {
    std::lock_guard lock(mutex_);
    state_ = State::kRunning;
    PW_LOG_INFO(
        "btproxy: L2capChannel initialized: "
        "transport_: %u, connection_handle_ : %u, "
        "local_cid_ : %#x, remote_cid_: %#x",
        cpp23::to_underlying(transport_),
        connection_handle(),
        local_cid(),
        remote_cid());
  }
  l2cap_channel_manager_.RegisterChannel(*this);
}

bool L2capChannel::AreValidParameters(uint16_t connection_handle,
                                      uint16_t local_cid,
                                      uint16_t remote_cid) {
  if (connection_handle > kMaxValidConnectionHandle) {
    PW_LOG_ERROR(
        "Invalid connection handle %#x. Maximum connection handle is 0x0EFF.",
        connection_handle);
    return false;
  }
  if (local_cid == 0 || remote_cid == 0) {
    PW_LOG_ERROR("L2CAP channel identifier 0 is not valid.");
    return false;
  }
  return true;
}

pw::Result<H4PacketWithH4> L2capChannel::PopulateTxL2capPacket(
    uint16_t data_length) {
  return PopulateL2capPacket(data_length);
}

namespace {

constexpr size_t H4SizeForL2capData(uint16_t data_length) {
  const size_t l2cap_packet_size =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + data_length;
  const size_t acl_packet_size =
      emboss::AclDataFrameHeader::IntrinsicSizeInBytes() + l2cap_packet_size;
  return sizeof(emboss::H4PacketType) + acl_packet_size;
}

}  // namespace

pw::Result<H4PacketWithH4> L2capChannel::PopulateL2capPacket(
    uint16_t data_length) {
  const size_t l2cap_packet_size =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + data_length;
  const size_t h4_packet_size = H4SizeForL2capData(data_length);

  pw::Result<H4PacketWithH4> h4_packet_res =
      l2cap_channel_manager_.GetAclH4Packet(h4_packet_size);
  if (!h4_packet_res.ok()) {
    return h4_packet_res.status();
  }
  H4PacketWithH4 h4_packet = std::move(h4_packet_res.value());
  h4_packet.SetH4Type(emboss::H4PacketType::ACL_DATA);

  PW_TRY_ASSIGN(
      auto acl,
      MakeEmbossWriter<emboss::AclDataFrameWriter>(h4_packet.GetHciSpan()));
  acl.header().handle().Write(connection_handle());
  // TODO: https://pwbug.dev/360932103 - Support packet segmentation, so this
  // value will not always be FIRST_NON_FLUSHABLE.
  acl.header().packet_boundary_flag().Write(
      emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
  acl.header().broadcast_flag().Write(
      emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
  acl.data_total_length().Write(l2cap_packet_size);

  PW_TRY_ASSIGN(auto l2cap_header,
                MakeEmbossWriter<emboss::BasicL2capHeaderWriter>(
                    acl.payload().BackingStorage().data(),
                    emboss::BasicL2capHeader::IntrinsicSizeInBytes()));
  l2cap_header.pdu_length().Write(data_length);
  l2cap_header.channel_id().Write(remote_cid());

  return h4_packet;
}

std::optional<uint16_t> L2capChannel::MaxL2capPayloadSize() const {
  std::optional<uint16_t> max_acl_length =
      channel_manager().MaxDataPacketLengthForTransport(transport());
  if (!max_acl_length.has_value()) {
    return std::nullopt;
  }
  if (*max_acl_length <= emboss::BasicL2capHeader::IntrinsicSizeInBytes()) {
    return std::nullopt;
  }
  return *max_acl_length - emboss::BasicL2capHeader::IntrinsicSizeInBytes();
}

void L2capChannel::ReportNewTxPacketsOrCredits() {
  l2cap_channel_manager_.ReportNewTxPacketsOrCredits();
}

void L2capChannel::DrainChannelQueuesIfNewTx() {
  l2cap_channel_manager_.DrainChannelQueuesIfNewTx();
}

//-------
//  Rx (protected)
//-------

bool L2capChannel::SendPayloadFromHostToClient(pw::span<uint8_t> payload) {
  return SendPayloadToClient(payload, payload_from_host_fn_);
}

bool L2capChannel::SendPayloadFromControllerToClient(
    pw::span<uint8_t> payload) {
  return SendPayloadToClient(payload, payload_from_controller_fn_);
}

bool L2capChannel::SendPayloadToClient(
    pw::span<uint8_t> payload, const OptionalPayloadReceiveCallback& callback) {
  if (!callback) {
    return false;
  }

  if (!rx_multibuf_allocator_) {
    PW_LOG_ERROR(
        "btproxy: rx_multibuf_allocator_ is null so unable to create multibuf "
        "to pass to client. Will passthrough instead. "
        "connection: %#x, local_cid: %#x ",
        connection_handle(),
        local_cid());
    return false;
  }

  auto result =
      MultiBufAdapter::Create(*rx_multibuf_allocator_, payload.size());
  if (!result.has_value()) {
    PW_LOG_ERROR(
        "btproxy: rx_multibuf_allocator_ is out of memory. So stopping "
        "channel and reporting it needs to be closed."
        "connection: %#x, local_cid: %#x ",
        connection_handle(),
        local_cid());
    StopAndSendEvent(L2capChannelEvent::kRxOutOfMemory);
    return true;
  }

  FlatMultiBufInstance buffer = std::move(result.value());
  MultiBufAdapter::Copy(buffer, 0, as_bytes(payload));

  // If client returned multibuf to us, we copy it to the payload and indicate
  // to the caller that packet should be forwarded.
  // In the future when whole path is operating with multibuf's, we could pass
  // it back up to container to be forwarded and avoid the two copies of
  // payload.
  auto client_multibuf = callback(std::move(MultiBufAdapter::Unwrap(buffer)));
  if (client_multibuf.has_value()) {
    MultiBufAdapter::Copy(as_writable_bytes(payload), client_multibuf.value());
    return false;
  }
  return true;
}

pw::Status L2capChannel::StartRecombinationBuf(Direction direction,
                                               size_t payload_size,
                                               size_t extra_header_size) {
  std::optional<MultiBufInstance>& buf_optref =
      GetRecombinationBufOptRef(direction);
  PW_CHECK(!buf_optref.has_value());

  if (rx_multibuf_allocator_ == nullptr) {
    // TODO: https://pwbug.dev/423695410 - Should eventually recombine for these
    // cases to allow channel to make handle/unhandle decision.
    PW_LOG_WARN(
        "Cannot start recombination without an allocator."
        "connection: %#x, local_cid: %#x ",
        connection_handle(),
        local_cid());
    return Status::FailedPrecondition();
  }

  buf_optref = MultiBufAdapter::Create(
      *rx_multibuf_allocator_, extra_header_size, payload_size);
  if (!buf_optref.has_value()) {
    return Status::ResourceExhausted();
  }

  return pw::OkStatus();
}

void L2capChannel::EndRecombinationBuf(Direction direction) {
  GetRecombinationBufOptRef(direction) = std::nullopt;
}

void L2capChannel::SendEvent(L2capChannelEvent event) {
  {
    std::lock_guard lock(static_mutex_);
    if (!client_.has_value() || *client_ == nullptr) {
      return;
    }
  }

  // We don't log kWriteAvailable since they happen often. Optimally we would
  // just debug log them also, but one of our downstreams logs all levels.
  if (event != L2capChannelEvent::kWriteAvailable) {
    PW_LOG_INFO(
        "btproxy: L2capChannel::SendEvent -  connection: %#x, "
        "local_cid: %#x, event: %u,",
        connection_handle(),
        local_cid(),
        cpp23::to_underlying(event));
  }

  if (event_fn_) {
    event_fn_(event);
  }
}

bool L2capChannel::IsStale() const {
  std::lock_guard lock(static_mutex_);
  return client_.has_value() && *client_ == nullptr;
}

bool L2capChannel::PayloadQueueEmpty() const {
  std::lock_guard lock(mutex_);
  return payload_queue_.empty();
}

}  // namespace pw::bluetooth::proxy
