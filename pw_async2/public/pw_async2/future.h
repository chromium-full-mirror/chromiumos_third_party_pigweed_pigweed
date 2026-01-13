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

#include <mutex>
#include <optional>
#include <type_traits>

#include "pw_assert/assert.h"
#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_containers/intrusive_list.h"
#include "pw_memory/container_of.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace pw::async2 {

/// @submodule{pw_async2,futures}

/// A `Future` is an abstract handle to an asynchronous operation that is polled
/// to completion. On completion, futures may return a value representing the
/// result of the operation.
///
/// Futures are single-use and track their completion status. It is an error
/// to poll a future after it has already completed.
///
/// # Implementing
///
/// The future class does not contain any members itself, providing only a core
/// interface and delegating management to specific implementations.
///
/// In practice, developers will rarely derive from `Future` directly. Instead,
/// they should use a more specific abstract future type like
/// `ListableFutureWithWaker`, which manages common behaviors like waker
/// storage.
///
/// Deriving from `Future` directly is necessary when these behaviors are not
/// required; for example, when implementing a future that composes other
/// futures.
///
/// Implementations derived directly from `Future` are required to provide the
/// following member functions:
///
/// - `Poll<T> DoPend(Context& cx)`: Implements the asynchronous operation.
/// - `void DoMarkComplete()`: Marks the future as complete.
/// - `bool DoIsComplete() const`: Returns `true` if `DoMarkCompleted` has
///    previously been called. This must always return a value, even if the
///    future's provider has been destroyed.
///
/// @tparam Derived The concrete class that implements this `Future`.
/// @tparam T       The type of the value returned by `Pend` upon completion.
///                 Use `void` for futures that do not return a value.
template <typename Derived, typename T>
class Future {
 public:
  using value_type = std::conditional_t<std::is_void_v<T>, ReadyType, T>;

  /// Polls the future to advance its state.
  ///
  /// Returns `Pending` if the future is not yet complete, or `Ready` with
  /// its result if it is.
  ///
  /// If this future has already completed, calling `Pend` will trigger an
  /// assertion.
  Poll<value_type> Pend(Context& cx) {
    PW_ASSERT(!is_complete());
    Poll<value_type> poll = derived().DoPend(cx);
    if (poll.IsReady()) {
      derived().DoMarkComplete();
    }
    return poll;
  }

  /// Returns `true` if the future has already returned a `Ready` result.
  ///
  /// Calling `Pend` on a completed future will trigger an assertion.
  bool is_complete() const { return derived().DoIsComplete(); }

 protected:
  constexpr Future() = default;

 private:
  Derived& derived() { return static_cast<Derived&>(*this); }
  const Derived& derived() const { return static_cast<const Derived&>(*this); }
};

template <typename T, typename = void>
struct is_future : std::false_type {};

template <typename T>
struct is_future<
    T,
    std::void_t<typename T::value_type,
                decltype(std::declval<T&>().Pend(std::declval<Context&>())),
                decltype(std::declval<const T&>().is_complete())>>
    : std::is_convertible<decltype(&T::Pend),
                          Poll<typename T::value_type> (T::*)(Context&)> {};

template <typename T>
constexpr bool is_future_v =
    is_future<std::remove_cv_t<std::remove_reference_t<T>>>::value;

/// `FutureCore` provides common functionality for futures that need to be
/// wakeable and stored in a list.
///
/// This class provides the core mechanism for:
/// - Storing a `Waker` to be woken up.
/// - Tracking future state.
/// - Being part of an `IntrusiveList`.
///
/// It is designed to be used as a member of a concrete future class
/// (composition) rather than a base class, to simplify move semantics and
/// object lifetime.
class FutureCore : public IntrusiveForwardList<FutureCore>::Item {
 public:
  constexpr FutureCore() : state_(State::kNull) {}

  FutureCore(FutureCore&& other) noexcept;

  FutureCore& operator=(FutureCore&& other) noexcept;

  /// Tag type to construct a `FutureCore` for which the next call to `Pend`
  /// will return `Ready`.
  enum ReadyForCompletion { kReadyForCompletion };

