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
#include "pw_containers/deque.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace pw::async2::experimental {

template <typename T>
class Receiver;

template <typename T>
class ReceiveFuture;

template <typename T>
class Sender;

template <typename T>
class SingleSender;

template <typename T>
class SendFuture;

template <typename T>
class ReserveSendFuture;

template <typename T>
class SendReservation;

template <typename T>
class DynamicChannel;

template <typename T>
std::optional<DynamicChannel<T>> CreateDynamicChannel(Allocator& alloc,
                                                      uint16_t capacity);

template <typename T>
class Channel {
 public:
  ~Channel() { PW_ASSERT(ref_count_ == 0); }

  /// Returns true if the channel is closed. A closed channel cannot create new
  /// senders or receivers, and cannot be re-opened.
  [[nodiscard]] bool closed() const {
    std::lock_guard lock(lock_);
    return closed_;
  }

  /// Creates a sender for this channel.
  Sender<T> CreateSender() {
    PW_ASSERT(!closed());
    return Sender<T>(this);
  }

  /// Creates a receiver for this channel.
  Receiver<T> CreateReceiver() {
    PW_ASSERT(!closed());
    return Receiver<T>(this);
  }

 protected:
  explicit Channel(FixedDeque<T>&& deque) : deque_(std::move(deque)) {}

  template <size_t kAlignment, size_t kCapacity>
  explicit Channel(containers::Storage<kAlignment, kCapacity>& storage)
      : deque_(storage) {}

 private:
  friend Allocator;
  friend DynamicChannel<T>;
  friend SendFuture<T>;
  friend ReserveSendFuture<T>;
  friend ReceiveFuture<T>;
  friend Sender<T>;
  friend Receiver<T>;
  friend SendReservation<T>;

  template <typename U>
  friend std::optional<DynamicChannel<U>> CreateDynamicChannel(Allocator&,
                                                               uint16_t);

  static Channel* Allocated(Allocator& alloc, uint16_t capacity) {
    FixedDeque<T> deque = FixedDeque<T>::TryAllocate(alloc, capacity);
    if (deque.capacity() == 0) {
      return nullptr;
    }
    return alloc.New<Channel<T>>(std::move(deque));
  }

  void Destroy() {
    Deallocator* deallocator = nullptr;
    {
      std::lock_guard lock(lock_);
      deallocator = deque_.deallocator();
    }

    if (deallocator != nullptr) {
      std::destroy_at(this);
      deallocator->Deallocate(this);
    }
  }

