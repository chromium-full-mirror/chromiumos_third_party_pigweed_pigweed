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

#include <type_traits>

#include "pw_assert/assert.h"
#include "pw_async2/future.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace pw::async2 {

template <typename T>
class ValueProvider;
template <typename T>
class BroadcastValueProvider;

/// @submodule{pw_async2,futures}

namespace internal {

template <typename FutureType>
class FutureList : public BaseFutureList {
 public:
  bool empty() const PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    return BaseFutureList::empty();
  }

  void Push(FutureCore& future) PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    BaseFutureList::Push(future);
  }

  bool PushIfEmpty(FutureCore& future) PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    return BaseFutureList::PushIfEmpty(future);
  }

  void Remove(FutureCore& future) PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    BaseFutureList::Remove(future);
  }

  void Replace(FutureCore& old_future, FutureCore& new_future)
      PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    BaseFutureList::Replace(old_future, new_future);
  }

  template <typename Func>
  std::invoke_result_t<Func> RunWithLock(Func&& func) PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    return func();
  }

  template <typename... Args>
  void ResolveAll(Args&&... args) {
    std::lock_guard lock(lock_);
    ResolveAllWith([&](FutureCore& base) {
      auto* future = ContainerOf(&base, &FutureType::core_);
      future->ResolveLocked(std::forward<Args>(args)...);
    });
  }

  template <typename... Args>
  void ResolveOne(Args&&... args) {
    std::lock_guard lock(lock_);
    ResolveOneWith([&](FutureCore& base) {
      auto* future = ContainerOf(&base, &FutureType::core_);
      future->ResolveLocked(std::forward<Args>(args)...);
    });
  }

 private:
  mutable sync::InterruptSpinLock lock_;
};

}  // namespace internal

/// A future that holds a single value.
///
/// A `ValueFuture` is a concrete `Future` implementation that is vended by a
/// `ValueProvider` or a `BroadcastValueProvider`. It waits until the provider
/// resolves it with a value.
template <typename T>
class ValueFuture {
 public:
  using value_type = T;

  ValueFuture(ValueFuture&& other) noexcept = default;
  ValueFuture& operator=(ValueFuture&& other) noexcept = default;
  ~ValueFuture() = default;

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

  Poll<T> Pend(Context& cx) { return core_.DoPend<ValueFuture<T>>(*this, cx); }

  [[nodiscard]] bool is_complete() const { return core_.is_complete(); }

  static constexpr char kWaitReason[] = "ValueFuture";

 private:
  friend class FutureCore;
  friend class ValueProvider<T>;
  friend class BroadcastValueProvider<T>;
  friend class internal::FutureList<ValueFuture<T>>;

  using DeferPush = FutureCore::DeferPush;

  template <typename... Args>
  explicit ValueFuture(std::in_place_t, Args&&... args)
      : core_(FutureCore::kReadyForCompletion),
        value_(std::in_place, std::forward<Args>(args)...) {}

  explicit ValueFuture(internal::FutureList<ValueFuture<T>>& list)
      : core_(list) {}

  ValueFuture(internal::FutureList<ValueFuture<T>>& list, DeferPush)
      : core_(list, FutureCore::kDeferPush) {}

  Poll<T> DoPend(Context&) {
    if (core_.list() == nullptr) {
      PW_ASSERT(value_.has_value());
      T value = std::move(value_.value());
      value_.reset();
      return Ready(std::move(value));
    }

    auto* list =
        static_cast<internal::FutureList<ValueFuture<T>>*>(core_.list());
    return list->RunWithLock([&]() -> Poll<T> {
      if (value_.has_value()) {
        T value = std::move(value_.value());
        value_.reset();
        return Ready(std::move(value));
      }
      return Pending();
    });
  }

  template <typename... Args>
  void ResolveLocked(Args&&... args) {
    // SAFETY: This is only called from FutureList with the lock held.
    PW_ASSERT(!value_.has_value());
    value_.emplace(std::forward<Args>(args)...);
    core_.Wake();
  }

  FutureCore core_;
  std::optional<T> value_;
};

/// Specialization for a future that does not return any value, just a
/// completion signal.
template <>
class ValueFuture<void> {
 public:
  using value_type = ReadyType;

  ValueFuture() : ready_(false) {}

  ValueFuture(ValueFuture&& other) noexcept = default;
  ValueFuture& operator=(ValueFuture&& other) noexcept = default;
  ~ValueFuture() = default;

  Poll<> Pend(Context& cx) {
    return core_.DoPend<ValueFuture<void>>(*this, cx);
  }

  [[nodiscard]] bool is_complete() const { return core_.is_complete(); }

  static ValueFuture Resolved() { return ValueFuture(true); }

  static constexpr char kWaitReason[] = "ValueFuture<void>";

 private:
  friend class FutureCore;
  friend class ValueProvider<void>;
  friend class BroadcastValueProvider<void>;
  friend class internal::FutureList<ValueFuture<void>>;

  using DeferPush = FutureCore::DeferPush;

  explicit ValueFuture(bool ready)
      : core_(FutureCore::kReadyForCompletion), ready_(ready) {}

  explicit ValueFuture(internal::FutureList<ValueFuture<void>>& list)
      : core_(list) {}

  ValueFuture(internal::FutureList<ValueFuture<void>>& list, DeferPush)
      : core_(list, FutureCore::kDeferPush) {}

  Poll<> DoPend(Context&) {
    if (core_.list() == nullptr) {
      return ready_ ? Ready() : Pending();
    }

    auto* list =
        static_cast<internal::FutureList<ValueFuture<void>>*>(core_.list());
    return list->RunWithLock(
        [&]() -> Poll<> { return ready_ ? Ready() : Pending(); });
  }

  void ResolveLocked() {
    // SAFETY: This is only called from FutureList with the lock held.
    PW_ASSERT(!ready_);
    ready_ = true;
    core_.Wake();
  }

  FutureCore core_;
  bool ready_ = false;
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
  ValueFuture<T> Get() { return ValueFuture<T>(list_); }

  /// Resolves every pending `ValueFuture` with a copy of the provided value.
  template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
  void Resolve(const U& value) {
    list_.ResolveAll(value);
  }

  /// Resolves every pending `ValueFuture`.
  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  void Resolve() {
    list_.ResolveAll();
  }

 private:
  internal::FutureList<ValueFuture<T>> list_;
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
    return ValueFuture<T>(list_);
  }

  /// Returns a `ValueFuture` that will be completed when `Resolve` is called.
  ///
  /// If a future has already been vended and is still pending, this will
  /// return `std::nullopt`.
  std::optional<ValueFuture<T>> TryGet() {
    ValueFuture<T> future(list_, ValueFuture<T>::DeferPush::kDeferPush);
    if (!list_.PushIfEmpty(future.core_)) {
      return std::nullopt;
    }
    return future;
  }

  /// Returns `true` if the provider stores a pending future.
  bool has_future() const { return !list_.empty(); }

  /// Resolves the pending `ValueFuture` by constructing its value in-place.
  template <typename... Args,
            typename U = T,
            std::enable_if_t<!std::is_void_v<U>, int> = 0>
  void Resolve(Args&&... args) {
    list_.ResolveOne(std::forward<Args>(args)...);
  }

  /// Resolves the pending `ValueFuture`.
  template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
  void Resolve() {
    list_.ResolveOne();
  }

 private:
  internal::FutureList<ValueFuture<T>> list_;
};

/// @endsubmodule

}  // namespace pw::async2
