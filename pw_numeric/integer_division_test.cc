// Copyright 2024 The Pigweed Authors
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

#include "pw_numeric/integer_division.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

template <typename T>
constexpr void Sweep() {
  T dividend = T{0};
  T divisor = T{1};
  if constexpr (std::is_signed_v<T>) {
    dividend = T{-100};
    divisor = T{-100};
  }
  for (; dividend <= T{100}; ++dividend) {
    for (; divisor <= T{100}; ++divisor) {
      if (divisor == 0) {
        continue;
      }
      EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(dividend, divisor),
                static_cast<T>(std::round(static_cast<long double>(dividend) /
                                          static_cast<long double>(divisor))));
    }
  }
}

TEST(IntegerDivisionRoundNearestTest, Sweep) {
  Sweep<int8_t>();
  Sweep<uint8_t>();
  Sweep<int16_t>();
  Sweep<uint16_t>();
  Sweep<int32_t>();
  Sweep<uint32_t>();
  Sweep<int64_t>();
  Sweep<uint64_t>();
  Sweep<char>();
  Sweep<unsigned char>();
  Sweep<short>();
  Sweep<unsigned short>();
  Sweep<int>();
  Sweep<unsigned int>();
  Sweep<long>();
  Sweep<unsigned long>();
  Sweep<long long>();
  Sweep<unsigned long long>();
  Sweep<std::intmax_t>();
  Sweep<std::uintmax_t>();
}

template <typename T>
constexpr void BiasOverflow() {
  const T max = std::numeric_limits<T>::max();
  PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(max, max), T{1});
  PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(max - T{1}, max), T{1});
  PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(max / T{2} + T{1}, max),
                    T{1});
  PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(max / T{2}, max), T{0});
  PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(T{1}, max), T{0});

  if constexpr (std::is_signed_v<T>) {
    const T min = std::numeric_limits<T>::min();
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(min, min), T{1});
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(min + T{1}, min),
                      T{1});
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(min / T{2}, min),
                      T{1});
    PW_TEST_EXPECT_EQ(
        pw::IntegerDivisionRoundNearest<T>(min / T{2} + T{1}, min), T{0});
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(T{1}, min), T{0});

    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(min, max), T{-1});
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(min + T{1}, max),
                      T{-1});
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(min / T{2}, max),
                      T{-1});
    PW_TEST_EXPECT_EQ(
        pw::IntegerDivisionRoundNearest<T>(min / T{2} + T{1}, max), T{0});
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(T{-1}, max), T{0});

    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(max, min), T{-1});
    PW_TEST_EXPECT_EQ(
        pw::IntegerDivisionRoundNearest<T>(max / T{2} + T{1}, min), T{-1});
    PW_TEST_EXPECT_EQ(pw::IntegerDivisionRoundNearest<T>(max / T{2}, min),
                      T{0});
  }
}

PW_CONSTEXPR_TEST(IntegerDivisionRoundNearestTest, BiasOverflow, {
  BiasOverflow<int8_t>();
  BiasOverflow<uint8_t>();
  BiasOverflow<int16_t>();
  BiasOverflow<uint16_t>();
  BiasOverflow<int32_t>();
  BiasOverflow<uint32_t>();
  BiasOverflow<int64_t>();
  BiasOverflow<uint64_t>();
  BiasOverflow<char>();
  BiasOverflow<unsigned char>();
  BiasOverflow<short>();
  BiasOverflow<unsigned short>();
  BiasOverflow<int>();
  BiasOverflow<unsigned int>();
  BiasOverflow<long>();
  BiasOverflow<unsigned long>();
  BiasOverflow<long long>();
  BiasOverflow<unsigned long long>();
  BiasOverflow<std::intmax_t>();
  BiasOverflow<std::uintmax_t>();
});

}  // namespace