  /// Tag type to construct an active `FutureCore`. `Pend` may be called.
  enum Pending { kPending };

  /// Constructs a pending `FutureCore` that represents an async operation.
  /// `Pend` must be called until it returns `Ready`.
  ///
  /// A pending `FutureCore` must be tracked by its provider (e.g. in a
  /// `FutureList`).
  explicit constexpr FutureCore(Pending) : state_(State::kPending) {}

  /// Creates a wakeable future that is ready for completion. The next call to
  /// `Pend` must return `Ready`.
  explicit constexpr FutureCore(ReadyForCompletion) : state_(State::kReady) {}

  FutureCore(const FutureCore&) = delete;
  FutureCore& operator=(const FutureCore&) = delete;

  ~FutureCore() = default;

  /// @returns Whether the future's `Pend()` function can be called.
  [[nodiscard]] bool is_pendable() const { return state_ > State::kComplete; }

  /// @returns Whether the future has completed: the future's `Pend()` returned
  /// `Ready`.
  [[nodiscard]] bool is_complete() const { return state_ == State::kComplete; }

  /// @returns `true` if the next `Pend()` call is guaranteed to return `Ready`.
  /// Depending on the implementation, `Pend()` may return `Ready` while
  /// `is_ready` is `false`. Future implementations call `WakeAndMarkReady` to
  /// set `is_ready` to `true`.
  [[nodiscard]] bool is_ready() const { return state_ == State::kReady; }

  /// Wakes the task waiting on the future, if any. The future must be pended in
  /// order to make progress.
  void Wake() { waker_.Wake(); }

  /// Wakes the task pending the future and sets `is_ready` to `true`. Only call
  /// this if the next call to to the future's `Pend()` will return `Ready`.
  void WakeAndMarkReady() {
    Wake();
    state_ = State::kReady;
  }

  /// Provides direct access to the waker for future implementations that
  /// manually store a waker.
  ///
  /// @warning Do not use this function when `FutureCore::DoPend` is used.
  /// `FutureCore::DoPend` stores the waker when `Pend()` returns `Pending`.
  Waker& waker() { return waker_; }

  /// Mark the future as complete, which indicates that a future has returned
  /// `Ready` from its `Pend` function.
  ///
  /// @warning Do not use this function when `FutureCore::DoPend` is used.
  /// `FutureCore::DoPend` calls `MarkComplete` when `Pend()` returns `Ready`.
  void MarkComplete() { state_ = State::kComplete; }

  /// Removes this future from its list, if it is in one.
  void Unlist() { unlist(); }

  /// Optional `Pend()` function that defers to the future implementation's
  /// `DoPend(Context&)` function. `FutureCore::DoPend` does the following:
  ///
  /// - Asserts that the future is pendable.
  /// - If the future's `DoPend` returns `Pending`, stores a waker.
  /// - If the future's `DoPend` returns `Ready`, marks the future as complete.
  ///
  /// It is recommended for `Pend` to use `FutureCore::DoPend`, but not
  /// required. Custom `Pend` implementations must enforce the same semantics.
  template <typename FutureType>
  auto DoPend(FutureType& future, Context& cx) PW_NO_LOCK_SAFETY_ANALYSIS {
    PW_ASSERT(is_pendable());

    auto poll = future.DoPend(cx);
    if (poll.IsPending()) {
      PW_ASYNC_STORE_WAKER(cx, waker_, FutureType::kWaitReason);
    } else {
      MarkComplete();
    }

    return poll;
  }

  /// @returns whether the future is currently in a list.
  [[nodiscard]] bool in_list() const { return !unlisted(); }

 private:
  friend class BaseFutureList;  // for IntrusiveForwardList::Item functions

  /// @returns `true` if the `FutureCore` was NOT default constructed.
  [[nodiscard]] bool is_initialized() const { return state_ != State::kNull; }

  Waker waker_;

