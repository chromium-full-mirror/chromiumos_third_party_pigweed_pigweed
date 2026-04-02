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

#include "pw_numeric/rounding.h"

#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

PW_CONSTEXPR_TEST(RoundDownTest, ToMultipleOf10, {
  PW_TEST_EXPECT_EQ(pw::RoundDown(0, 10), 0);
  PW_TEST_EXPECT_EQ(pw::RoundDown(1, 10), 0);
  PW_TEST_EXPECT_EQ(pw::RoundDown(9, 10), 0);
  PW_TEST_EXPECT_EQ(pw::RoundDown(10, 10), 10);
  PW_TEST_EXPECT_EQ(pw::RoundDown(11, 10), 10);
});

PW_CONSTEXPR_TEST(RoundDownTest, NegativeNumbers, {
  PW_TEST_EXPECT_EQ(pw::RoundDown(-1, 10), -10);
  PW_TEST_EXPECT_EQ(pw::RoundDown(-9, 10), -10);
  PW_TEST_EXPECT_EQ(pw::RoundDown(-10, 10), -10);
  PW_TEST_EXPECT_EQ(pw::RoundDown(-11, 10), -20);
});

PW_CONSTEXPR_TEST(RoundDownTest, DifferentTypes, {
  PW_TEST_EXPECT_EQ(pw::RoundDown(uint16_t(0), uint8_t(10)), uint8_t(0));
  PW_TEST_EXPECT_EQ(pw::RoundDown(uint8_t(9), uint16_t(256)), uint8_t(0));
  PW_TEST_EXPECT_EQ(pw::RoundDown(int8_t(-1), uint16_t(10)), int8_t(-10));
  PW_TEST_EXPECT_EQ(pw::RoundDown(int16_t(-9), int8_t(10)), int16_t(-10));
});

PW_CONSTEXPR_TEST(RoundDownTest, EdgeCases, {
  PW_TEST_EXPECT_EQ(pw::RoundDown(uint8_t(255), 10), 250u);
  PW_TEST_EXPECT_EQ(pw::RoundDown(int8_t(127), 16), 112);
});

PW_CONSTEXPR_TEST(RoundUpTest, ToMultipleOf10, {
  PW_TEST_EXPECT_EQ(pw::RoundUp(0, 10), 0);
  PW_TEST_EXPECT_EQ(pw::RoundUp(1, 10), 10);
  PW_TEST_EXPECT_EQ(pw::RoundUp(9, 10), 10);
  PW_TEST_EXPECT_EQ(pw::RoundUp(10, 10), 10);
  PW_TEST_EXPECT_EQ(pw::RoundUp(11, 10), 20);
});

PW_CONSTEXPR_TEST(RoundUpTest, NegativeNumbers, {
  PW_TEST_EXPECT_EQ(pw::RoundUp(-1, 10), 0);
  PW_TEST_EXPECT_EQ(pw::RoundUp(-9, 10), 0);
  PW_TEST_EXPECT_EQ(pw::RoundUp(-10, 10), -10);
  PW_TEST_EXPECT_EQ(pw::RoundUp(-11, 10), -10);
});

PW_CONSTEXPR_TEST(RoundUpTest, DifferentTypes, {
  PW_TEST_EXPECT_EQ(pw::RoundUp(uint16_t(0), uint8_t(10)), uint16_t(0));
  PW_TEST_EXPECT_EQ(pw::RoundUp(uint8_t(9), uint16_t(10)), uint8_t(10));
  PW_TEST_EXPECT_EQ(pw::RoundUp(int8_t(-1), uint16_t(10)), int8_t(0));
  PW_TEST_EXPECT_EQ(pw::RoundUp(int16_t(-9), int8_t(10)), int16_t(0));
});

PW_CONSTEXPR_TEST(RoundUpTest, EdgeCases, {
  PW_TEST_EXPECT_EQ(pw::RoundUp(uint8_t(0), 10), 0u);
  PW_TEST_EXPECT_EQ(pw::RoundUp(int8_t(-128), 16), -128);
});

}  // namespace
