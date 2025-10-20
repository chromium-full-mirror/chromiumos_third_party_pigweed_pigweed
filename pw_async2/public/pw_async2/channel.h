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

#include "pw_allocator/allocator.h"
#include "pw_async2/future.h"
#include "pw_containers/dynamic_deque.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace pw::async2::experimental {

template <typename T>
class Receiver;

template <typename T>
class SingleReceiver;

template <typename T>
class ReceiveFuture;

template <typename T>
class Sender;

template <typename T>
class SingleSender;

template <typename T>
class SendFuture;

template <typename T>
std::pair<SingleSender<T>, SingleReceiver<T>> CreateSpscChannel(
    Allocator& alloc, uint16_t capacity);

template <typename T>
std::pair<SingleSender<T>, Receiver<T>> CreateSpmcChannel(Allocator& alloc,
                                                          uint16_t capacity);

template <typename T>
std::pair<Sender<T>, SingleReceiver<T>> CreateMpscChannel(Allocator& alloc,
                                                          uint16_t capacity);

template <typename T>
std::pair<Sender<T>, Receiver<T>> CreateMpmcChannel(Allocator& alloc,
                                                    uint16_t capacity);

namespace internal {

template <typename T>
class AllocatedStorage {
 public:
  AllocatedStorage(Allocator& alloc, uint16_t capacity) : deque_(alloc) {
    if (!deque_.try_reserve_exact(capacity)) {
      status_ = Status::ResourceExhausted();
    }
  }

  Status status() {
    std::lock_guard lock(lock_);
    return status_;
  }

  bool closed() {
    std::lock_guard lock(lock_);
    return closed_locked();
  }

  bool full() { return remaining_capacity() == 0; }

  uint16_t remaining_capacity() {
    std::lock_guard lock(lock_);
    return deque_.capacity() - deque_.size();
  }

  uint16_t capacity() const PW_NO_LOCK_SAFETY_ANALYSIS {
    // SAFETY: The capacity of `deque_` cannot change.
    return deque_.capacity();
  }

  bool empty() {
    std::lock_guard lock(lock_);
    return deque_.empty();
  }

  void Push(const T& value) {
    std::lock_guard lock(lock_);
    PW_ASSERT(!closed_locked());
    PushAndWake(value);
  }

  void Push(T&& value) {
    std::lock_guard lock(lock_);
    PW_ASSERT(!closed_locked());
    PushAndWake(std::forward<T>(value));
  }

  bool TryPush(const T& value) {
    std::lock_guard lock(lock_);
    if (closed_locked() || deque_.size() == deque_.capacity()) {
      return false;
    }
    PushAndWake(value);
    return true;
  }

  bool TryPush(T&& value) {
    std::lock_guard lock(lock_);
    if (closed_locked() || deque_.size() == deque_.capacity()) {
      return false;
    }
    PushAndWake(std::forward<T>(value));
    return true;
  }

  T Pop() {
    std::lock_guard lock(lock_);
    return PopAndWake();
  }

  std::optional<T> TryPop() {
    std::lock_guard lock(lock_);
    if (deque_.empty()) {
      return std::nullopt;
    }
    return PopAndWake();
  }

  void add_receiver() {
    std::lock_guard lock(lock_);
    if (!closed_locked()) {
      receiver_count_++;
    }
  }

  void remove_receiver() {
    std::lock_guard lock(lock_);
    if (closed_locked()) {
      return;
    }

    receiver_count_--;
    if (receiver_count_ == 0) {
      Close();
    }
  }

  void add_sender() {
    std::lock_guard lock(lock_);
    if (!closed_locked()) {
      sender_count_++;
    }
  }

  void remove_sender() {
    std::lock_guard lock(lock_);
    if (closed_locked()) {
      return;
    }

    sender_count_--;
    if (sender_count_ == 0) {
      Close();
    }
  }

 private:
  friend SendFuture<T>;
  friend ReceiveFuture<T>;

