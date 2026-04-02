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

#include "pw_clock_tree/clock_tree.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_unit_test/framework.h"

namespace pw::clock_tree {
namespace {

TEST(ClockTreeDependency, CompileTimeTests) {}

#if PW_NC_TEST(NonBlockingCannotFailDependsOnBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

// 1. ElementNonBlockingCannotFail depending on ElementBlocking
void NonBlockingCannotFailDependsOnBlocking() {
  ClockSourceNoOpBlocking source_blocking;
  class DependentNonBlocking
      : public DependentElement<ElementNonBlockingCannotFail> {
   public:
    constexpr DependentNonBlocking(ElementBlocking& source)
        : DependentElement<ElementNonBlockingCannotFail>(source) {}
    Status DoEnable() override { return OkStatus(); }
  };
  DependentNonBlocking dep(source_blocking);
}

#elif PW_NC_TEST(NonBlockingMightFailDependsOnBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

// 2. ElementNonBlockingMightFail depending on ElementBlocking
void NonBlockingMightFailDependsOnBlocking() {
  ClockSourceNoOpBlocking source_blocking;
  class DependentNonBlocking
      : public DependentElement<ElementNonBlockingMightFail> {
   public:
    constexpr DependentNonBlocking(ElementBlocking& source)
        : DependentElement<ElementNonBlockingMightFail>(source) {}
    Status DoEnable() override { return OkStatus(); }
  };
  DependentNonBlocking dep(source_blocking);
}

#elif PW_NC_TEST(NonBlockingCannotFailDependsOnNonBlockingMightFail)
PW_NC_EXPECT("Non-failing element cannot depend on an element that might fail");

// 3. ElementNonBlockingCannotFail depending on ElementNonBlockingMightFail
void NonBlockingCannotFailDependsOnNonBlockingMightFail() {
  class ClockSourceNonBlockingMightFail
      : public ClockSource<ElementNonBlockingMightFail> {
   public:
    Status DoEnable() override { return OkStatus(); }
  };
  ClockSourceNonBlockingMightFail source_might_fail;

  class DependentNonBlocking
      : public DependentElement<ElementNonBlockingCannotFail> {
   public:
    constexpr DependentNonBlocking(ElementNonBlockingMightFail& source)
        : DependentElement<ElementNonBlockingCannotFail>(source) {}
    Status DoEnable() override { return OkStatus(); }
  };
  DependentNonBlocking dep(source_might_fail);
}

#elif PW_NC_TEST(ClockDividerNonBlockingCannotFailDependsOnBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

// 4. ClockDividerNonBlockingCannotFail depending on ElementBlocking
void ClockDividerNonBlockingCannotFailDependsOnBlocking() {
  ClockSourceNoOpBlocking source_blocking;
  class MyDivider : public ClockDividerElement<ElementNonBlockingCannotFail> {
   public:
    constexpr MyDivider(ElementBlocking& source, uint32_t divider)
        : ClockDividerElement<ElementNonBlockingCannotFail>(source, divider) {}
    Status DoEnable() override { return OkStatus(); }
  };
  MyDivider divider(source_blocking, 2);
}

#elif PW_NC_TEST(ClockDividerNonBlockingCannotFailDependsOnNonBlockingMightFail)
PW_NC_EXPECT("Non-failing element cannot depend on an element that might fail");

// 5. ClockDividerNonBlockingCannotFail depending on ElementNonBlockingMightFail
void ClockDividerNonBlockingCannotFailDependsOnNonBlockingMightFail() {
  class ClockSourceNonBlockingMightFail
      : public ClockSource<ElementNonBlockingMightFail> {
   public:
    Status DoEnable() override { return OkStatus(); }
  };
  ClockSourceNonBlockingMightFail source_might_fail;

  class MyDivider : public ClockDividerElement<ElementNonBlockingCannotFail> {
   public:
    constexpr MyDivider(ElementNonBlockingMightFail& source, uint32_t divider)
        : ClockDividerElement<ElementNonBlockingCannotFail>(source, divider) {}
    Status DoEnable() override { return OkStatus(); }
  };
  MyDivider divider(source_might_fail, 2);
}

#elif PW_NC_TEST(ClockDividerNonBlockingMightFailDependsOnBlocking)
PW_NC_EXPECT("Non-blocking element cannot depend on a blocking element");

// 6. ClockDividerNonBlockingMightFail depending on ElementBlocking
void ClockDividerNonBlockingMightFailDependsOnBlocking() {
  ClockSourceNoOpBlocking source_blocking;
  class MyDivider : public ClockDividerElement<ElementNonBlockingMightFail> {
   public:
    constexpr MyDivider(ElementBlocking& source, uint32_t divider)
        : ClockDividerElement<ElementNonBlockingMightFail>(source, divider) {}
    Status DoEnable() override { return OkStatus(); }
  };
  MyDivider divider(source_blocking, 2);
}
#endif  // PW_NC_TEST

}  // namespace
}  // namespace pw::clock_tree