  void Close() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    closed_ = true;
    while (!send_futures_.empty()) {
      send_futures_.Pop().Wake();
    }
    while (!reserve_send_futures_.empty()) {
      reserve_send_futures_.Pop().Wake();
    }
    while (!receive_futures_.empty()) {
      receive_futures_.Pop().Wake();
    }
  }

  void PushAndWake(T&& value) PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    deque_.push_back(std::move(value));
    if (!receive_futures_.empty()) {
      receive_futures_.Pop().Wake();
    }
  }

  void PushAndWake(const T& value) PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    deque_.push_back(value);
    if (!receive_futures_.empty()) {
      receive_futures_.Pop().Wake();
    }
  }

  template <typename... Args>
  void EmplaceAndWake(Args&&... args) PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    deque_.emplace_back(std::forward<Args>(args)...);
    if (!receive_futures_.empty()) {
      receive_futures_.Pop().Wake();
    }
  }

  T PopAndWake() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    PW_ASSERT(!deque_.empty());

    T value = std::move(deque_.front());
    deque_.pop_front();

    WakeOneSender();
    return value;
  }

  void WakeOneSender() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    // TODO: b/456507134 - Store both future types in the same list.
    if (prioritize_reserve_) {
      if (!reserve_send_futures_.empty()) {
        reserve_send_futures_.Pop().Wake();
      } else if (!send_futures_.empty()) {
        send_futures_.Pop().Wake();
      }
    } else {
      if (!send_futures_.empty()) {
        send_futures_.Pop().Wake();
      } else if (!reserve_send_futures_.empty()) {
        reserve_send_futures_.Pop().Wake();
      }
    }

    prioritize_reserve_ = !prioritize_reserve_;
  }

  bool full() { return remaining_capacity() == 0; }
  bool full_locked() const PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return remaining_capacity_locked() == 0;
  }

  uint16_t remaining_capacity() {
    std::lock_guard lock(lock_);
    return remaining_capacity_locked();
  }
  uint16_t remaining_capacity_locked() const
      PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    return deque_.capacity() - deque_.size() - reservations_;
  }

  uint16_t capacity() const PW_NO_LOCK_SAFETY_ANALYSIS {
    // SAFETY: The capacity of `deque_` cannot change.
    return deque_.capacity();
  }

  bool empty() {
    std::lock_guard lock(lock_);
    return deque_.empty();
  }

  void Push(const T& value) PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    PW_ASSERT(!closed_);
    PushAndWake(value);
  }

  void Push(T&& value) PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    PW_ASSERT(!closed_);
    PushAndWake(std::move(value));
  }

  bool TryPush(const T& value) {
    std::lock_guard lock(lock_);
    if (closed_ || remaining_capacity_locked() == 0) {
      return false;
    }
    PushAndWake(value);
    return true;
  }

  bool TryPush(T&& value) {
    std::lock_guard lock(lock_);
    if (closed_ || remaining_capacity_locked() == 0) {
      return false;
    }
    PushAndWake(std::move(value));
    return true;
  }

  std::optional<T> TryPop() {
    std::lock_guard lock(lock_);
    if (deque_.empty()) {
      return std::nullopt;
    }
    return PopAndWake();
  }

  bool Reserve() {
    std::lock_guard lock(lock_);
    if (closed_ || remaining_capacity_locked() == 0) {
      return false;
    }
    reservations_++;
    return true;
  }

  void DropReservation() {
    std::lock_guard lock(lock_);
    PW_ASSERT(!closed_ && reservations_ > 0);
    reservations_--;
    WakeOneSender();
  }

  template <typename... Args>
  void CommitReservation(Args&&... args) {
    std::lock_guard lock(lock_);
    PW_ASSERT(!closed_ && reservations_ > 0);
    reservations_--;
    EmplaceAndWake(std::forward<Args>(args)...);
  }

  void add_receiver() {
    std::lock_guard lock(lock_);
    if (!closed_) {
      receiver_count_++;
    }
    ref_count_++;
  }

  void remove_receiver() {
    bool destroy;

    {
      std::lock_guard lock(lock_);
      if (!closed_) {
        receiver_count_--;
        if (receiver_count_ == 0) {
          Close();
        }
      }
      destroy = decrement_ref_locked();
    }

    if (destroy) {
      Destroy();
    }
  }

  void add_sender() {
    std::lock_guard lock(lock_);
    if (!closed_) {
      sender_count_++;
    }
    ref_count_++;
  }

  void remove_sender() {
    bool destroy;

    {
      std::lock_guard lock(lock_);
      if (!closed_) {
        sender_count_--;
        if (sender_count_ == 0) {
          Close();
        }
      }
      destroy = decrement_ref_locked();
    }

    if (destroy) {
      Destroy();
    }
  }

  void add_ref() {
    std::lock_guard lock(lock_);
    ref_count_++;
  }

  void remove_ref() {
    bool destroy;
    {
      std::lock_guard lock(lock_);
      destroy = decrement_ref_locked();
    }
    if (destroy) {
      Destroy();
    }
  }

  bool decrement_ref_locked() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    ref_count_--;
    return ref_count_ == 0;
  }

  // ListFutureProvider is internally synchronized.
  ListFutureProvider<SendFuture<T>> send_futures_;
  ListFutureProvider<ReserveSendFuture<T>> reserve_send_futures_;
  ListFutureProvider<ReceiveFuture<T>> receive_futures_;

  mutable sync::InterruptSpinLock lock_;
  FixedDeque<T> deque_ PW_GUARDED_BY(lock_);
  uint16_t reservations_ PW_GUARDED_BY(lock_) = 0;
  bool closed_ PW_GUARDED_BY(lock_) = false;
  bool prioritize_reserve_ PW_GUARDED_BY(lock_) = true;

  // Channels are reference counted in two ways:
  //
  // - Senders and receivers are tracked independently. Once either reaches
  //   zero, the channel is closed, but not destroyed. No new values can be
  //   sent, but any buffered values can still be read.
  //
  // - Overall object reference count, including senders, receivers, futures,
  //   and channel handles. Once this reaches zero, the channel is destroyed.
  //
  uint8_t sender_count_ PW_GUARDED_BY(lock_) = 0;
  uint8_t receiver_count_ PW_GUARDED_BY(lock_) = 0;
  uint16_t ref_count_ PW_GUARDED_BY(lock_) = 0;
};

