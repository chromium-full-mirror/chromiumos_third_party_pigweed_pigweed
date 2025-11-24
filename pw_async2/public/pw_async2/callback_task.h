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

#include <functional>
#include <type_traits>

#include "pw_async2/future.h"
#include "pw_async2/poll.h"
#include "pw_async2/task.h"
#include "pw_function/function.h"

namespace pw::async2 {
namespace internal {

template <typename FutureType>
using CallbackType = std::conditional_t<
    std::is_same_v<typename FutureType::value_type, ReadyType>,
    Function<void()>,
    Function<void(typename FutureType::value_type)>>;

}  // namespace internal

/// @submodule{pw_async2,adapters}

/// A `Task` which pends a future and invokes a provided callback
/// with its output when it returns `Ready`.
///
/// A `FutureCallbackTask` terminates after the underlying future returns
/// `Ready` and can be cleaned up afterwards.
template <typename FutureType,
          typename Func = internal::CallbackType<FutureType>>
class FutureCallbackTask final : public Task {
 public:
  using value_type = typename FutureType::value_type;

  static_assert(is_future_v<FutureType>,
                "FutureCallbackTask can only be used with Future types");

  FutureCallbackTask(FutureType&& future, Func&& callback)
      : future_(std::move(future)), callback_(std::move(callback)) {}

  ~FutureCallbackTask() override { Deregister(); }

 private:
  Poll<> DoPend(Context& cx) final {
    Poll<value_type> poll = future_.Pend(cx);
    if (poll.IsPending()) {
      return Pending();
    }

    if constexpr (std::is_same_v<value_type, ReadyType>) {
      callback_();
    } else {
      callback_(std::move(*poll));
    }

    return Ready();
  }

  FutureType future_;
  Func callback_;
};

template <typename FutureType, typename Func>
FutureCallbackTask(FutureType&&, Func&&)
    -> FutureCallbackTask<FutureType, Func>;

// TODO: b/458069794 - Add StreamCallbackTask.

/// @}

}  // namespace pw::async2
