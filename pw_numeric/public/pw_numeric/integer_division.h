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
#pragma once

#include <cstdint>
#include <type_traits>

#include "pw_numeric/absolute_value.h"
#include "pw_numeric/checked_arithmetic.h"

namespace pw {

/// @module{pw_numeric}

/// Performs integer division and rounds to the nearest integer.
///
/// Yields the same result as
/// `std::round(static_cast<double>(dividend) / static_cast<double>(divisor))`,
/// but requires no floating point operation and is `constexpr`.
template <typename T>
constexpr T IntegerDivisionRoundNearest(T dividend, T divisor);

// Template method implementations.

namespace internal {

// Helper function that returns the value to add to the dividend in order to
// bias the division operation towards the nearest multiple of the divisor.
template <typename T>
constexpr T GetBias(T dividend, T divisor) {
  T bias = divisor / T{2};
  if constexpr (std::is_signed_v<T>) {
    if ((dividend < 0) != (divisor < 0)) {
      bias = static_cast<T>(-bias);
    }
  }
  return bias;
}

// Implementation version of `IntegerDivisionRoundNearest` for types smaller
// than std::intmax_t. Small types can be promoted to prevent overflow when
// biasing the dividend.
template <typename T>
constexpr T FastIntegerDivisionRoundNearest(T dividend, T divisor) {
  using U = std::conditional_t<
      sizeof(T) < sizeof(int),
      std::conditional_t<std::is_signed_v<T>, int, unsigned>,
      std::conditional_t<std::is_signed_v<T>, std::intmax_t, std::uintmax_t>>;
  const auto promoted = static_cast<U>(dividend);
  return static_cast<T>((promoted + GetBias(dividend, divisor)) / divisor);
}

// Implementation version of `IntegerDivisionRoundNearest` for types as large as
// std::intmax_t. This version uses the modulo operator to avoid adding a bias
// to the dividend that could overflow.
template <typename T>
constexpr T SlowIntegerDivisionRoundNearest(T dividend, T divisor) {
  T quotient = dividend / divisor;
  std::uintmax_t remainder = 0;
  std::uintmax_t threshold = 0;
  T bias = 1;
  if constexpr (std::is_signed_v<T>) {
    remainder = static_cast<std::uintmax_t>(abs(dividend % divisor));
    threshold = static_cast<std::uintmax_t>(pw::abs(divisor / 2)) +
                (divisor & 1 ? 1 : 0);
    bias = (dividend < 0) != (divisor < 0) ? T{-1} : T{1};
  } else {
    remainder = dividend % divisor;
    threshold = (divisor / 2) + (divisor & 1 ? 1 : 0);
  }
  if (remainder >= threshold) {
    quotient += bias;
  }
  return quotient;
}

}  // namespace internal

template <typename T>
constexpr T IntegerDivisionRoundNearest(T dividend, T divisor) {
  static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
  if constexpr (sizeof(T) < sizeof(std::intmax_t)) {
    return internal::FastIntegerDivisionRoundNearest(dividend, divisor);
  } else {
    if (CheckedIncrement(dividend, internal::GetBias(dividend, divisor))) {
      return dividend / divisor;
    }
    return internal::SlowIntegerDivisionRoundNearest(dividend, divisor);
  }
}

}  // namespace pw