/// An asynchronous channel which supports multiple producers and multiple
/// consumers with a fixed storage capacity.
///
/// Senders and receivers to the channel are created from the `StaticChannel`,
/// and the channel remains open as long as at least one sender and one receiver
/// are alive. Once the channel is closed, no more senders or receivers may be
/// created from it.
///
/// `StaticChannel` owns its storage, and must outlive all senders and
/// receivers created from it.
template <typename T, uint16_t kCapacity>
class StaticChannel : public Channel<T> {
 public:
  StaticChannel() : Channel<T>(storage_) {}

 private:
  containers::StorageFor<T, kCapacity> storage_;
};

/// A handle to a dynamically allocated channel.
///
/// This handle is used to create senders and receivers for the channel.
///
/// After all desired senders and receivers are created, this handle may be
/// dropped. The channel will remain allocated and open as long as at least
/// one sender and one receiver are alive.
template <typename T>
class DynamicChannel {
 public:
  DynamicChannel() : channel_(nullptr) {}

  DynamicChannel(const DynamicChannel& other) : channel_(other.channel_) {
    if (channel_ != nullptr) {
      channel_->add_ref();
    }
  }

  DynamicChannel& operator=(const DynamicChannel& other) {
    if (channel_ != nullptr) {
      channel_->remove_ref();
    }
    channel_ = other.channel_;
    if (channel_ != nullptr) {
      channel_->add_ref();
    }
    return *this;
  }

  DynamicChannel(DynamicChannel&& other) noexcept
      : channel_(std::exchange(other.channel_, nullptr)) {}

  DynamicChannel& operator=(DynamicChannel&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    if (channel_ != nullptr) {
      channel_->remove_ref();
    }
    channel_ = std::exchange(other.channel_, nullptr);
    return *this;
  }

  ~DynamicChannel() {
    if (channel_ != nullptr) {
      channel_->remove_ref();
    }
  }

  bool closed() const { return channel_ == nullptr || channel_->closed(); }

  /// Creates a new sender for the channel, increasing the active sender count.
  Sender<T> CreateSender() {
    PW_ASSERT(channel_ != nullptr);
    return channel_->CreateSender();
  }

  /// Creates a new receiver for the channel, increasing the active receiver
  /// count.
  Receiver<T> CreateReceiver() {
    PW_ASSERT(channel_ != nullptr);
    return channel_->CreateReceiver();
  }

 private:
  template <typename U>
  friend std::optional<DynamicChannel<U>> CreateDynamicChannel(Allocator&,
                                                               uint16_t);

  explicit DynamicChannel(Channel<T>* channel) : channel_(channel) {
    if (channel_ != nullptr) {
      channel_->add_ref();
    }
  }

  Channel<T>* channel_;
};

