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

#include "pw_clock_tree_mcuxpresso/sync_selector.h"

#include "fsl_clock.h"
#include "mocks.h"
#include "pw_unit_test/framework.h"

namespace pw::clock_tree {
namespace {

// Mock clock sources for testing
class MockSourceBlocking : public ClockSourceBlocking {
 public:
  void set_fail_on_enable(bool fail) { fail_on_enable_ = fail; }

 private:
  pw::Status DoEnable() final {
    if (fail_on_enable_) {
      return pw::Status::Internal();
    }
    return pw::OkStatus();
  }

  pw::Status DoDisable() final { return pw::OkStatus(); }

  bool fail_on_enable_ = false;
};

class MockSourceNonBlockingMightFail : public ClockSourceNonBlockingMightFail {
 public:
  void set_fail_on_enable(bool fail) { fail_on_enable_ = fail; }

 private:
  pw::Status DoEnable() final {
    if (fail_on_enable_) {
      return pw::Status::Internal();
    }
    return pw::OkStatus();
  }

  pw::Status DoDisable() final { return pw::OkStatus(); }

  bool fail_on_enable_ = false;
};

class MockSourceNonBlockingCannotFail
    : public ClockSourceNonBlockingCannotFail {
 private:
  pw::Status DoEnable() final { return pw::OkStatus(); }

  pw::Status DoDisable() final { return pw::OkStatus(); }
};

const clock_attach_id_t kDisableSelector = 7;

TEST(ClockMcuxpressoSyncSelector, BlockingWithMixedSources) {
  sdk_state.Reset();
  MockSourceBlocking source1;
  MockSourceNonBlockingCannotFail source2;
  // Use arbitrary selector values for testing
  ClockMcuxpressoSyncSelectorBlocking sync_selector(
      source1, 1, kDisableSelector);
  EXPECT_EQ(sync_selector.ref_count(), 0u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);

  // Acquire sync_selector
  EXPECT_EQ(sync_selector.Acquire(), OkStatus());
  EXPECT_EQ(sync_selector.ref_count(), 1u);
  EXPECT_EQ(source1.ref_count(), 1u);
  EXPECT_EQ(source2.ref_count(), 0u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // DoEnable
  EXPECT_EQ(sdk_state.last_attached_clk, 1u);

  // Change source to source2
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 2));
  EXPECT_EQ(sync_selector.ref_count(), 1u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 1u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 2u);  // ChangeSource
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);

  // Release sync_selector
  PW_TEST_EXPECT_OK(sync_selector.Release());
  EXPECT_EQ(sync_selector.ref_count(), 0u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 3u);  // DoDisable
  EXPECT_EQ(sdk_state.last_attached_clk, kDisableSelector);
}

// Test with ElementNonBlockingMightFail and mixed sources
TEST(ClockMcuxpressoSyncSelector, NonBlockingMightFailWithMixedSources) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingMightFail source2;
  ClockMcuxpressoSyncSelectorNonBlockingMightFail sync_selector(
      source1, 1, kDisableSelector);
  EXPECT_EQ(sync_selector.ref_count(), 0u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);

  // Initial source is CannotFail
  PW_TEST_EXPECT_OK(sync_selector.Acquire());
  EXPECT_EQ(sync_selector.ref_count(), 1u);
  EXPECT_EQ(source1.ref_count(), 1u);
  EXPECT_EQ(source2.ref_count(), 0u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // DoEnable
  EXPECT_EQ(sdk_state.last_attached_clk, 1u);

  // Change source to source2
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 2));
  EXPECT_EQ(sync_selector.ref_count(), 1u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 1u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 2u);  // ChangeSource
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);

  // Change back to source1
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source1, 1));
  EXPECT_EQ(sync_selector.ref_count(), 1u);
  EXPECT_EQ(source1.ref_count(), 1u);
  EXPECT_EQ(source2.ref_count(), 0u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 3u);  // ChangeSource
  EXPECT_EQ(sdk_state.last_attached_clk, 1u);

  // Release sync_selector
  PW_TEST_EXPECT_OK(sync_selector.Release());
  EXPECT_EQ(sync_selector.ref_count(), 0u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 4u);  // DoDisable
  EXPECT_EQ(sdk_state.last_attached_clk, kDisableSelector);
}