  bool closed_locked() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return status_.IsFailedPrecondition();
  }

  void Close() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    status_ = Status::FailedPrecondition();
    while (!send_futures_.empty()) {
      send_futures_.Pop().Wake();
    }
    while (!receive_futures_.empty()) {
      receive_futures_.Pop().Wake();
    }
  }

  void PushAndWake(T&& value) PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    PW_ASSERT(deque_.try_push_back(std::move(value)));
    if (!receive_futures_.empty()) {
      receive_futures_.Pop().Wake();
    }
  }

  T PopAndWake() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    PW_ASSERT(!deque_.empty());

    T value = std::move(deque_.front());
    deque_.pop_front();

    if (!send_futures_.empty()) {
      send_futures_.Pop().Wake();
    }

    return value;
  }

  // ListFutureProvider is internally synchronized.
  ListFutureProvider<SendFuture<T>> send_futures_;
  ListFutureProvider<ReceiveFuture<T>> receive_futures_;

  sync::InterruptSpinLock lock_;
  Status status_ PW_GUARDED_BY(lock_);
  DynamicDeque<T, uint16_t> deque_ PW_GUARDED_BY(lock_);
  uint8_t sender_count_ PW_GUARDED_BY(lock_) = 0;
  uint8_t receiver_count_ PW_GUARDED_BY(lock_) = 0;
};

}  // namespace internal

template <typename T>
class ReceiveFuture
    : public ListableFutureWithWaker<ReceiveFuture<T>, std::optional<T>> {
 public:
  ReceiveFuture(ReceiveFuture&& other)
      : Base(Base::kMovedFrom),
        storage_(std::exchange(other.storage_, nullptr)) {
    Base::MoveFrom(other);
  }

  ReceiveFuture& operator=(ReceiveFuture&& other) {
    storage_ = std::exchange(other.storage_, nullptr);
    Base::MoveFrom(other);
    return *this;
  }

 private:
  using Base = ListableFutureWithWaker<ReceiveFuture<T>, std::optional<T>>;
  friend Base;
  friend internal::AllocatedStorage<T>;
  friend Receiver<T>;

  static constexpr const char kWaitReason[] = "Receiver::Receive";

  ReceiveFuture(SharedPtr<internal::AllocatedStorage<T>> storage)
      : Base(storage->receive_futures_), storage_(storage) {}

  ReceiveFuture() : Base(Base::kReadyForCompletion) {}

  Poll<std::optional<T>> DoPend(Context&) {
    if (storage_ == nullptr) {
      return Ready<std::optional<T>>(std::nullopt);
    }

    if (storage_->empty()) {
      if (storage_->closed()) {
        storage_.reset();
        return Ready<std::optional<T>>(std::nullopt);
      }
      return Pending();
    }

    T value = storage_->Pop();
    storage_.reset();
    return Ready(std::move(value));
  }

  using Base::Wake;

  SharedPtr<internal::AllocatedStorage<T>> storage_;
};

template <typename T>
class Receiver {
 public:
  Receiver(const Receiver& other) = delete;
  Receiver& operator=(const Receiver& other) = delete;

  Receiver(Receiver&& other) noexcept
      : storage_(std::exchange(other.storage_, nullptr)) {}

  Receiver& operator=(Receiver&& other) noexcept {
    if (storage_ != nullptr) {
      storage_->remove_receiver();
    }
    storage_ = std::exchange(other.storage_, nullptr);
    return *this;
  }

  ~Receiver() {
    if (storage_ != nullptr) {
      storage_->remove_receiver();
    }
  }

  /// Creates a copy of the receiver which can be used to listen on the same
  /// channel.
  Receiver clone() const { return Receiver(this->storage_); }