template <typename T>
class [[nodiscard]] ReceiveFuture
    : public ListableFutureWithWaker<ReceiveFuture<T>, std::optional<T>> {
 public:
  ReceiveFuture(ReceiveFuture&& other)
      : Base(Base::kMovedFrom),
        channel_(std::exchange(other.channel_, nullptr)) {
    Base::MoveFrom(other);
  }

  ReceiveFuture& operator=(ReceiveFuture&& other) {
    if (this == &other) {
      return *this;
    }
    if (channel_ != nullptr) {
      channel_->remove_ref();
    }
    channel_ = std::exchange(other.channel_, nullptr);
    Base::MoveFrom(other);
    return *this;
  }

  ~ReceiveFuture() { reset(); }

 private:
  using Base = ListableFutureWithWaker<ReceiveFuture<T>, std::optional<T>>;
  friend Base;
  friend Channel<T>;
  friend Receiver<T>;

  static constexpr const char kWaitReason[] = "Receiver::Receive";

  explicit ReceiveFuture(Channel<T>& channel)
      : Base(channel.receive_futures_), channel_(&channel) {
    channel_->add_ref();
  }

  ReceiveFuture() : Base(Base::kReadyForCompletion), channel_(nullptr) {}

  Poll<std::optional<T>> DoPend(Context&) {
    if (channel_ == nullptr) {
      return Ready<std::optional<T>>(std::nullopt);
    }

    std::optional<T> value = channel_->TryPop();
    if (!value.has_value()) {
      if (channel_->closed()) {
        reset();
        return Ready<std::optional<T>>(std::nullopt);
      }
      return Pending();
    }

    reset();
    return Ready(std::move(value));
  }

  void reset() {
    if (channel_ != nullptr) {
      channel_->remove_ref();
      channel_ = nullptr;
    }
  }

  using Base::Wake;

  Channel<T>* channel_;
};

/// A receiver which reads values from an asynchronous channel.
template <typename T>
class Receiver {
 public:
  Receiver(const Receiver& other) = delete;
  Receiver& operator=(const Receiver& other) = delete;

  Receiver(Receiver&& other) noexcept
      : channel_(std::exchange(other.channel_, nullptr)) {}

  Receiver& operator=(Receiver&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    if (channel_ != nullptr) {
      channel_->remove_receiver();
    }
    channel_ = std::exchange(other.channel_, nullptr);
    return *this;
  }

  ~Receiver() {
    if (channel_ != nullptr) {
      channel_->remove_receiver();
    }
  }

  /// Reads a value from the channel, blocking until it is available.
  ///
  /// Returns a `Future<std::optional<T>>` which resolves to a `T` value if the
  /// read is successful, or `std::nullopt` if the channel is closed.
  ///
  /// If there are multiple receivers for a channel, each of them compete for
  /// exclusive values.
  ReceiveFuture<T> Receive() {
    if (channel_ == nullptr) {
      return ReceiveFuture<T>();
    }
    return ReceiveFuture<T>(*channel_);
  }

  /// Removes this receiver from its channel, preventing the receiver from
  /// reading further values.
  ///
  /// The channel may remain open if other receivers exist.
  void Disconnect() {
    if (channel_ != nullptr) {
      channel_->remove_receiver();
      channel_ = nullptr;
    }
  }

 private:
  template <typename U>
  friend class Channel;

  explicit Receiver(Channel<T>* channel) : channel_(channel) {
    if (channel_ != nullptr) {
      channel_->add_receiver();
    }
  }

  Channel<T>* channel_;
};

template <typename T>
class [[nodiscard]] SendFuture
    : public ListableFutureWithWaker<SendFuture<T>, bool> {
 public:
  SendFuture(SendFuture&& other)
      : Base(Base::kMovedFrom),
        channel_(std::exchange(other.channel_, nullptr)),
        value_(std::move(other.value_)) {
    Base::MoveFrom(other);
  }

  SendFuture& operator=(SendFuture&& other) {
    if (this == &other) {
      return *this;
    }
    if (channel_ != nullptr) {
      channel_->remove_ref();
    }
    channel_ = std::exchange(other.channel_, nullptr);
    value_ = std::move(other.value_);
    Base::MoveFrom(other);
    return *this;
  }

  ~SendFuture() { reset(); }

 private:
  using Base = ListableFutureWithWaker<SendFuture<T>, bool>;
  friend Base;
  friend Channel<T>;
  friend Sender<T>;

  static constexpr const char kWaitReason[] = "Sender::Send";

  SendFuture(Channel<T>& channel, const T& value)
      : Base(channel.send_futures_), channel_(&channel), value_(value) {
    channel_->add_ref();
  }

  SendFuture(Channel<T>& channel, T&& value)
      : Base(channel.send_futures_),
        channel_(&channel),
        value_(std::move(value)) {
    channel_->add_ref();
  }

  enum ClosedState { kClosed };

  SendFuture(ClosedState, const T& value)
      : Base(Base::kReadyForCompletion), channel_(nullptr), value_(value) {}

  SendFuture(ClosedState, T&& value)
      : Base(Base::kReadyForCompletion),
        channel_(nullptr),
        value_(std::move(value)) {}

  Poll<bool> DoPend(async2::Context&) {
    if (channel_ == nullptr || channel_->closed()) {
      reset();
      return Ready(false);
    }

    {
      std::lock_guard lock(channel_->lock_);
      if (channel_->full_locked()) {
        return Pending();
      }

      channel_->Push(std::move(value_));
    }

    reset();
    return Ready(true);
  }

  void reset() {
    if (channel_ != nullptr) {
      channel_->remove_ref();
      channel_ = nullptr;
    }
  }

  using Base::Wake;

  Channel<T>* channel_;
  T value_;
};