  enum class State : unsigned char {
    /// The FutureCore is in a default constructed state. It does not yet
    /// represent an async operation and `Pend()` cannot be called.
    kNull,

    /// A previous call to `Pend()` returned `Ready()`.
    kComplete,

    /// The next call to `Pend()` will return `Ready()`.
    kReady,

    /// `Pend()` may be called to advance the operation represented by this
    /// future. `Pend()` may return `Ready()` or `Pending()`.
    kPending,
  } state_;
};

/// A list of `FutureCore`s with common future-related operations. Future
/// providers may use `BaseFutureList` or `FutureList` in place of a plain
/// `IntrusiveForwardList`.
///
/// This class does not provide any locking. It is the responsibility of the
/// user to ensure safety, typically by holding a lock in the containing
/// provider.
class BaseFutureList {
 public:
  constexpr BaseFutureList() = default;

  bool empty() const { return list_.empty(); }

  void Push(FutureCore& future) { containers::PushBackSlow(list_, future); }

  void PushRequireEmpty(FutureCore& future);

  bool PushIfEmpty(FutureCore& future);

  FutureCore* PopIfAvailable() { return list_.empty() ? nullptr : &Pop(); }

  FutureCore& Pop() {
    FutureCore& future = list_.front();
    list_.pop_front();
    return future;
  }

  /// Pops the next future fromt the list and calls `WakeAndMarkReady()`.
  /// Crashes if there are no futures in the list.
  void ResolveOne() {
    list_.front().WakeAndMarkReady();
    list_.pop_front();
  }

  void ResolveOneIfAvailable() {
    if (!list_.empty()) {
      ResolveOne();
    }
  }

  /// Pops all futures and calls `WakeAndMarkReady()` on them.
  void ResolveAll();

 protected:
  IntrusiveForwardList<FutureCore>& list() { return list_; }

 private:
  IntrusiveForwardList<FutureCore> list_;
};

/// List of futures of a custom future type. This is a minimal extension to
/// `BaseFutureList`.
///
/// @tparam kGetFutureImpl a function that converts a `FutureCore&` to its
///     corresponding future type
/// @tparam kGetFutureCore a function that converts a future reference to its
///     corresponding `FutureCore`.
template <auto kGetFutureImpl, auto kGetFutureCore>
class CustomFutureList : public BaseFutureList {
 public:
  using value_type = std::remove_reference_t<decltype(*kGetFutureImpl(
      std::declval<FutureCore*>()))>;
  using pointer = value_type*;
  using reference = value_type&;

  constexpr CustomFutureList() = default;

  void Push(FutureCore& future) { BaseFutureList::Push(future); }
  void Push(reference future) { Push(kGetFutureCore(future)); }

  void PushRequireEmpty(FutureCore& future) {
    BaseFutureList::PushRequireEmpty(future);
  }
  void PushRequireEmpty(reference future) {
    PushRequireEmpty(kGetFutureCore(future));
  }

  bool PushIfEmpty(FutureCore& future) {
    return BaseFutureList::PushIfEmpty(future);
  }
  bool PushIfEmpty(reference future) {
    return PushIfEmpty(kGetFutureCore(future));
  }

  pointer PopIfAvailable() {
    return kGetFutureImpl(BaseFutureList::PopIfAvailable());
  }

  reference Pop() { return *kGetFutureImpl(BaseFutureList::PopIfAvailable()); }

  template <typename Resolver>
  void ResolveAllWith(Resolver&& resolver) {
    while (!list().empty()) {
      resolver(Pop());
    }
  }

  template <typename Resolver>
  void ResolveOneWith(Resolver&& resolver) {
    if (!list().empty()) {
      resolver(Pop());
    }
  }
};

/// A `CustomFutureList` that uses a pointer to a `FutureCore` member.
///
/// @tparam kMemberPtr pointer to a `FutureCore` member of a custom future
///     class
template <auto kMemberPtr>
using FutureList =
    CustomFutureList<ContainerOf<kMemberPtr, FutureCore>, MemberOf<kMemberPtr>>;

/// @endsubmodule

}  // namespace pw::async2
