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

#include "pw_containers/internal/var_len_entry_iterator.h"

#include <string_view>

#include "pw_containers/inline_var_len_entry_queue.h"
#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::containers::internal::VarLenEntry;
using pw::containers::internal::VarLenEntryIterator;
using namespace std::literals::string_view_literals;

TEST(VarLenEntryIterator, DefaultConstructed) {
  VarLenEntryIterator<std::byte> it1;
  VarLenEntryIterator<std::byte> it2;
  EXPECT_EQ(it1, it2);
}

TEST(VarLenEntryIterator, IterationContiguous) {
  pw::InlineVarLenEntryQueue<10> queue;
  const auto data = "ABCDE"sv;
  queue.push(pw::as_bytes(pw::span(data)));
  VarLenEntry<const std::byte> entry = queue.front();

  auto it = entry.begin();
  EXPECT_EQ(*it, std::byte{'A'});
  ++it;
  EXPECT_EQ(*it, std::byte{'B'});
  it++;
  EXPECT_EQ(*it, std::byte{'C'});
  ++it;
  EXPECT_EQ(*it, std::byte{'D'});
  ++it;
  EXPECT_EQ(*it, std::byte{'E'});
  ++it;
  EXPECT_EQ(it, entry.end());
}

TEST(VarLenEntryIterator, IterationDiscontiguous) {
  pw::InlineVarLenEntryQueue<5> queue;
  queue.push(pw::as_bytes(pw::span("12"sv)));
  queue.push_overwrite(pw::as_bytes(pw::span("ABCDE"sv)));
  VarLenEntry<const std::byte> entry = queue.front();

  auto it = entry.begin();
  EXPECT_EQ(*it, std::byte{'A'});
  ++it;
  EXPECT_EQ(*it, std::byte{'B'});
  it++;
  EXPECT_EQ(*it, std::byte{'C'});
  ++it;
  EXPECT_EQ(*it, std::byte{'D'});
  ++it;
  EXPECT_EQ(*it, std::byte{'E'});
  ++it;
  EXPECT_EQ(it, entry.end());
}

TEST(VarLenEntryIterator, Addition) {
  pw::InlineVarLenEntryQueue<10> queue;
  const auto data = "0123456789"sv;
  queue.push(pw::as_bytes(pw::span(data)));
  VarLenEntry<const std::byte> entry = queue.front();

  auto it = entry.begin();
  EXPECT_EQ(*(it + 3), std::byte{'3'});
  EXPECT_EQ(*(5 + it), std::byte{'5'});

  it += 7;
  EXPECT_EQ(*it, std::byte{'7'});
}

TEST(VarLenEntryIterator, Equality) {
  pw::InlineVarLenEntryQueue<10> queue;
  queue.push(pw::as_bytes(pw::span("0123456789"sv)));
  VarLenEntry<const std::byte> entry = queue.front();

  auto it1 = entry.begin();
  auto it2 = entry.begin();
  EXPECT_EQ(it1, it2);

  ++it2;
  EXPECT_NE(it1, it2);

  it1 += 1;
  EXPECT_EQ(it1, it2);

  EXPECT_NE(it1, entry.end());
  EXPECT_NE(entry.end(), it2);
}

TEST(VarLenEntryIterator, Mutable) {
  pw::InlineVarLenEntryQueue<5> queue;
  queue.push(pw::as_bytes(pw::span("ABCDE"sv)));
  VarLenEntry<std::byte> entry = queue.front();

  auto it = entry.begin();
  *it = std::byte{'Z'};
  *(it + 1) = std::byte{'Y'};
  *(it + 4) = std::byte{'X'};

  std::array<std::byte, 5> copy_buffer;
  entry.copy(copy_buffer.data(), copy_buffer.size());
  EXPECT_EQ(std::memcmp(copy_buffer.data(), "ZYCDX", 5), 0);
}

}  // namespace