/// A reservation for sending values to a channel, returned from a
/// `ReserveSendFuture` once space is available in the channel.
///
/// The `SendReservation` must be used immediately once its future resolves.
/// If the reservation object is dropped, its reservation is released and the
/// space is made available for other senders.
template <typename T>
class SendReservation {
 public:
  SendReservation(const SendReservation& other) = delete;
  SendReservation& operator=(const SendReservation& other) = delete;

  SendReservation(SendReservation&& other)
      : channel_(std::exchange(other.channel_, nullptr)) {}

  SendReservation& operator=(SendReservation&& other) {
    if (this == &other) {
      return *this;
    }
    Cancel();
    channel_ = std::exchange(other.channel_, nullptr);
    return *this;
  }

  ~SendReservation() { Cancel(); }

  /// Commits a value to a reserved slot.
  template <typename... Args>
  void Commit(Args&&... args) {
    PW_ASSERT(channel_ != nullptr);
    channel_->CommitReservation(std::forward<Args>(args)...);
    channel_->remove_ref();
    channel_ = nullptr;
  }

  /// Releases the reservation, making the space available for other senders.
  void Cancel() {
    if (channel_ != nullptr) {
      channel_->DropReservation();
      channel_->remove_ref();
      channel_ = nullptr;
    }
  }

 private:
  friend class ReserveSendFuture<T>;

  explicit SendReservation(Channel<T>& channel) : channel_(&channel) {
    channel_->add_ref();
  }

  Channel<T>* channel_;
};

template <typename T>
class [[nodiscard]] ReserveSendFuture
    : public ListableFutureWithWaker<ReserveSendFuture<T>,
                                     std::optional<SendReservation<T>>> {
 public:
  ReserveSendFuture(ReserveSendFuture&& other)
      : Base(Base::kMovedFrom),
        channel_(std::exchange(other.channel_, nullptr)) {
    Base::MoveFrom(other);
  }

  ReserveSendFuture& operator=(ReserveSendFuture&& other) {
    if (channel_ != nullptr) {
      channel_->remove_ref();
    }
    channel_ = std::exchange(other.channel_, nullptr);
    Base::MoveFrom(other);
    return *this;
  }

  ~ReserveSendFuture() { reset(); }

 private:
  using Base = ListableFutureWithWaker<ReserveSendFuture<T>,
                                       std::optional<SendReservation<T>>>;
  friend Base;
  friend Channel<T>;
  friend Sender<T>;

  static constexpr const char kWaitReason[] = "Sender::ReserveSend";

  explicit ReserveSendFuture(Channel<T>* channel)
      : Base(channel->reserve_send_futures_), channel_(channel) {
    channel_->add_ref();
  }

  enum ClosedState { kClosed };

  explicit ReserveSendFuture(ClosedState)
      : Base(Base::kReadyForCompletion), channel_(nullptr) {}

  Poll<std::optional<SendReservation<T>>> DoPend(async2::Context&) {
    if (channel_ == nullptr || channel_->closed()) {
      reset();
      return Ready<std::optional<SendReservation<T>>>(std::nullopt);
    }

    if (!channel_->Reserve()) {
      return Pending();
    }

    SendReservation<T> reservation(*channel_);
    reset();
    return reservation;
  }

  void reset() {
    if (channel_ != nullptr) {
      channel_->remove_ref();
      channel_ = nullptr;
    }
  }

  using Base::Wake;

  Channel<T>* channel_;
};