  /// Reads a value from the channel, blocking until it is available.
  ///
  /// Returns a `Future<std::optional<T>>` which resolves to a `T` value if the
  /// read is successful, or `std::nullopt` if the channel is closed.
  ///
  /// If there are multiple receivers for a channel, each of them compete for
  /// exclusive values.
  ReceiveFuture<T> Receive() {
    if (storage_ == nullptr) {
      return ReceiveFuture<T>();
    }
    return ReceiveFuture<T>(storage_);
  }

  /// Removes this receiver from its channel, preventing the receiver from
  /// reading further values.
  ///
  /// The channel may remain open if other receivers exist.
  void Disconnect() {
    if (storage_ != nullptr) {
      storage_->remove_receiver();
      storage_.reset();
    }
  }

 protected:
  explicit Receiver(SharedPtr<internal::AllocatedStorage<T>> storage)
      : storage_(storage) {
    if (storage_ != nullptr) {
      storage_->add_receiver();
    }
  }

 private:
  template <typename U>
  friend std::pair<SingleSender<U>, Receiver<U>> CreateSpmcChannel(Allocator&,
                                                                   uint16_t);
  template <typename U>
  friend std::pair<Sender<U>, Receiver<U>> CreateMpmcChannel(Allocator&,
                                                             uint16_t);

  SharedPtr<internal::AllocatedStorage<T>> storage_;
};

template <typename T>
class SingleReceiver : private Receiver<T> {
 public:
  SingleReceiver(const SingleReceiver&) = delete;
  SingleReceiver& operator=(const SingleReceiver&) = delete;

  SingleReceiver(SingleReceiver&&) = default;
  SingleReceiver& operator=(SingleReceiver&&) = default;

  using Receiver<T>::Receive;
  using Receiver<T>::Disconnect;

 private:
  template <typename U>
  friend std::pair<SingleSender<U>, SingleReceiver<U>> CreateSpscChannel(
      Allocator&, uint16_t);
  template <typename U>
  friend std::pair<Sender<U>, SingleReceiver<U>> CreateMpscChannel(Allocator&,
                                                                   uint16_t);

  explicit SingleReceiver(SharedPtr<internal::AllocatedStorage<T>> storage)
      : Receiver<T>(std::move(storage)) {}
};

template <typename T>
class [[nodiscard]] SendFuture
    : public ListableFutureWithWaker<SendFuture<T>, bool> {
 public:
  SendFuture(SendFuture&& other)
      : Base(Base::kMovedFrom),
        storage_(std::exchange(other.storage_, nullptr)),
        value_(std::move(other.value_)) {
    Base::MoveFrom(other);
  }
  SendFuture& operator=(SendFuture&& other) {
    storage_ = std::exchange(other.storage_, nullptr);
    value_ = std::move(other.value_);
    Base::MoveFrom(other);
    return *this;
  }

 private:
  using Base = ListableFutureWithWaker<SendFuture<T>, bool>;
  friend Base;
  friend internal::AllocatedStorage<T>;
  friend Sender<T>;

  static constexpr const char kWaitReason[] = "Sender::Send";

  SendFuture(SharedPtr<internal::AllocatedStorage<T>> storage, const T& value)
      : Base(storage->send_futures_), storage_(storage), value_(value) {}

  SendFuture(SharedPtr<internal::AllocatedStorage<T>> storage, T&& value)
      : Base(storage->send_futures_),
        storage_(storage),
        value_(std::move(value)) {}

  enum ClosedState { kClosed };

  SendFuture(ClosedState, const T& value)
      : Base(Base::kReadyForCompletion), value_(value) {}

  SendFuture(ClosedState, T&& value)
      : Base(Base::kReadyForCompletion), value_(std::move(value)) {}

  Poll<bool> DoPend(async2::Context&) {
    if (storage_ == nullptr || storage_->closed()) {
      storage_.reset();
      return Ready(false);
    }

    if (storage_->full()) {
      return Pending();
    }

    storage_->Push(std::move(value_));
    storage_.reset();
    return Ready(true);
  }

  using Base::Wake;

  SharedPtr<internal::AllocatedStorage<T>> storage_;
  T value_;
};

