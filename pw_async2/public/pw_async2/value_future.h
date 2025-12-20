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

#include "pw_assert/assert.h"
#include "pw_async2/future.h"

namespace pw::async2 {

template <typename T>
class ValueProvider;
template <typename T>
class BroadcastValueProvider;

/// @submodule{pw_async2,futures}

/// A future that holds a single value.
///
/// A `ValueFuture` is a concrete `Future` implementation that is vended by a
/// `ValueProvider` or a `BroadcastValueProvider`. It waits until the provider
/// resolves it with a value.
template <typename T>
class ValueFuture : public ListableFutureWithWaker<ValueFuture<T>, T> {
 public:
  using Base = ListableFutureWithWaker<ValueFuture<T>, T>;

  ValueFuture(ValueFuture&& other) noexcept
      : Base(Base::kMovedFrom), value_(std::move(other.value_)) {
    Base::MoveFrom(other);
  }

  ValueFuture& operator=(ValueFuture&& other) noexcept {
    if (this != &other) {
      value_ = std::move(other.value_);
      Base::MoveFrom(other);
    }
    return *this;
  }

  /// Creates a `ValueFuture` that is already resolved with the given value.
  static ValueFuture Resolved(T value) {
    return ValueFuture(std::in_place, std::move(value));
  }

  /// Creates a `ValueFuture` that is already resolved by constructing its
  /// value in-place.
  template <typename... Args>
  static ValueFuture Resolved(Args&&... args) {
    return ValueFuture(std::in_place, std::forward<Args>(args)...);
  }

 private:
  friend Base;
  friend class ValueProvider<T>;
  friend class BroadcastValueProvider<T>;

  static constexpr const char kWaitReason[] = "ValueFuture";

  explicit ValueFuture(ListFutureProvider<ValueFuture<T>>& provider)
      : Base(provider) {}
  explicit ValueFuture(SingleFutureProvider<ValueFuture<T>>& provider)
      : Base(provider) {}

  template <typename... Args>
  explicit ValueFuture(std::in_place_t, Args&&... args)
      : Base(Base::kReadyForCompletion),
        value_(std::in_place, std::forward<Args>(args)...) {}

  template <typename... Args>
  void Resolve(Args&&... args) {
    {
      std::lock_guard guard(Base::lock());
      PW_ASSERT(!value_.has_value());
      value_.emplace(std::forward<Args>(args)...);
      this->unlist();
    }
    Base::Wake();
  }

  Poll<T> DoPend(Context&) {
    std::lock_guard guard(Base::lock());
    if (value_.has_value()) {
      T value = std::move(value_.value());
      value_.reset();
      return Ready(std::move(value));
    }
    return Pending();
  }

  std::optional<T> value_;
};

/// Specialization for a future that does not return any value, just a
/// completion signal.
template <>
class ValueFuture<void>
    : public ListableFutureWithWaker<ValueFuture<void>, void> {
 public:
  using Base = ListableFutureWithWaker<ValueFuture<void>, void>;

  ValueFuture(ValueFuture&& other) noexcept
      : Base(Base::kMovedFrom),
        completed_(std::exchange(other.completed_, true)) {
    Base::MoveFrom(other);
  }

  ValueFuture& operator=(ValueFuture&& other) noexcept {
    completed_ = std::exchange(other.completed_, true);
    Base::MoveFrom(other);
    return *this;
  }

  static ValueFuture Resolved() { return ValueFuture(std::in_place); }

 private:
  friend Base;
  friend class ValueProvider<void>;
  friend class BroadcastValueProvider<void>;

  static constexpr const char kWaitReason[] = "ValueFuture";

  explicit ValueFuture(ListFutureProvider<ValueFuture<void>>& provider)
      : Base(provider) {}
  explicit ValueFuture(SingleFutureProvider<ValueFuture<void>>& provider)
      : Base(provider) {}

  explicit ValueFuture(std::in_place_t)
      : Base(Base::kReadyForCompletion), completed_(true) {}

  void Resolve() {
    {
      std::lock_guard guard(Base::lock());
      PW_ASSERT(!completed_);
      completed_ = true;
      this->unlist();
    }
    Base::Wake();
  }

  Poll<> DoPend(Context&) {
    std::lock_guard guard(Base::lock());
    if (completed_) {
      return Ready();
    }
    return Pending();
  }

  bool completed_ = false;
};

/// A `ValueFuture` that does not return any value, just a completion signal.
using VoidFuture = ValueFuture<void>;

/// A one-to-many provider for a single value.
///
/// A `BroadcastValueProvider` can vend multiple `ValueFuture` objects. When the
/// provider is resolved, all futures vended by it are completed with the same
/// value.
///
/// This provider is multi-shot: after `Resolve` is called, new futures can
/// be retrieved with `Get` to wait for the next `Resolve` event.
template <typename T>
class BroadcastValueProvider {
 public:
  /// Returns a `ValueFuture` that will be completed when `Resolve` is called.
  ///
  /// Multiple futures can be retrieved and will pend concurrently.
  ValueFuture<T> Get() { return ValueFuture<T>(provider_); }

  /// Resolves every pending `ValueFuture` with a copy of the provided value.
  template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
  void Resolve(const U& value) {
    while (auto future = provider_.Pop()) {
      future->get().Resolve(value);
    }
  }

  /// Resolves every pending `ValueFuture`.
  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  void Resolve() {
    while (auto future = provider_.Pop()) {
      future->get().Resolve();
    }
  }

 private:
  ListFutureProvider<ValueFuture<T>> provider_;
};

/// A one-to-one provider for a single value.
///
/// An `ValueProvider` can only vend one `ValueFuture` at a time.
///
/// This provider is multi-shot: after `Resolve` is called, a new future can
/// be retrieved with `Get` to wait for the next `Resolve` event.
template <typename T>
class ValueProvider {
 public:
  /// Returns a `ValueFuture` that will be completed when `Resolve` is called.
  ///
  /// If a future has already been vended and is still pending, this crashes.
  ValueFuture<T> Get() {
    PW_ASSERT(!has_future());
    return ValueFuture<T>(provider_);
  }

  /// Returns a `ValueFuture` that will be completed when `Resolve` is called.
  ///
  /// If a future has already been vended and is still pending, this will
  /// return `std::nullopt`.
  std::optional<ValueFuture<T>> TryGet() {
    if (has_future()) {
      return std::nullopt;
    }
    return ValueFuture<T>(provider_);
  }

  /// Returns `true` if the provider stores a pending future.
  bool has_future() { return provider_.has_future(); }

  /// Resolves the pending `ValueFuture` by constructing its value in-place.
  template <typename... Args,
            typename U = T,
            std::enable_if_t<!std::is_void_v<U>, int> = 0>
  void Resolve(Args&&... args) {
    if (auto future = provider_.Take()) {
      future->get().Resolve(std::forward<Args>(args)...);
    }
  }

  /// Resolves the pending `ValueFuture`.
  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  void Resolve() {
    if (auto future = provider_.Take()) {
      future->get().Resolve();
    }
  }

 private:
  SingleFutureProvider<ValueFuture<T>> provider_;
};

/// @endsubmodule

}  // namespace pw::async2