/// A sender which writes values to an asynchronous channel.
template <typename T>
class Sender {
 public:
  Sender(const Sender& other) = delete;
  Sender& operator=(const Sender& other) = delete;

  Sender(Sender&& other) noexcept
      : channel_(std::exchange(other.channel_, nullptr)) {}

  Sender& operator=(Sender&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    if (channel_ != nullptr) {
      channel_->remove_sender();
    }
    channel_ = std::exchange(other.channel_, nullptr);
    return *this;
  }

  ~Sender() {
    if (channel_ != nullptr) {
      channel_->remove_sender();
    }
  }

  /// Sends `value` through the channel, blocking until there is space.
  ///
  /// Returns a `Future<bool>` which resolves to `true` if the value was
  /// successfully sent to the channel, or `false` if the channel is closed.
  ///
  /// Note that a value being sent successfully does not guarantee that it will
  /// be read. If all corresponding receivers disconnect, any values still
  /// buffered in the channel are lost.
  SendFuture<T> Send(const T& value) {
    if (channel_ == nullptr) {
      return SendFuture<T>(SendFuture<T>::kClosed, value);
    }
    return SendFuture<T>(*channel_, value);
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
    if (channel_ == nullptr) {
      return SendFuture<T>(SendFuture<T>::kClosed, std::move(value));
    }
    return SendFuture<T>(*channel_, std::move(value));
  }

  /// Returns a `Future<std::optional<SendReservation>>` which resolves to a
  /// `SendReservation` which can be used to write `count` values directly into
  /// the channel when space is available.
  ///
  /// If the channel is closed, the future resolves to `nullopt`.
  ReserveSendFuture<T> ReserveSend() {
    if (channel_ == nullptr) {
      return ReserveSendFuture<T>(ReserveSendFuture<T>::kClosed);
    }
    return ReserveSendFuture<T>(channel_);
  }

  /// Synchronously attempts to send `value` if there is space in the channel.
  /// Returns `true` if successful.
  /// This operation is thread-safe and may be called from outside of an async
  /// context.
  bool TrySend(const T& value) {
    if (channel_ == nullptr) {
      return false;
    }
    return channel_->TryPush(value);
  }

  /// Synchronously attempts to send `value` if there is space in the channel.
  /// Returns `true` if successful.
  /// This operation is thread-safe and may be called from outside of an async
  /// context.
  bool TrySend(T&& value) {
    if (channel_ == nullptr) {
      return false;
    }
    return channel_->TryPush(std::move(value));
  }

  /// Removes this sender from its channel, preventing it from writing further
  /// values.
  ///
  /// The channel may remain open if other senders and receivers exist.
  void Disconnect() {
    if (channel_ != nullptr) {
      channel_->remove_sender();
      channel_ = nullptr;
    }
  }

  /// Returns the remaining capacity of the channel.
  uint16_t remaining_capacity() const {
    return channel_ != nullptr ? channel_->remaining_capacity() : 0;
  }

  /// Returns the maximum capacity of the channel.
  uint16_t capacity() const {
    return channel_ != nullptr ? channel_->capacity() : 0;
  }

 private:
  template <typename U>
  friend class Channel;

  explicit Sender(Channel<T>* channel) : channel_(channel) {
    if (channel_ != nullptr) {
      channel_->add_sender();
    }
  }

  Channel<T>* channel_;
};

/// Creates a dynamically allocated channel which supports multiple producers
/// and multiple consumers with a fixed storage capacity.
///
/// Returns a `DynamicChannel` handle to the channel which may be used to
/// create senders and receivers. The handle itself can be dropped without
/// affecting the channel.
///
/// All allocation occurs during the creation of the channel. After this
/// function returns, usage of the channel is guaranteed not to allocate.
/// If allocation fails, returns `std::nullopt`.
///
/// The channel remains open as long as at least one sender and one receiver
//
template <typename T>
std::optional<DynamicChannel<T>> CreateDynamicChannel(Allocator& alloc,
                                                      uint16_t capacity) {
  auto channel = Channel<T>::Allocated(alloc, capacity);
  if (!channel) {
    return std::nullopt;
  }
  return DynamicChannel<T>(channel);
}

}  // namespace pw::async2::experimental
