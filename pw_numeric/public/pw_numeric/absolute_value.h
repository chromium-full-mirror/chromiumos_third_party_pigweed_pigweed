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
#pragma once

#include <cmath>

#if defined(__cpp_lib_constexpr_cmath) && __cpp_lib_constexpr_cmath >= 202202L

#include <cstdlib>

/// As of C++23, std::abs and related are constexpr.
namespace pw {

using std::abs;
using std::fabs;
using std::fabsf;
using std::fabsl;
using std::imaxabs;
using std::labs;
using std::llabs;

}  // namespace pw

#else  // __cpp_lib_constexpr_cmath

#include <cstdint>
#include <type_traits>

namespace pw {

constexpr int abs(int num) { return num < 0 ? static_cast<int>(-num) : num; }

constexpr long abs(long num) {
  return num < 0L ? static_cast<long>(-num) : num;
}

constexpr long labs(long num) {
  return num < 0L ? static_cast<long>(-num) : num;
}

constexpr long long abs(long long num) {
  return num < 0LL ? static_cast<long long>(-num) : num;
}

constexpr long long llabs(long long num) {
  return num < 0LL ? static_cast<long long>(-num) : num;
}

constexpr std::intmax_t imaxabs(std::intmax_t num) {
  return num < std::intmax_t{} ? static_cast<std::intmax_t>(-num) : num;
}

constexpr float abs(float num) {
  return num < 0.f ? static_cast<float>(-num) : num;
}

constexpr float fabs(float num) {
  return num < 0.f ? static_cast<float>(-num) : num;
}

constexpr float fabsf(float num) {
  return num < 0.f ? static_cast<float>(-num) : num;
}

constexpr double abs(double num) {
  return num < 0. ? static_cast<double>(-num) : num;
}

constexpr double fabs(double num) {
  return num < 0. ? static_cast<double>(-num) : num;
}

constexpr long double abs(long double num) {
  return num < 0.L ? static_cast<long double>(-num) : num;
}

constexpr long double fabs(long double num) {
  return num < 0.L ? static_cast<long double>(-num) : num;
}

constexpr long double fabsl(long double num) {
  return num < 0.L ? static_cast<long double>(-num) : num;
}

template <typename Integer>
constexpr double fabs(Integer num) {
  return fabs(static_cast<double>(num));
}

}  // namespace pw

#endif  // __cpp_lib_constexpr_cmath
