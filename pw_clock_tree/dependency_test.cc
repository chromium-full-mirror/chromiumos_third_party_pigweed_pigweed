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
#include "pw_unit_test/framework.h"

namespace pw::clock_tree {
namespace {

// 1. ElementNonBlockingMightFail depending on ElementNonBlockingCannotFail
TEST(ClockTreeDependency, NonBlockingMightFailDependsOnNonBlockingCannotFail) {
  ClockSourceNoOp source_cannot_fail;
  class DependentMightFail
      : public DependentElement<ElementNonBlockingMightFail> {
   public:
    constexpr DependentMightFail(ElementNonBlockingCannotFail& source)
        : DependentElement<ElementNonBlockingMightFail>(source) {}
    Status DoEnable() override { return OkStatus(); }
  };
  DependentMightFail dep(source_cannot_fail);
  EXPECT_EQ(dep.Acquire(), OkStatus());
  EXPECT_EQ(dep.Release(), OkStatus());
}

// 2. ElementBlocking depending on ElementNonBlockingMightFail
TEST(ClockTreeDependency, BlockingDependsOnNonBlockingMightFail) {
  class ClockSourceNonBlockingMightFail
      : public ClockSource<ElementNonBlockingMightFail> {
   public:
    Status DoEnable() override { return OkStatus(); }
  };
  ClockSourceNonBlockingMightFail source_might_fail;

  class DependentBlocking : public DependentElement<ElementBlocking> {
   public:
    constexpr DependentBlocking(ElementNonBlockingMightFail& source)
        : DependentElement<ElementBlocking>(source) {}
    Status DoEnable() override { return OkStatus(); }
  };
  DependentBlocking dep(source_might_fail);
  EXPECT_EQ(dep.Acquire(), OkStatus());
  EXPECT_EQ(dep.Release(), OkStatus());
}

// 3. ElementBlocking depending on ElementNonBlockingCannotFail
TEST(ClockTreeDependency, BlockingDependsOnNonBlockingCannotFail) {
  ClockSourceNoOp source_cannot_fail;
  class DependentBlocking : public DependentElement<ElementBlocking> {
   public:
    constexpr DependentBlocking(ElementNonBlockingCannotFail& source)
        : DependentElement<ElementBlocking>(source) {}
    Status DoEnable() override { return OkStatus(); }
  };
  DependentBlocking dep(source_cannot_fail);
  EXPECT_EQ(dep.Acquire(), OkStatus());
  EXPECT_EQ(dep.Release(), OkStatus());
}

// 4. ClockDividerElement with different source type
TEST(ClockTreeDependency, ClockDividerBlockingDependsOnNonBlockingCannotFail) {
  ClockSourceNoOp source_cannot_fail;
  class MyDivider : public ClockDividerElement<ElementBlocking> {
   public:
    constexpr MyDivider(ElementNonBlockingCannotFail& source, uint32_t divider)
        : ClockDividerElement<ElementBlocking>(source, divider) {}
    Status DoEnable() override { return OkStatus(); }
  };
  MyDivider divider(source_cannot_fail, 2);
  EXPECT_EQ(divider.Acquire(), OkStatus());
  EXPECT_EQ(divider.Release(), OkStatus());
}

// 5. Clock divider chain: Blocking -> NonBlockingMightFail ->
// NonBlockingCannotFail
TEST(ClockTreeDependency, ClockDividerChain) {
  ClockSourceNoOp source_cannot_fail;

  class MyDividerMightFail
      : public ClockDividerElement<ElementNonBlockingMightFail> {
   public:
    constexpr MyDividerMightFail(ElementNonBlockingCannotFail& source,
                                 uint32_t divider)
        : ClockDividerElement<ElementNonBlockingMightFail>(source, divider) {}
    Status DoEnable() override { return OkStatus(); }
  };
  MyDividerMightFail divider_might_fail(source_cannot_fail, 2);

  class MyDividerBlocking : public ClockDividerElement<ElementBlocking> {
   public:
    constexpr MyDividerBlocking(ElementNonBlockingMightFail& source,
                                uint32_t divider)
        : ClockDividerElement<ElementBlocking>(source, divider) {}
    Status DoEnable() override { return OkStatus(); }
  };
  MyDividerBlocking divider_blocking(divider_might_fail, 4);

  EXPECT_EQ(divider_blocking.Acquire(), OkStatus());
  EXPECT_EQ(divider_blocking.Release(), OkStatus());
}

// 6. Clock divider: Blocking -> NonBlockingCannotFail
TEST(ClockTreeDependency, BlockingDependsOnNonBlockingCannotFailDivider) {
  ClockSourceNoOp source_cannot_fail;
  class MyDividerBlocking : public ClockDividerElement<ElementBlocking> {
   public:
    constexpr MyDividerBlocking(ElementNonBlockingCannotFail& source,
                                uint32_t divider)
        : ClockDividerElement<ElementBlocking>(source, divider) {}
    Status DoEnable() override { return OkStatus(); }
  };
  MyDividerBlocking divider_blocking(source_cannot_fail, 2);

  EXPECT_EQ(divider_blocking.Acquire(), OkStatus());
  EXPECT_EQ(divider_blocking.Release(), OkStatus());
}

// 7. Test various ClockSource aliases
TEST(ClockTreeDependency, ClockSourceBlocking) {
  class MyClockSource : public ClockSourceBlocking {
   public:
    Status DoEnable() override { return OkStatus(); }
  };
  MyClockSource source;
  EXPECT_EQ(source.Acquire(), OkStatus());
  EXPECT_EQ(source.Release(), OkStatus());
}

TEST(ClockTreeDependency, ClockSourceNonBlockingMightFail) {
  class MyClockSource : public ClockSourceNonBlockingMightFail {
   public:
    Status DoEnable() override { return OkStatus(); }
  };
  MyClockSource source;
  EXPECT_EQ(source.Acquire(), OkStatus());
  EXPECT_EQ(source.Release(), OkStatus());
}

TEST(ClockTreeDependency, ClockSourceNonBlockingCannotFail) {
  class MyClockSource : public ClockSourceNonBlockingCannotFail {
   public:
    Status DoEnable() override { return OkStatus(); }
  };
  MyClockSource source;
  source.Acquire();  // returns void
  source.Release();  // returns void
}

}  // namespace
}  // namespace pw::clock_tree
