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

#include "pw_containers/internal/var_len_entry_queue_iterator.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "pw_containers/inline_var_len_entry_queue.h"
#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::containers::internal::VarLenEntry;
using pw::containers::internal::VarLenEntryQueueIterator;
using namespace std::literals::string_view_literals;

TEST(VarLenEntryQueueIterator, DefaultConstructed) {
  VarLenEntryQueueIterator<std::byte> it;
  VarLenEntryQueueIterator<std::byte> other;
  EXPECT_EQ(it, other);
  EXPECT_FALSE(it != other);
}

class VarLenEntryQueueIteratorTest : public ::testing::Test {
 protected:
  VarLenEntryQueueIteratorTest() {
    queue_.push(pw::as_bytes(pw::span("hello"sv)));
    queue_.push(pw::as_bytes(pw::span("world"sv)));
    queue_.push(pw::as_bytes(pw::span("test"sv)));
  }

  pw::InlineVarLenEntryQueue<20> queue_;
};

TEST_F(VarLenEntryQueueIteratorTest, Equality) {
  VarLenEntryQueueIterator<std::byte> it1 = queue_.begin();
  VarLenEntryQueueIterator<std::byte> it2 = queue_.begin();
  VarLenEntryQueueIterator<std::byte> it3 = ++(queue_.begin());

  EXPECT_EQ(it1, it2);
  EXPECT_NE(it1, it3);
  EXPECT_TRUE(it1 == it2);
  EXPECT_TRUE(it1 != it3);
}

TEST_F(VarLenEntryQueueIteratorTest, Copy) {
  VarLenEntryQueueIterator<std::byte> it1 = queue_.begin();
  VarLenEntryQueueIterator<std::byte> it2(it1);
  EXPECT_EQ(it1, it2);

  VarLenEntryQueueIterator<std::byte> it3;
  it3 = it1;
  EXPECT_EQ(it1, it3);
}

TEST_F(VarLenEntryQueueIteratorTest, Dereference) {
  VarLenEntryQueueIterator<std::byte> it = queue_.begin();

  const VarLenEntry<std::byte>& entry = *it;
  std::array<std::byte, 5> expected;
  memcpy(expected.data(), "hello", expected.size());
  EXPECT_EQ(entry.size(), 5u);
  EXPECT_TRUE(std::equal(entry.begin(), entry.end(), expected.begin()));
}

TEST_F(VarLenEntryQueueIteratorTest, Arrow) {
  VarLenEntryQueueIterator<std::byte> it = queue_.begin();

  std::array<std::byte, 5> expected;
  memcpy(expected.data(), "hello", expected.size());
  EXPECT_EQ(it->size(), 5u);
  EXPECT_TRUE(std::equal(it->begin(), it->end(), expected.begin()));
}

TEST_F(VarLenEntryQueueIteratorTest, PreIncrement) {
  VarLenEntryQueueIterator<std::byte> it = queue_.begin();
  ++it;

  const VarLenEntry<std::byte>& entry = *it;
  std::array<std::byte, 5> expected;
  memcpy(expected.data(), "world", expected.size());
  EXPECT_EQ(entry.size(), 5u);
  EXPECT_TRUE(std::equal(entry.begin(), entry.end(), expected.begin()));
}

TEST_F(VarLenEntryQueueIteratorTest, PostIncrement) {
  VarLenEntryQueueIterator<std::byte> it = queue_.begin();
  VarLenEntryQueueIterator<std::byte> prev_it = it++;

  std::array<std::byte, 5> prev_expected;
  memcpy(prev_expected.data(), "hello", prev_expected.size());
  EXPECT_EQ(prev_it->size(), 5u);
  EXPECT_TRUE(
      std::equal(prev_it->begin(), prev_it->end(), prev_expected.begin()));

  std::array<std::byte, 5> expected;
  memcpy(expected.data(), "world", expected.size());
  EXPECT_EQ(it->size(), 5u);
  EXPECT_TRUE(std::equal(it->begin(), it->end(), expected.begin()));
}

TEST_F(VarLenEntryQueueIteratorTest, Iteration) {
  VarLenEntryQueueIterator<std::byte> it = queue_.begin();

  // First item
  std::array<std::byte, 5> expected1;
  memcpy(expected1.data(), "hello", expected1.size());
  EXPECT_EQ(it->size(), 5u);
  EXPECT_TRUE(std::equal(it->begin(), it->end(), expected1.begin()));

  ++it;

  // Second item
  std::array<std::byte, 5> expected2;
  memcpy(expected2.data(), "world", expected2.size());
  EXPECT_EQ(it->size(), 5u);
  EXPECT_TRUE(std::equal(it->begin(), it->end(), expected2.begin()));

  ++it;

  // Third item
  std::array<std::byte, 4> expected3;
  memcpy(expected3.data(), "test", expected3.size());
  EXPECT_EQ(it->size(), 4u);
  EXPECT_TRUE(std::equal(it->begin(), it->end(), expected3.begin()));
}

}  // namespace
