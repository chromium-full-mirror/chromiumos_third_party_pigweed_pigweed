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

#include "pw_bluetooth_proxy/internal/generic_l2cap_channel.h"

#include <mutex>

#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_bluetooth_proxy/internal/l2cap_coc_internal.h"

namespace pw::bluetooth::proxy::internal {

GenericL2capChannel::GenericL2capChannel(L2capChannel& channel)
    : channel_(&channel),
      connection_handle_(channel.connection_handle()),
      transport_(channel.transport()),
      local_cid_(channel.local_cid()),
      remote_cid_(channel.remote_cid()) {
  std::lock_guard lock(L2capChannel::mutex());
  channel_->client_ = this;
}

GenericL2capChannel& GenericL2capChannel::operator=(
    GenericL2capChannel&& other) {
  if (this == &other) {
    return *this;
  }
  std::lock_guard lock(L2capChannel::mutex());
  if (channel_ != nullptr) {
    channel_->client_ = nullptr;
  }
  channel_ = std::exchange(other.channel_, nullptr);
  if (channel_ != nullptr) {
    channel_->client_ = this;
  }
  connection_handle_ = std::exchange(other.connection_handle_, 0);
  transport_ = std::exchange(other.transport_, AclTransportType::kBrEdr);
  local_cid_ = std::exchange(other.local_cid_, 0);
  remote_cid_ = std::exchange(other.remote_cid_, 0);
  return *this;
}

GenericL2capChannel::~GenericL2capChannel() {
  std::lock_guard lock(L2capChannel::mutex());
  if (channel_ != nullptr) {
    channel_->client_ = nullptr;
  }
}

StatusWithMultiBuf GenericL2capChannel::Write(FlatConstMultiBuf&& payload) {
  if (auto status = DoCheckWriteParameter(payload); !status.ok()) {
    return {status, std::move(payload)};
  }
  auto result = BorrowL2capChannel();
  if (!result.ok()) {
    return {Status::FailedPrecondition(), std::move(payload)};
  }
  BorrowedL2capChannel borrowed = std::move(*result);
  return borrowed->Write(std::move(payload));
}

Status GenericL2capChannel::IsWriteAvailable() {
  PW_TRY_ASSIGN(BorrowedL2capChannel borrowed, BorrowL2capChannel());
  return borrowed->IsWriteAvailable();
}

Status GenericL2capChannel::SendAdditionalRxCredits(
    uint16_t additional_rx_credits) {
  PW_TRY_ASSIGN(BorrowedL2capChannel borrowed, BorrowL2capChannel());
  auto& l2cap_coc = static_cast<internal::L2capCocInternal&>(*borrowed);
  return l2cap_coc.SendAdditionalRxCredits(additional_rx_credits);
}

void GenericL2capChannel::Stop() {
  auto result = BorrowL2capChannel();
  if (result.ok()) {
    BorrowedL2capChannel borrowed = std::move(*result);
    borrowed->Stop();
  }
}

void GenericL2capChannel::Close() {
  auto result = BorrowL2capChannel();
  if (result.ok()) {
    BorrowedL2capChannel borrowed = std::move(*result);
    borrowed->Close();
  }
}

L2capChannel* GenericL2capChannel::InternalForTesting() const {
  std::lock_guard lock(L2capChannel::mutex());
  return channel_;
}

Result<BorrowedL2capChannel> GenericL2capChannel::BorrowL2capChannel() const {
  std::lock_guard lock(L2capChannel::mutex());
  if (channel_ == nullptr ||
      channel_->state() != L2capChannel::State::kRunning) {
    return Status::FailedPrecondition();
  }
  return BorrowedL2capChannel(*channel_);
}

}  // namespace pw::bluetooth::proxy::internal
