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

#include "pw_bluetooth_sapphire/internal/host/common/smart_task.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

namespace bt::l2cap::internal {

struct SniffModeParams {
  uint16_t min_interval;
  uint16_t max_interval;
  uint16_t sniff_attempt;
  uint16_t sniff_timeout;
};

class AutosniffSuppressInterest;

/// Implements autosniff functionality for a logical link
class Autosniff {
 public:
  Autosniff(SniffModeParams params,
            hci::CommandChannel* channel,
            hci_spec::ConnectionHandle handle,
            pw::async::Dispatcher* dispatcher,
            pw::chrono::SystemClock::duration idle_timeout);

  void MarkPacketRx();
  void MarkPacketTx();

  /// Suppress sniff mode for the time being. Returns a token which should be
  /// released explicitly using AutosniffSuppressInterest::Release() or by
  /// deconstructing. Returns nullptr_t if suppression fails for any reason.
  std::unique_ptr<AutosniffSuppressInterest> Suppress(const char* reason);

  // Attach Autosniff's inspect node as a child of |parent| with the given
  // |name|.
  void AttachInspect(inspect::Node& parent, std::string name);

  inline pw::bluetooth::emboss::AclConnectionMode CurrentMode() const {
    return connection_mode_;
  }

  using WeakPtr = WeakSelf<Autosniff>::WeakPtr;

 private:
  friend class AutosniffSuppressInterest;

  RecurringDisposition OnTimeout();
  void ResetTimeout();
  hci::CommandChannel::EventCallbackResult OnModeChange(
      const hci::EventPacket& event);

  static auto ChangeModesCallback(
      Autosniff::WeakPtr self,
      pw::bluetooth::emboss::AclConnectionMode new_mode);

  void RemoveSuppression();

  SniffModeParams params_;
  RecurringTask autosniff_timeout_;
  hci::CommandChannel* cmd_channel_;
  hci_spec::ConnectionHandle handle_;
  hci::CommandChannel::OwnedEventHandle mode_change_event_;
  pw::bluetooth::emboss::AclConnectionMode connection_mode_ =
      pw::bluetooth::emboss::AclConnectionMode::ACTIVE;

  inspect::Node inspect_node_;
  inspect::StringProperty inspect_current_mode_;

  uint8_t suppression_count_ = 0;

  // Used to avoid a mode change request while in the process of transitioning
  bool mode_transition_ = false;

  WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }
  WeakSelf<Autosniff> weak_self_;
};

class AutosniffSuppressInterest final {
 public:
  // Destroying the suppress interest will clear out this suppression,
  // possibly restarting the autosniff timer.
  ~AutosniffSuppressInterest();

  // Attach as a child of |parent| with the given |name|.
  void AttachInspect(inspect::Node& parent, std::string name);

  // Releasing the suppression interest will clear out this suppression,
  // possibly restarting the autosniff timer.
  // This function is idempotent.
  void Release();

 private:
  friend class Autosniff;

  // Used by Autosniff to create a SuppressInterest.
  explicit AutosniffSuppressInterest(Autosniff::WeakPtr autosniff,
                                     const char* reason);

  const char* reason_;
  Autosniff::WeakPtr autosniff_;

  inspect::StringProperty inspect_reason_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AutosniffSuppressInterest);
};

}  // namespace bt::l2cap::internal