// Test with ElementNonBlockingCannotFail
TEST(ClockMcuxpressoSyncSelector, NonBlockingCannotFailSources) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingCannotFail source2;
  ClockMcuxpressoSyncSelectorNonBlocking sync_selector(
      source1, 1, kDisableSelector);
  EXPECT_EQ(sync_selector.ref_count(), 0u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);

  sync_selector.Acquire();
  EXPECT_EQ(sync_selector.ref_count(), 1u);
  EXPECT_EQ(source1.ref_count(), 1u);
  EXPECT_EQ(source2.ref_count(), 0u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // DoEnable
  EXPECT_EQ(sdk_state.last_attached_clk, 1u);

  // Change source to source2
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 2));
  EXPECT_EQ(sync_selector.ref_count(), 1u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 1u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 2u);  // ChangeSource
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);

  // Release sync_selector
  sync_selector.Release();
  EXPECT_EQ(sync_selector.ref_count(), 0u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);
  EXPECT_EQ(sdk_state.attach_clk_call_count, 3u);  // DoDisable
  EXPECT_EQ(sdk_state.last_attached_clk, kDisableSelector);
}

TEST(ClockMcuxpressoSyncSelector, ChangeSourceSameSelector) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingCannotFail source2;
  ClockMcuxpressoSyncSelectorNonBlocking sync_selector(
      source1, 1, kDisableSelector);

  // 1. Inactive: Change to same selector should do nothing.
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source1, 1));
  EXPECT_EQ(sdk_state.attach_clk_call_count, 0u);
  EXPECT_EQ(source1.ref_count(), 0u);

  // Even if we provide a different source but same selector, it should do
  // nothing as per current implementation.
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 1));
  EXPECT_EQ(sdk_state.attach_clk_call_count, 0u);
  EXPECT_EQ(source2.ref_count(), 0u);

  // 2. Active: Change to same selector should do nothing.
  sync_selector.Acquire();
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // DoEnable
  EXPECT_EQ(source1.ref_count(), 1u);

  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source1, 1));
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // No change
  EXPECT_EQ(source1.ref_count(), 1u);

  // Release
  sync_selector.Release();
  EXPECT_EQ(sdk_state.attach_clk_call_count, 2u);  // DoDisable
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(sdk_state.last_attached_clk, kDisableSelector);
}

TEST(ClockMcuxpressoSyncSelector, AlwaysOnMux) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingCannotFail source2;
  // disable_selector = kNoSelector (0) means mux cannot be disabled.
  ClockMcuxpressoSyncSelectorNonBlocking sync_selector(source1, 1);

  // 1. Inactive: ChangeSource should STILL touch hardware.
  // This is because for an always-on mux, we must ensure it points to the
  // correct source even when this element is logically disabled.
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 2));
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // Hardware switch performed
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);

  // 2. Acquire
  sync_selector.Acquire();
  EXPECT_EQ(sdk_state.attach_clk_call_count, 2u);  // DoEnable
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);
  EXPECT_EQ(source2.ref_count(), 1u);

  // 3. Release: Should NOT touch hardware
  sync_selector.Release();
  EXPECT_EQ(sdk_state.attach_clk_call_count,
            2u);  // No call to AttachClk on disable
  EXPECT_EQ(source2.ref_count(), 0u);
}

TEST(ClockMcuxpressoSyncSelector, AlwaysOnMuxActive) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingCannotFail source2;
  // disable_selector = kNoSelector (0) means mux cannot be disabled.
  ClockMcuxpressoSyncSelectorNonBlocking sync_selector(source1, 1);

  // 1. Acquire
  sync_selector.Acquire();
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // DoEnable
  EXPECT_EQ(sdk_state.last_attached_clk, 1u);
  EXPECT_EQ(source1.ref_count(), 1u);

  // 2. Active: ChangeSource should update hardware and sources.
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 2));
  EXPECT_EQ(sdk_state.attach_clk_call_count, 2u);  // Hardware switch performed
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 1u);

  // 3. Release
  sync_selector.Release();
  EXPECT_EQ(sdk_state.attach_clk_call_count,
            2u);  // No call to AttachClk on disable
  EXPECT_EQ(source2.ref_count(), 0u);
}

