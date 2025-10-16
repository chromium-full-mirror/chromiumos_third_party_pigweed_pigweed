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

#include "pw_containers/storage.h"

#include <cstdint>

#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::containers::Storage;
using ::pw::containers::StorageFor;

PW_CONSTEXPR_TEST(Storage, DefaultConstructor, {
  StorageFor<int> storage;
  PW_TEST_EXPECT_EQ(alignof(decltype(storage)), alignof(int));
  PW_TEST_EXPECT_EQ(storage.size(), sizeof(int));
  PW_TEST_EXPECT_FALSE(storage.empty());
});

PW_CONSTEXPR_TEST(Storage, ZeroSize, {
  StorageFor<int, 0> storage;
  PW_TEST_EXPECT_EQ(alignof(decltype(storage)), alignof(int));
  PW_TEST_EXPECT_EQ(storage.size(), 0u);
  PW_TEST_EXPECT_TRUE(storage.empty());
});

PW_CONSTEXPR_TEST(Storage, OddSize, {
  Storage<8, 5> storage;
  PW_TEST_EXPECT_EQ(alignof(decltype(storage)), 8u);
  PW_TEST_EXPECT_EQ(storage.size(), 5u);
});

PW_CONSTEXPR_TEST(Storage, MultipleItems, {
  StorageFor<int, 5> storage;
  PW_TEST_EXPECT_EQ(storage.size(), sizeof(int) * 5);
  PW_TEST_EXPECT_FALSE(storage.empty());
});

TEST(Storage, Data) {
  StorageFor<int, 5> storage;
  EXPECT_NE(storage.data(), nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(storage.data()) % alignof(int), 0u);
}

struct alignas(128) LargeAlignment {
  char data[32];
};

TEST(Storage, LargeAlignment) {
  StorageFor<LargeAlignment, 2> storage;
  static_assert(alignof(decltype(storage)) == alignof(LargeAlignment));

  EXPECT_EQ(storage.size(), sizeof(LargeAlignment) * 2);
  EXPECT_NE(storage.data(), nullptr);
  EXPECT_EQ(
      reinterpret_cast<uintptr_t>(storage.data()) % alignof(LargeAlignment),
      0u);
}

PW_CONSTEXPR_TEST(Storage, Fill, {
  Storage<alignof(uint32_t), 1> storage;
  storage.fill(std::byte{0xAB});
  for (size_t i = 0; i < storage.size(); ++i) {
    PW_TEST_EXPECT_EQ(storage.data()[i], std::byte{0xAB});
  }
});

}  // namespace
