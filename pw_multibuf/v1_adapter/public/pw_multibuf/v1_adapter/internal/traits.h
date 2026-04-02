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
#pragma once

#include <optional>
#include <type_traits>
#include <variant>

namespace pw::multibuf::v1_adapter::internal {

// Type traits to detect `std::optional<...>`, `std::variant<...>`, etc. These
// have implicit move-constructors and/or move-assignment operators, and lead to
// ambiguity with some of MultiBuf's conversion operators.

template <typename T, template <typename...> class Template>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template<Args...>, Template> : std::true_type {};

template <typename T, template <typename...> class Template>
constexpr bool is_specialization_of_v =
    is_specialization_of<std::remove_cv_t<std::remove_reference_t<T>>,
                         Template>::value;

template <typename T>
constexpr bool is_optional_v = is_specialization_of_v<T, std::optional>;

template <typename T>
constexpr bool is_variant_v = is_specialization_of_v<T, std::variant>;

}  // namespace pw::multibuf::v1_adapter::internal