template <typename T>
class Sender {
 public:
  Sender(const Sender& other) = delete;
  Sender& operator=(const Sender& other) = delete;

  Sender(Sender&& other) noexcept
      : storage_(std::exchange(other.storage_, nullptr)) {}

  Sender& operator=(Sender&& other) noexcept {
    if (storage_ != nullptr) {
      storage_->remove_sender();
    }
    storage_ = std::exchange(other.storage_, nullptr);
    return *this;
  }

  ~Sender() {
    if (storage_ != nullptr) {
      storage_->remove_sender();
    }
  }

  /// Creates a copy of the sender which can be used to write to the same
  /// channel.
  Sender clone() const { return Sender(this->storage_); }

  /// Sends `value` through the channel, blocking until there is space.
  ///
  /// Returns a `Future<bool>` which resolves to `true` if the value was
  /// successfully sent to the channel, or `false` if the channel is closed.
  ///
  /// Note that a value being sent successfully does not guarantee that it will
  /// be read. If all corresponding receivers disconnect, any values still
  /// buffered in the channel are lost.
  SendFuture<T> Send(const T& value) {
    if (storage_ == nullptr) {
      return SendFuture<T>(SendFuture<T>::kClosed, value);
    }
    return SendFuture<T>(storage_, value);
  }

  /// Sends `value` through the channel, blocking until there is space.
  ///
  /// Returns a `Future<bool>` which resolves to `true` if the value was
  /// successfully sent to the channel, or `false` if the channel is closed.
  ///
  /// Note that a value being sent successfully does not guarantee that it will
  /// be read. If all corresponding receivers disconnect, any values still
  /// buffered in the channel are lost.
  SendFuture<T> Send(T&& value) {
    if (storage_ == nullptr) {
      return SendFuture<T>(SendFuture<T>::kClosed, std::move(value));
    }
    return SendFuture<T>(storage_, std::move(value));
  }

  /// Synchronously attempts to send `value` if there is space in the channel.
  /// Returns `true` if successful.
  /// This operation is thread-safe and may be called from outside of an async
  /// context.
  bool TrySend(const T& value) {
    if (storage_ == nullptr) {
      return false;
    }
    return storage_->TryPush(value);
  }

  /// Synchronously attempts to send `value` if there is space in the channel.
  /// Returns `true` if successful.
  /// This operation is thread-safe and may be called from outside of an async
  /// context.
  bool TrySend(T&& value) {
    if (storage_ == nullptr) {
      return false;
    }
    return storage_->TryPush(std::move(value));
  }

  /// Removes this sender from its channel, preventing it from writing further
  /// values.
  ///
  /// The channel may remain open if other senders and receivers exist.
  void Disconnect() {
    if (storage_ != nullptr) {
      storage_->remove_sender();
      storage_.reset();
    }
  }

  /// Returns the current capacity of the channel.
  uint16_t remaining_capacity() const {
    return storage_ != nullptr ? storage_->remaining_capacity() : 0;
  }

  /// Returns the maximum capacity of the channel.
  uint16_t capacity() const {
    return storage_ != nullptr ? storage_->capacity() : 0;
  }

 protected:
  explicit Sender(SharedPtr<internal::AllocatedStorage<T>> storage)
      : storage_(storage) {
    if (storage_ != nullptr) {
      storage_->add_sender();
    }
  }

 private:
  template <typename U>
  friend std::pair<Sender<U>, SingleReceiver<U>> CreateMpscChannel(Allocator&,
                                                                   uint16_t);
  template <typename U>
  friend std::pair<Sender<U>, Receiver<U>> CreateMpmcChannel(Allocator&,
                                                             uint16_t);

  SharedPtr<internal::AllocatedStorage<T>> storage_;
};

template <typename T>
class SingleSender : private Sender<T> {
 public:
  SingleSender(const SingleSender&) = delete;
  SingleSender& operator=(const SingleSender&) = delete;

