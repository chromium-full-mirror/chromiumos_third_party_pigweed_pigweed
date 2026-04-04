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

#include "pw_clock_tree_mcuxpresso/clock_tree.h"
#include "pw_clock_tree_mcuxpresso/sync_selector.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_unit_test/framework.h"

namespace pw::clock_tree {
namespace {

// Mock clock sources for testing
class MockSourceBlocking : public ClockSourceBlocking {
 public:
  Status DoEnable() override { return OkStatus(); }
};

class MockSourceNonBlockingMightFail : public ClockSourceNonBlockingMightFail {
 public:
  Status DoEnable() override { return OkStatus(); }
};

class MockSourceNonBlockingCannotFail
    : public ClockSourceNonBlockingCannotFail {
 public:
  Status DoEnable() override { return OkStatus(); }
};

TEST(ClockTreeSyncSelector, CompileTimeTests) {}

#if PW_NC_TEST(NonBlockingCannotFailRejectMightFail)
PW_NC_EXPECT("Non-failing element cannot depend on an element that might fail");

void NonBlockingCannotFailRejectMightFail() {
  MockSourceNonBlockingMightFail source;
  // This should fail because NonBlocking (CannotFail) cannot depend on
  // MightFail
  ClockMcuxpressoSyncSelectorNonBlocking selector(source, 1, 0);
}

#elif PW_NC_TEST(NonBlockingCannotFailRejectBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

void NonBlockingCannotFailRejectBlocking() {
  MockSourceBlocking source;
  // This should fail because NonBlocking cannot depend on Blocking
  ClockMcuxpressoSyncSelectorNonBlocking selector(source, 1, 0);
}

#elif PW_NC_TEST(NonBlockingMightFailRejectBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

void NonBlockingMightFailRejectBlocking() {
  MockSourceBlocking source;
  // This should fail because NonBlocking cannot depend on Blocking
  ClockMcuxpressoSyncSelectorNonBlockingMightFail selector(source, 1, 0);
}

#elif PW_NC_TEST(ChangeSourceValidationCannotFailMightFail)
PW_NC_EXPECT("Non-failing element cannot depend on an element that might fail");

void ChangeSourceValidationCannotFailMightFail() {
  MockSourceNonBlockingCannotFail source1;
  MockSourceNonBlockingMightFail source2;
  ClockMcuxpressoSyncSelectorNonBlocking selector(source1, 1, 0);
  // This should fail because ChangeSource validates new_source type via
  // SetSource
  selector.ChangeSource(source2, 2);
}

#elif PW_NC_TEST(ChangeSourceValidationCannotFailBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

void ChangeSourceValidationCannotFailBlocking() {
  MockSourceNonBlockingCannotFail source1;
  MockSourceBlocking source2;
  ClockMcuxpressoSyncSelectorNonBlocking selector(source1, 1, 0);
  // This should fail because ChangeSource validates new_source type via
  // SetSource
  selector.ChangeSource(source2, 2);
}

#elif PW_NC_TEST(ChangeSourceValidationMightFailBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

void ChangeSourceValidationMightFailBlocking() {
  MockSourceNonBlockingCannotFail source1;
  MockSourceBlocking source2;
  ClockMcuxpressoSyncSelectorNonBlockingMightFail selector(source1, 1, 0);
  // This should fail because ChangeSource validates new_source type via
  // SetSource
  selector.ChangeSource(source2, 2);
}

#elif PW_NC_TEST(ChangeSourceValidationElementMightFailBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

void ChangeSourceValidationElementMightFailBlocking() {
  MockSourceNonBlockingCannotFail source1;
  MockSourceBlocking source2;
  Element& element_source2 = source2;
  ClockMcuxpressoSyncSelectorNonBlockingMightFail selector(source1, 1, 0);
  // This should fail because ChangeSource validates new_source type via
  // SetSource
  selector.ChangeSource(element_source2, 2);
}
#endif

}  // namespace
}  // namespace pw::clock_tree
