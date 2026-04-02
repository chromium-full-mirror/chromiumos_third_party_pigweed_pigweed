// Copyright 2026 The Pigweed Authors
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

#include <mutex>

#include "fsl_clock.h"
#include "pw_assert/assert.h"
#include "pw_clock_tree/clock_tree.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace pw::clock_tree {

/// Class template implementing a synchronized clock mux.
///
/// This class supports:
/// * Clock muxes which can be disabled (kNONE_to_*) and those which cannot.
/// * Dynamic source selection via ChangeSource.
template <typename ElementType>
class ClockMcuxpressoSyncSelector : public DependentElement<ElementType> {
 public:
  static constexpr clock_attach_id_t kNoSelector =
      static_cast<clock_attach_id_t>(0);

  /// Constructs a new ClockMcuxpressoSyncSelector.
  ///
  /// @param initial_source The initial logical source element.
  /// This should reflect the state of the mux when this object is constructed.
  ///
  /// @param initial_selector The initial selector.
  /// This should reflect the state of the mux when this object is constructed.
  ///
  /// @param disable_selector The selector value which can be used to disable
  /// the mux. If the mux does not support such a value, pass kNoSelector.
  template <typename SourceType>
  constexpr ClockMcuxpressoSyncSelector(
      SourceType& initial_source,
      clock_attach_id_t initial_selector,
      clock_attach_id_t disable_selector = kNoSelector)
      : DependentElement<ElementType>(initial_source),
        current_selector_(initial_selector),
        disable_selector_(disable_selector) {
    PW_ASSERT(initial_selector != kNoSelector);
  }

  template <typename SourceType>
  pw::Status ChangeSource(SourceType& new_source,
                          clock_attach_id_t new_selector) {
    PW_ASSERT(new_selector != kNoSelector);

    std::lock_guard lock(this->lock());

    if (this->current_selector_ == new_selector) {
      return pw::OkStatus();
    }
    Element& current_source = this->source();

    // This clock element might be in an enabled or disabled state!
    //
    // We only switch the selector if:
    // 1. This element is logically enabled (ref_count() > 0).
    //    We must switch the running mux to the new source.
    // OR
    // 2. This element is logically disabled, but the mux does not support a
    //    disable selector (the mux is physically always-on.) We must perform
    //    the switch (and the synchronized mux dance) to ensure the mux points
    //    to the new source, even though this element is logically off.
    //    Note that in this case, the clock will run briefly!
    const bool is_enabled = (this->ref_count() > 0);
    const bool always_on_mux = (disable_selector_ == kNoSelector);

    if (is_enabled || always_on_mux) {
      // Hardware switch required.

      // Ensure the old source is running (sync clock mux requirement).
      // If this element is enabled, we already hold a ref to current_source.
      // If this element is disabled (but always_on_mux), we need a temporary
      // ref.
      bool acquired_temp_current_ref = false;
      if (!is_enabled) {
        PW_TRY(current_source.Acquire());
        acquired_temp_current_ref = true;
      }

      // Ensure the new source is running (sync clock mux requirement).
      // We acquire it now. If we are enabled, this becomes our permanent ref.
      // If we are disabled, this is a temporary ref for the switch.
      if constexpr (SourceType::kMayFail) {
        if (pw::Status status = new_source.Acquire(); !status.ok()) {
          if (acquired_temp_current_ref) {
            current_source.Release().IgnoreError();
          }
          return status;
        }
      } else {
        new_source.Acquire();
      }

      // Perform the hardware switch.
      CLOCK_AttachClk(new_selector);

      // Cleanup the old source.
      // We always release the old source.
      // If is_enabled: We are dropping our permanent ref.
      // If !is_enabled: We are dropping the temporary ref we took above.
      current_source.Release().IgnoreError();

      // Cleanup the new source (if necessary).
      if (!is_enabled) {
        // This element is disabled, so we cannot hold on to the reference
        // taken above.
        if constexpr (SourceType::kMayFail) {
          new_source.Release().IgnoreError();
        } else {
          new_source.Release();
        }
      }
    }

    // Update internal state regardless of whether we touched HW.
    // If we didn't touch HW, DoAcquireLocked() / DoEnable() will use these new
    // values later.
    this->SetSource(new_source);
    current_selector_ = new_selector;

    return pw::OkStatus();
  }

 private:
  clock_attach_id_t current_selector_;
  const clock_attach_id_t disable_selector_;

  // pw::clock_tree::Element impl.

  // NOTE: DoEnable() and DoDisable() are marked 'final' and cannot fail.
  // This allows internal callers to skip error checking.
  pw::Status DoEnable() final {
    PW_ASSERT(current_selector_ != kNoSelector);
    CLOCK_AttachClk(current_selector_);
    return pw::OkStatus();
  }

  pw::Status DoDisable() final {
    if (disable_selector_ != kNoSelector) {
      CLOCK_AttachClk(disable_selector_);
    }
    return pw::OkStatus();
  }
};

/// Alias for a blocking synchronized clock selector clock tree element.
using ClockMcuxpressoSyncSelectorBlocking =
    ClockMcuxpressoSyncSelector<ElementBlocking>;

/// Alias for a non-blocking synchronized clock selector clock tree element
/// where updates might fail.
using ClockMcuxpressoSyncSelectorNonBlockingMightFail =
    ClockMcuxpressoSyncSelector<ElementNonBlockingMightFail>;

/// Alias for a non-blocking synchronized clock selector clock tree element
/// where updates cannot fail.
using ClockMcuxpressoSyncSelectorNonBlocking =
    ClockMcuxpressoSyncSelector<ElementNonBlockingCannotFail>;

}  // namespace pw::clock_tree