  SingleSender(SingleSender&&) = default;
  SingleSender& operator=(SingleSender&&) = default;

  using Sender<T>::Send;
  using Sender<T>::TrySend;
  using Sender<T>::Disconnect;

 private:
  template <typename U>
  friend std::pair<SingleSender<U>, SingleReceiver<U>> CreateSpscChannel(
      Allocator&, uint16_t);
  template <typename U>
  friend std::pair<SingleSender<U>, Receiver<U>> CreateSpmcChannel(Allocator&,
                                                                   uint16_t);

  explicit SingleSender(SharedPtr<internal::AllocatedStorage<T>> storage)
      : Sender<T>(std::move(storage)) {}
};

/// Creates a dynamically allocated single-producer, single-consumer channel
/// with a fixed storage capacity.
///
/// Returns a pair of sender and receiver to the channel.
///
/// All allocation occurs during the creation of the channel. After this
/// function returns, usage of the channel is guaranteed not to allocate.
///
/// The channel remains open as long as both the sender and receiver are alive.
template <typename T>
std::pair<SingleSender<T>, SingleReceiver<T>> CreateSpscChannel(
    Allocator& alloc, uint16_t capacity) {
  auto storage =
      alloc.MakeShared<internal::AllocatedStorage<T>>(alloc, capacity);
  if (storage != nullptr && !storage->status().ok()) {
    storage.reset();
  }
  return {SingleSender<T>(storage), SingleReceiver<T>(storage)};
}

/// Creates a dynamically allocated single-producer, multi-consumer channel
/// with a fixed storage capacity.
///
/// Returns a pair of sender and receiver to the channel.
///
/// All allocation occurs during the creation of the channel. After this
/// function returns, usage of the channel is guaranteed not to allocate.
///
/// The channel remains open as long as both the sender and at least one
/// receiver are alive.
template <typename T>
std::pair<SingleSender<T>, Receiver<T>> CreateSpmcChannel(Allocator& alloc,
                                                          uint16_t capacity) {
  auto storage =
      alloc.MakeShared<internal::AllocatedStorage<T>>(alloc, capacity);
  if (storage != nullptr && !storage->status().ok()) {
    storage.reset();
  }
  return {SingleSender<T>(storage), Receiver<T>(storage)};
}

/// Creates a dynamically allocated multi-producer, single-consumer channel with
/// a fixed storage capacity.
///
/// Returns a pair of sender and receiver to the channel. The `Sender` can be
/// cloned to create multiple producers.
///
/// All allocation occurs during the creation of the channel. After this
/// function returns, usage of the channel is guaranteed not to allocate.
///
/// The channel remains open as long as the receiver and at least one sender are
/// alive.
template <typename T>
std::pair<Sender<T>, SingleReceiver<T>> CreateMpscChannel(Allocator& alloc,
                                                          uint16_t capacity) {
  auto storage =
      alloc.MakeShared<internal::AllocatedStorage<T>>(alloc, capacity);
  if (storage != nullptr && !storage->status().ok()) {
    storage.reset();
  }
  return {Sender<T>(storage), SingleReceiver<T>(storage)};
}

/// Creates a dynamically allocated multi-producer, multi-consumer channel
/// with a fixed storage capacity.
///
/// Returns a pair of sender and receiver to the channel.
///
/// All allocation occurs during the creation of the channel. After this
/// function returns, usage of the channel is guaranteed not to allocate.
///
/// The channel remains open as long as at least one sender and one receiver
/// are alive.
template <typename T>
std::pair<Sender<T>, Receiver<T>> CreateMpmcChannel(Allocator& alloc,
                                                    uint16_t capacity) {
  auto storage =
      alloc.MakeShared<internal::AllocatedStorage<T>>(alloc, capacity);
  if (storage != nullptr && !storage->status().ok()) {
    storage.reset();
  }
  return {Sender<T>(storage), Receiver<T>(storage)};
}

}  // namespace pw::async2::experimental
