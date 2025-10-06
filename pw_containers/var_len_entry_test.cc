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

#include "pw_containers/internal/var_len_entry.h"

#include <array>
#include <cstddef>
#include <string_view>

#include "pw_containers/inline_var_len_entry_queue.h"
#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::containers::internal::VarLenEntry;
using namespace std::literals::string_view_literals;

PW_CONSTEXPR_TEST(VarLenEntry, DefaultConstructed, {
  VarLenEntry<std::byte> entry;
  PW_TEST_EXPECT_TRUE(entry.empty());
  PW_TEST_EXPECT_EQ(entry.size(), 0u);
  PW_TEST_EXPECT_EQ(entry.begin(), entry.end());

  VarLenEntry<const std::byte> const_entry;
  PW_TEST_EXPECT_TRUE(const_entry.empty());
  PW_TEST_EXPECT_EQ(const_entry.size(), 0u);
  PW_TEST_EXPECT_EQ(const_entry.begin(), const_entry.end());
});

TEST(VarLenEntry, Contiguous) {
  pw::InlineVarLenEntryQueue<10> queue;
  const auto data = "ABCDE"sv;
  queue.push(as_bytes(pw::span(data)));

  VarLenEntry<const std::byte> entry = queue.front();

  EXPECT_FALSE(entry.empty());
  EXPECT_EQ(entry.size(), 5u);

  EXPECT_EQ(entry.front(), std::byte{'A'});
  EXPECT_EQ(entry.back(), std::byte{'E'});

  EXPECT_EQ(entry[0], std::byte{'A'});
  EXPECT_EQ(entry[1], std::byte{'B'});
  EXPECT_EQ(entry[2], std::byte{'C'});
  EXPECT_EQ(entry[3], std::byte{'D'});
  EXPECT_EQ(entry[4], std::byte{'E'});

  EXPECT_EQ(entry.at(2), std::byte{'C'});

  auto [span1, span2] = entry.contiguous_data();
  EXPECT_EQ(span1.size(), 5u);
  EXPECT_TRUE(span2.empty());
  EXPECT_EQ(std::memcmp(span1.data(), data.data(), data.size()), 0);

  std::array<std::byte, 5> copy_buffer;
  EXPECT_EQ(entry.copy(copy_buffer.data(), copy_buffer.size()), 5u);
  EXPECT_EQ(std::memcmp(copy_buffer.data(), data.data(), data.size()), 0);

  std::array<std::byte, 3> small_copy_buffer;
  EXPECT_EQ(entry.copy(small_copy_buffer.data(), small_copy_buffer.size()), 5u);
  EXPECT_EQ(small_copy_buffer[0], std::byte{'A'});
  EXPECT_EQ(small_copy_buffer[1], std::byte{'B'});
  EXPECT_EQ(small_copy_buffer[2], std::byte{'C'});

  size_t i = 0;
  for (const auto& byte : entry) {
    EXPECT_EQ(byte, static_cast<std::byte>(data[i++]));
  }
  EXPECT_EQ(i, 5u);
}

TEST(VarLenEntry, Discontiguous) {
  pw::InlineVarLenEntryQueue<5> queue;
  queue.push(as_bytes(pw::span("12"sv)));
  queue.push_overwrite(as_bytes(pw::span("ABCDE"sv)));

  VarLenEntry<const std::byte> entry = queue.front();

  EXPECT_FALSE(entry.empty());
  EXPECT_EQ(entry.size(), 5u);

  EXPECT_EQ(entry.front(), std::byte{'A'});
  EXPECT_EQ(entry.back(), std::byte{'E'});

  EXPECT_EQ(entry[0], std::byte{'A'});
  EXPECT_EQ(entry[1], std::byte{'B'});
  EXPECT_EQ(entry[2], std::byte{'C'});
  EXPECT_EQ(entry[3], std::byte{'D'});
  EXPECT_EQ(entry[4], std::byte{'E'});

  EXPECT_EQ(entry.at(3), std::byte{'D'});

  auto [span1, span2] = entry.contiguous_data();
  EXPECT_FALSE(span1.empty());
  EXPECT_FALSE(span2.empty());
  EXPECT_EQ(span1.size() + span2.size(), 5u);

  std::array<std::byte, 5> copy_buffer;
  EXPECT_EQ(entry.copy(copy_buffer.data(), copy_buffer.size()), 5u);
  EXPECT_EQ(std::memcmp(copy_buffer.data(), "ABCDE", 5), 0);

  std::array<std::byte, 3> small_copy_buffer;
  EXPECT_EQ(entry.copy(small_copy_buffer.data(), small_copy_buffer.size()), 5u);
  EXPECT_EQ(std::memcmp(small_copy_buffer.data(), "ABC", 3), 0);

  size_t i = 0;
  const auto data = "ABCDE"sv;
  for (const auto& byte : entry) {
    EXPECT_EQ(byte, static_cast<std::byte>(data[i++]));
  }
  EXPECT_EQ(i, 5u);
}

TEST(VarLenEntry, ConstConversion) {
  pw::InlineVarLenEntryQueue<5> queue;
  queue.push(as_bytes(pw::span("12"sv)));
  queue.push_overwrite(as_bytes(pw::span("ABC"sv)));

  VarLenEntry<std::byte> entry = queue.front();
  VarLenEntry<const std::byte> const_entry = entry;

  EXPECT_EQ(const_entry.size(), 3u);
  EXPECT_EQ(const_entry[0], std::byte{'A'});
  EXPECT_EQ(const_entry[1], std::byte{'B'});
  EXPECT_EQ(const_entry[2], std::byte{'C'});
}

TEST(VarLenEntry, Equality) {
  pw::InlineVarLenEntryQueue<10> queue;
  queue.push(as_bytes(pw::span("ABC"sv)));
  queue.push(as_bytes(pw::span("ABC"sv)));

  VarLenEntry<const std::byte> entry1 = queue.front();
  queue.pop();
  VarLenEntry<const std::byte> entry2 = queue.front();

  // These entries have the same content but are at different locations.
  EXPECT_NE(entry1, entry2);

  // Create a copy of the iterator to get an equal entry.
  auto it = queue.begin();
  VarLenEntry<const std::byte> entry3 = *it;
  EXPECT_EQ(entry2, entry3);
}

TEST(VarLenEntry, Mutable) {
  pw::InlineVarLenEntryQueue<5> queue;
  queue.push(as_bytes(pw::span("ABCDE"sv)));

  VarLenEntry<std::byte> entry = queue.front();
  entry[0] = std::byte{'Z'};
  entry.at(1) = std::byte{'Y'};
  entry.back() = std::byte{'X'};

  std::array<std::byte, 5> copy_buffer;
  entry.copy(copy_buffer.data(), copy_buffer.size());
  EXPECT_EQ(std::memcmp(copy_buffer.data(), "ZYCDX", 5), 0);
}

}  // namespace