TEST(ClockMcuxpressoSyncSelector, ChangeSourceNewSourceAcquireFail) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingMightFail source2;
  ClockMcuxpressoSyncSelectorNonBlockingMightFail sync_selector(
      source1, 1, kDisableSelector);

  // 1. Active
  PW_TEST_EXPECT_OK(sync_selector.Acquire());
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);
  EXPECT_EQ(source1.ref_count(), 1u);

  // 2. Inject failure on source2 acquire
  source2.set_fail_on_enable(true);
  EXPECT_EQ(sync_selector.ChangeSource(source2, 2), pw::Status::Internal());

  // 3. Verify state remains unchanged
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // No hardware switch
  EXPECT_EQ(sdk_state.last_attached_clk, 1u);
  EXPECT_EQ(source1.ref_count(), 1u);
  EXPECT_EQ(source2.ref_count(), 0u);

  PW_TEST_EXPECT_OK(sync_selector.Release());
}

TEST(ClockMcuxpressoSyncSelector, ChangeSourceCurrentSourceAcquireFail) {
  sdk_state.Reset();
  MockSourceNonBlockingMightFail source1;
  MockSourceNonBlockingCannotFail source2;
  // disable_selector = kNoSelector (0) means mux is always-on.
  ClockMcuxpressoSyncSelectorNonBlockingMightFail sync_selector(source1, 1);

  // 1. Inactive always-on mux.
  EXPECT_EQ(sync_selector.ref_count(), 0u);

  // 2. Inject failure on source1 acquire (needed for temp ref during switch)
  source1.set_fail_on_enable(true);
  EXPECT_EQ(sync_selector.ChangeSource(source2, 2), pw::Status::Internal());

  // 3. Verify state remains unchanged
  EXPECT_EQ(sdk_state.attach_clk_call_count, 0u);  // No hardware switch
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);
}

TEST(ClockMcuxpressoSyncSelector, ChangeSourceNewSourceAcquireFailWithTempRef) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingMightFail source2;
  // disable_selector = kNoSelector (0) means mux is always-on.
  ClockMcuxpressoSyncSelectorNonBlockingMightFail sync_selector(source1, 1);

  // 1. Inactive always-on mux.
  EXPECT_EQ(sync_selector.ref_count(), 0u);

  // 2. Inject failure on source2 acquire
  source2.set_fail_on_enable(true);
  // ChangeSource will:
  // - Acquire source1 (succeeds)
  // - Attempt to acquire source2 (fails)
  // - Release source1
  EXPECT_EQ(sync_selector.ChangeSource(source2, 2), pw::Status::Internal());

  // 3. Verify state remains unchanged and temp ref released
  EXPECT_EQ(sdk_state.attach_clk_call_count, 0u);  // No hardware switch
  EXPECT_EQ(source1.ref_count(), 0u);              // Temp ref released
  EXPECT_EQ(source2.ref_count(), 0u);
}

TEST(ClockMcuxpressoSyncSelector, InactiveWithDisableSelector) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingCannotFail source2;
  ClockMcuxpressoSyncSelectorNonBlocking sync_selector(
      source1, 1, kDisableSelector);

  // 1. Inactive: ChangeSource when HAS a disable selector should NOT touch
  // hardware.
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 2));
  EXPECT_EQ(sdk_state.attach_clk_call_count, 0u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);

  // 2. Acquire should now use the NEW source and selector.
  sync_selector.Acquire();
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // DoEnable
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);
  EXPECT_EQ(source2.ref_count(), 1u);

  sync_selector.Release();
}

TEST(ClockMcuxpressoSyncSelector, AlwaysOnMuxMightFailSource) {
  sdk_state.Reset();
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingMightFail source2;
  ClockMcuxpressoSyncSelectorNonBlockingMightFail sync_selector(source1, 1);

  // 1. Inactive switch for always-on mux with MightFail source.
  PW_TEST_EXPECT_OK(sync_selector.ChangeSource(source2, 2));
  EXPECT_EQ(sdk_state.attach_clk_call_count, 1u);  // Hardware switch performed
  EXPECT_EQ(sdk_state.last_attached_clk, 2u);
  EXPECT_EQ(source1.ref_count(), 0u);
  EXPECT_EQ(source2.ref_count(), 0u);  // Temporary ref released
}

}  // namespace
}  // namespace pw::clock_tree
