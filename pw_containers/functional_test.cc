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

#include "pw_containers/functional.h"

#include <string>

#include "pw_unit_test/framework.h"

namespace {

TEST(EqualTo, BasicTypes) {
  pw::EqualTo equal_to;

  EXPECT_TRUE(equal_to(1, 1));
  EXPECT_FALSE(equal_to(1, 2));
  EXPECT_TRUE(equal_to(1.0, 1.0));
  EXPECT_FALSE(equal_to(1.0, 2.0));
  EXPECT_TRUE(equal_to('a', 'a'));
  EXPECT_FALSE(equal_to('a', 'b'));
  EXPECT_TRUE(equal_to(true, true));
  EXPECT_FALSE(equal_to(true, false));
  const char* str1 = "hello";
  const char* str2 = "hello";
  const char* str3 = "world";
  EXPECT_TRUE(equal_to(std::string(str1), std::string(str2)));
  EXPECT_FALSE(equal_to(std::string(str1), std::string(str3)));
}

TEST(EqualTo, MixedTypes) {
  pw::EqualTo equal_to;

  EXPECT_TRUE(equal_to(1, 1L));
  EXPECT_TRUE(equal_to(1, 1.0));
  EXPECT_FALSE(equal_to(1, 2.0));
  EXPECT_TRUE(equal_to(0, false));
  EXPECT_TRUE(equal_to(1, true));
  EXPECT_FALSE(equal_to(2, true));
}

TEST(EqualTo, Pointers) {
  pw::EqualTo equal_to;
  int x = 5;
  int y = 5;
  int* p1 = &x;
  int* p2 = &x;
  int* p3 = &y;
  int* null_ptr = nullptr;

  EXPECT_TRUE(equal_to(p1, p2));
  EXPECT_FALSE(equal_to(p1, p3));
  EXPECT_TRUE(equal_to(null_ptr, nullptr));
  EXPECT_FALSE(equal_to(p1, null_ptr));
}

template <typename T>
struct Comparable {
  T value;
  bool operator==(const Comparable<T>& other) const {
    return value == other.value;
  }
};

template <typename T>
bool operator==(const Comparable<T>& lhs, const T& rhs) {
  return lhs.value == rhs;
}

template <typename T>
bool operator==(const T& lhs, const Comparable<T>& rhs) {
  return lhs == rhs.value;
}

TEST(EqualTo, UserDefinedTypes) {
  pw::EqualTo equal_to;

  Comparable<int> c1{1};
  Comparable<int> c2{1};
  Comparable<int> c3{2};

  EXPECT_TRUE(equal_to(c1, c2));
  EXPECT_FALSE(equal_to(c1, c3));
  EXPECT_TRUE(equal_to(c1, 1));
  EXPECT_FALSE(equal_to(c1, 2));
  EXPECT_TRUE(equal_to(1, c1));
  EXPECT_FALSE(equal_to(2, c1));

  Comparable<double> cd1{1.0};
  Comparable<double> cd2{1.0};
  Comparable<double> cd3{2.0};

  EXPECT_TRUE(equal_to(cd1, cd2));
  EXPECT_FALSE(equal_to(cd1, cd3));
  EXPECT_TRUE(equal_to(cd1, 1.0));
  EXPECT_FALSE(equal_to(cd1, 2.0));
  EXPECT_TRUE(equal_to(1.0, cd1));
  EXPECT_FALSE(equal_to(2.0, cd1));

  Comparable<std::string> cs1{"hello"};
  Comparable<std::string> cs2{"hello"};
  Comparable<std::string> cs3{"world"};

  EXPECT_TRUE(equal_to(cs1, cs2));
  EXPECT_FALSE(equal_to(cs1, cs3));
  EXPECT_TRUE(equal_to(cs1, std::string("hello")));
  EXPECT_FALSE(equal_to(cs1, std::string("world")));
  EXPECT_TRUE(equal_to(std::string("hello"), cs1));
  EXPECT_FALSE(equal_to(std::string("world"), cs1));
}

}  // namespace
