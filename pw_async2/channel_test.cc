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

#include "pw_async2/channel.h"

#include "pw_allocator/testing.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/try.h"
#include "pw_containers/vector.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::ChannelStorage;
using pw::async2::Context;
using pw::async2::CreateMpmcChannel;
using pw::async2::CreateMpscChannel;
using pw::async2::CreateSpmcChannel;
using pw::async2::CreateSpscChannel;
using pw::async2::Dispatcher;
using pw::async2::MpmcChannelHandle;
using pw::async2::PendFuncTask;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;
using pw::async2::ReceiveFuture;
using pw::async2::Receiver;
using pw::async2::ReserveSendFuture;
using pw::async2::Sender;
using pw::async2::SendFuture;
using pw::async2::SendReservation;
using pw::async2::Task;

class SenderTask : public Task {
 public:
  SenderTask(Sender<int> sender, int start, int end)
      : Task(PW_ASYNC_TASK_NAME("SenderTask")),
        sender_(std::move(sender)),
        next_(start),
        end_(end) {}

  bool succeeded() const { return success_; }

 private:
  Poll<> DoPend(Context& cx) override {
    while (next_ <= end_) {
      if (!future_.has_value()) {
        future_.emplace(sender_.Send(next_));
      }

      PW_TRY_READY_ASSIGN(bool sent, future_->Pend(cx));
      if (!sent) {
        success_ = false;
        return Ready();
      }

      future_.reset();
      next_++;
    }

    success_ = true;
    sender_.Disconnect();
    return Ready();
  }

  Sender<int> sender_;
  std::optional<SendFuture<int>> future_;
  bool success_ = false;
  int next_;
  int end_;
};

class ReceiverTask : public Task {
 public:
  ReceiverTask(Receiver<int> receiver)
      : Task(PW_ASYNC_TASK_NAME("ReceiverTask")),
        receiver_(std::move(receiver)) {}

  pw::Vector<int>& received() { return received_; }
  const pw::Vector<int>& received() const { return received_; }

 private:
  Poll<> DoPend(Context& cx) override {
    while (true) {
      if (!future_.has_value()) {
        future_.emplace(receiver_.Receive());
      }

      PW_TRY_READY_ASSIGN(std::optional<int> value, future_->Pend(cx));
      if (!value.has_value()) {
        break;
      }

      received_.push_back(*value);
      future_.reset();
    }

    receiver_.Disconnect();
    return Ready();
  }

  Receiver<int> receiver_;
  std::optional<ReceiveFuture<int>> future_;
  pw::Vector<int, 10> received_;
};

class DisconnectingReceiverTask : public Task {
 public:
  DisconnectingReceiverTask(Receiver<int> receiver, size_t disconnect_after)
      : Task(PW_ASYNC_TASK_NAME("DisconnectingReceiverTask")),
        receiver_(std::move(receiver)),
        disconnect_after_(disconnect_after) {}

 private:
  Poll<> DoPend(Context& cx) override {
    while (disconnect_after_ > 0) {
      if (!future_.has_value()) {
        future_.emplace(receiver_.Receive());
      }

      PW_TRY_READY_ASSIGN(std::optional<int> value, future_->Pend(cx));
      if (!value.has_value()) {
        break;
      }
      future_.reset();
      disconnect_after_--;
    }

    receiver_.Disconnect();
    return Ready();
  }

  Receiver<int> receiver_;
  std::optional<ReceiveFuture<int>> future_;
  size_t disconnect_after_;
};

void ExpectReceived1To6(const pw::Vector<int>& received) {
  ASSERT_EQ(received.size(), 6u);
  for (size_t i = 0; i < 6; ++i) {
    EXPECT_EQ(received[i], static_cast<int>(i + 1));
  }
}

TEST(StaticChannel, SingleProducerSingleConsumer) {
  Dispatcher dispatcher;

  ChannelStorage<int, 2> storage;
  auto [channel, sender, receiver] = CreateSpscChannel(storage);
  channel.Release();

  SenderTask sender_task(std::move(sender), 1, 6);
  ReceiverTask receiver_task(std::move(receiver));

  dispatcher.Post(sender_task);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  EXPECT_TRUE(sender_task.succeeded());
  ExpectReceived1To6(receiver_task.received());
}

TEST(StaticChannel, MultiProducerSingleConsumer) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto [channel, receiver] = CreateMpscChannel(storage);

  SenderTask sender_task_1(channel.CreateSender(), 1, 3);
  SenderTask sender_task_2(channel.CreateSender(), 4, 6);
  ReceiverTask receiver_task(std::move(receiver));
  channel.Release();

  dispatcher.Post(sender_task_1);
  dispatcher.Post(sender_task_2);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  EXPECT_TRUE(sender_task_1.succeeded());
  EXPECT_TRUE(sender_task_2.succeeded());

  pw::Vector<int, 10> received(receiver_task.received());
  std::stable_partition(
      received.begin(), received.end(), [](int x) { return x < 4; });
  ExpectReceived1To6(received);
}

TEST(StaticChannel, SingleProducerMultiConsumer) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto [channel, sender] = CreateSpmcChannel(storage);

  SenderTask sender_task(std::move(sender), 1, 6);
  ReceiverTask receiver_task_1(channel.CreateReceiver());
  ReceiverTask receiver_task_2(channel.CreateReceiver());
  channel.Release();

  dispatcher.Post(sender_task);
  dispatcher.Post(receiver_task_1);
  dispatcher.Post(receiver_task_2);

  dispatcher.RunToCompletion();

  EXPECT_TRUE(sender_task.succeeded());

  pw::Vector<int, 10> all_received;
  const auto& received_1 = receiver_task_1.received();
  const auto& received_2 = receiver_task_2.received();
  EXPECT_TRUE(std::is_sorted(received_1.begin(), received_1.end()));
  EXPECT_TRUE(std::is_sorted(received_2.begin(), received_2.end()));
  std::merge(received_1.begin(),
             received_1.end(),
             received_2.begin(),
             received_2.end(),
             std::back_inserter(all_received));
  ExpectReceived1To6(all_received);
}

TEST(StaticChannel, MultiProducerMultiConsumer) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  SenderTask sender_task_1(channel.CreateSender(), 1, 3);
  SenderTask sender_task_2(channel.CreateSender(), 4, 6);
  ReceiverTask receiver_task_1(channel.CreateReceiver());
  ReceiverTask receiver_task_2(channel.CreateReceiver());
  channel.Release();

  dispatcher.Post(sender_task_1);
  dispatcher.Post(sender_task_2);
  dispatcher.Post(receiver_task_1);
  dispatcher.Post(receiver_task_2);

  dispatcher.RunToCompletion();

  EXPECT_TRUE(sender_task_1.succeeded());
  EXPECT_TRUE(sender_task_2.succeeded());

  pw::Vector<int, 10> all_received;
  auto& received_1 = receiver_task_1.received();
  auto& received_2 = receiver_task_2.received();
  std::stable_partition(
      received_1.begin(), received_1.end(), [](int x) { return x < 4; });
  std::stable_partition(
      received_2.begin(), received_2.end(), [](int x) { return x < 4; });
  EXPECT_TRUE(std::is_sorted(received_1.begin(), received_1.end()));
  EXPECT_TRUE(std::is_sorted(received_2.begin(), received_2.end()));
  std::merge(received_1.begin(),
             received_1.end(),
             received_2.begin(),
             received_2.end(),
             std::back_inserter(all_received));
  ExpectReceived1To6(all_received);
}

TEST(StaticChannel, NonAsyncTrySend) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  Sender<int> sender = channel.CreateSender();
  ReceiverTask receiver_task(channel.CreateReceiver());
  channel.Release();

  dispatcher.Post(receiver_task);

  EXPECT_TRUE(sender.TrySend(1));
  EXPECT_TRUE(sender.TrySend(2));
  EXPECT_FALSE(sender.TrySend(3));
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  EXPECT_TRUE(sender.TrySend(3));
  EXPECT_TRUE(sender.TrySend(4));
  EXPECT_FALSE(sender.TrySend(5));
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  EXPECT_TRUE(sender.TrySend(5));
  EXPECT_TRUE(sender.TrySend(6));
  sender.Disconnect();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());

  ExpectReceived1To6(receiver_task.received());
}

TEST(StaticChannel, ReceiverDisconnects) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  SenderTask sender_task_1(channel.CreateSender(), 1, 10);
  SenderTask sender_task_2(channel.CreateSender(), 11, 20);
  DisconnectingReceiverTask receiver_task(channel.CreateReceiver(), 3);
  channel.Release();

  dispatcher.Post(sender_task_1);
  dispatcher.Post(sender_task_2);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  EXPECT_FALSE(sender_task_1.succeeded());
  EXPECT_FALSE(sender_task_2.succeeded());
}

class ReservedSenderTask : public Task {
 public:
  ReservedSenderTask(Sender<int> sender, int start, int end)
      : Task(PW_ASYNC_TASK_NAME("ReservedSenderTask")),
        sender_(std::move(sender)),
        next_(start),
        end_(end) {}

  bool succeeded() const { return success_; }

 private:
  Poll<> DoPend(Context& cx) override {
    while (next_ <= end_) {
      if (!future_.has_value()) {
        future_.emplace(sender_.ReserveSend());
      }

      PW_TRY_READY_ASSIGN(auto reservation, future_->Pend(cx));
      if (!reservation.has_value()) {
        success_ = false;
        return Ready();
      }

      reservation->Commit(next_);
      next_++;
      future_.reset();
    }

    success_ = true;
    sender_.Disconnect();
    return Ready();
  }

  Sender<int> sender_;
  std::optional<ReserveSendFuture<int>> future_;
  bool success_ = false;
  int next_;
  int end_;
};

TEST(StaticChannel, ReserveSend) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  ReservedSenderTask sender_task(channel.CreateSender(), 1, 6);
  ReceiverTask receiver_task(channel.CreateReceiver());
  channel.Release();

  dispatcher.Post(sender_task);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  EXPECT_TRUE(sender_task.succeeded());
  ExpectReceived1To6(receiver_task.received());
}

TEST(StaticChannel, ReserveSendReservesSpace) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  ReceiverTask receiver_task(channel.CreateReceiver());

  Sender<int> sender = channel.CreateSender();
  channel.Release();

  static constexpr int kReservedSendValue = 37;
  static constexpr int kFirstSendValue = 40;
  static constexpr int kSecondSendValue = 43;

  PendFuncTask reserved_sender_task(
      [sender = std::move(sender)](Context& cx) mutable -> Poll<> {
        // This task runs once without sleeping.
        // The channel has two slots. First, we reserve a slot through
        // `ReserveSend`, but don't commit a value. Then, we attempt to write
        // two values through the regular `Send` API. The first should succeed
        // as there is a second available slot, whereas the second should block.
        // Finally, commit the reserved slot.
        auto reserve_send_future = sender.ReserveSend();
        auto send_future_1 = sender.Send(kFirstSendValue);
        auto send_future_2 = sender.Send(kSecondSendValue);
        PW_TRY_READY_ASSIGN(auto reservation, reserve_send_future.Pend(cx));
        EXPECT_EQ(sender.remaining_capacity(), 1u);

        EXPECT_EQ(send_future_1.Pend(cx), Ready(true));
        EXPECT_EQ(sender.remaining_capacity(), 0u);
        EXPECT_EQ(send_future_2.Pend(cx), Pending());

        reservation->Commit(kReservedSendValue);
        EXPECT_EQ(sender.remaining_capacity(), 0u);

        sender.Disconnect();
        return Ready();
      });

  dispatcher.Post(reserved_sender_task);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  ASSERT_EQ(receiver_task.received().size(), 2u);
  EXPECT_EQ(receiver_task.received()[0], kFirstSendValue);
  EXPECT_EQ(receiver_task.received()[1], kReservedSendValue);
}

TEST(StaticChannel, ReserveSendReleasesSpaceWhenDropped) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  ReceiverTask receiver_task(channel.CreateReceiver());
  Sender<int> sender = channel.CreateSender();
  channel.Release();

  static constexpr int kFirstSendValue = 40;
  static constexpr int kSecondSendValue = 43;

  PendFuncTask reserved_sender_task(
      [sender = std::move(sender)](Context& cx) mutable -> Poll<> {
        // This task runs once without sleeping.
        // The channel has two slots. First, we reserve a slot through
        // `ReserveSend`, but don't commit a value. Then, we attempt to write
        // two values through the regular `Send` API. The first should succeed
        // and the second should block. Afterwards, we drop the reservation,
        // which should release the slot, allowing the second send to succeed.
        auto reserve_send_future = sender.ReserveSend();
        auto send_future_1 = sender.Send(kFirstSendValue);
        auto send_future_2 = sender.Send(kSecondSendValue);

        {
          PW_TRY_READY_ASSIGN(auto reservation, reserve_send_future.Pend(cx));
          EXPECT_EQ(sender.remaining_capacity(), 1u);
          EXPECT_EQ(send_future_1.Pend(cx), Ready(true));
          EXPECT_EQ(sender.remaining_capacity(), 0u);
          EXPECT_EQ(send_future_2.Pend(cx), Pending());
          EXPECT_EQ(sender.remaining_capacity(), 0u);
        }

        EXPECT_EQ(sender.remaining_capacity(), 1u);
        EXPECT_EQ(send_future_2.Pend(cx), Ready(true));
        EXPECT_EQ(sender.remaining_capacity(), 0u);

        sender.Disconnect();
        return Ready();
      });

  dispatcher.Post(reserved_sender_task);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  ASSERT_EQ(receiver_task.received().size(), 2u);
  EXPECT_EQ(receiver_task.received()[0], kFirstSendValue);
  EXPECT_EQ(receiver_task.received()[1], kSecondSendValue);
}

TEST(StaticChannel, ReserveSendManualCancel) {
  Dispatcher dispatcher;
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  ReceiverTask receiver_task(channel.CreateReceiver());
  Sender<int> sender = channel.CreateSender();
  channel.Release();

  static constexpr int kFirstSendValue = 40;
  static constexpr int kSecondSendValue = 43;

  PendFuncTask reserved_sender_task(
      [sender = std::move(sender)](Context& cx) mutable -> Poll<> {
        // This task runs once without sleeping.
        // The channel has two slots. First, we reserve a slot through
        // `ReserveSend`, but don't commit a value. Then, we attempt to write
        // two values through the regular `Send` API. The first should succeed
        // and the second should block. Afterwards, we cancel the reservation,
        // which should release the slot, allowing the second send to succeed.
        auto reserve_send_future = sender.ReserveSend();
        auto send_future_1 = sender.Send(kFirstSendValue);
        auto send_future_2 = sender.Send(kSecondSendValue);

        PW_TRY_READY_ASSIGN(auto reservation, reserve_send_future.Pend(cx));
        EXPECT_EQ(sender.remaining_capacity(), 1u);
        EXPECT_EQ(send_future_1.Pend(cx), Ready(true));
        EXPECT_EQ(sender.remaining_capacity(), 0u);
        EXPECT_EQ(send_future_2.Pend(cx), Pending());
        EXPECT_EQ(sender.remaining_capacity(), 0u);

        reservation->Cancel();

        EXPECT_EQ(sender.remaining_capacity(), 1u);
        EXPECT_EQ(send_future_2.Pend(cx), Ready(true));
        EXPECT_EQ(sender.remaining_capacity(), 0u);

        sender.Disconnect();
        return Ready();
      });

  dispatcher.Post(reserved_sender_task);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  ASSERT_EQ(receiver_task.received().size(), 2u);
  EXPECT_EQ(receiver_task.received()[0], kFirstSendValue);
  EXPECT_EQ(receiver_task.received()[1], kSecondSendValue);
}

TEST(StaticChannel, RemainingCapacity) {
  ChannelStorage<int, 2> storage;
  auto channel = CreateMpmcChannel(storage);

  Sender<int> sender = channel.CreateSender();

  EXPECT_EQ(sender.remaining_capacity(), 2u);
  EXPECT_EQ(sender.capacity(), 2u);

  EXPECT_TRUE(sender.TrySend(1));
  EXPECT_EQ(sender.remaining_capacity(), 1u);
  EXPECT_EQ(sender.capacity(), 2u);

  channel.Release();
}

class MoveOnly {
 public:
  explicit MoveOnly(int val) : value(val) {}

  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  MoveOnly(MoveOnly&& other) : value(other.value), moved(other.moved + 1) {}
  MoveOnly& operator=(MoveOnly&& other) {
    value = other.value;
    moved = other.moved + 1;
    return *this;
  }

  operator int() const { return value; }

  int value;
  int moved = 0;
};

TEST(StaticChannel, MoveOnly) {
  Dispatcher dispatcher;
  ChannelStorage<MoveOnly, 3> storage;
  auto channel = CreateMpmcChannel(storage);

  Sender<MoveOnly> sender = channel.CreateSender();
  Receiver<MoveOnly> receiver = channel.CreateReceiver();
  channel.Release();

  PendFuncTask sender_task(
      [sender = std::move(sender)](Context& cx) mutable -> Poll<> {
        MoveOnly move_only_1(1);
        auto send_future = sender.Send(std::move(move_only_1));
        EXPECT_EQ(send_future.Pend(cx), Ready(true));

        MoveOnly move_only_2(2);
        auto reserve_send_future = sender.ReserveSend();
        Poll<std::optional<SendReservation<MoveOnly>>> poll =
            reserve_send_future.Pend(cx);
        auto& reservation_1 = poll.value();
        reservation_1->Commit(std::move(move_only_2));

        reserve_send_future = sender.ReserveSend();
        poll = reserve_send_future.Pend(cx);
        auto& reservation_2 = poll.value();
        reservation_2->Commit(3);

        sender.Disconnect();
        return Ready();
      });

  PendFuncTask receiver_task(
      [receiver = std::move(receiver)](Context& cx) mutable -> Poll<> {
        auto receive_future = receiver.Receive();
        auto poll1 = receive_future.Pend(cx);
        EXPECT_EQ(poll1.value(), MoveOnly(1));

        receive_future = receiver.Receive();
        auto poll2 = receive_future.Pend(cx);
        EXPECT_EQ(poll2.value(), MoveOnly(2));

        receive_future = receiver.Receive();
        auto poll3 = receive_future.Pend(cx);
        EXPECT_EQ(poll3.value(), MoveOnly(3));

        receiver.Disconnect();
        return Ready();
      });

  dispatcher.Post(sender_task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());

  dispatcher.Post(receiver_task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(DynamicChannel, ForwardsDataAndAutomaticallyDeallocates) {
  pw::allocator::test::AllocatorForTest<1024> alloc;
  Dispatcher dispatcher;

  std::optional<MpmcChannelHandle<int>> channel =
      CreateMpmcChannel<int>(alloc, 2);
  ASSERT_TRUE(channel.has_value());

  Sender<int> sender = channel->CreateSender();
  Receiver<int> receiver = channel->CreateReceiver();
  channel->Release();

  EXPECT_EQ(alloc.metrics().num_allocations.value(), 2u);
  EXPECT_EQ(alloc.metrics().num_deallocations.value(), 0u);

  SenderTask sender_task(std::move(sender), 1, 6);
  ReceiverTask receiver_task(std::move(receiver));

  dispatcher.Post(sender_task);
  dispatcher.Post(receiver_task);

  dispatcher.RunToCompletion();

  EXPECT_TRUE(sender_task.succeeded());
  ExpectReceived1To6(receiver_task.received());

  EXPECT_EQ(alloc.metrics().allocated_bytes.value(), 0u);
  EXPECT_EQ(alloc.metrics().num_allocations.value(), 2u);
  EXPECT_EQ(alloc.metrics().num_deallocations.value(), 2u);
}

TEST(DynamicChannel, RemainingCapacity) {
  pw::allocator::test::AllocatorForTest<1024> alloc;
  std::optional<MpmcChannelHandle<int>> channel =
      CreateMpmcChannel<int>(alloc, 2);
  ASSERT_TRUE(channel.has_value());

  Sender<int> sender = channel->CreateSender();

  EXPECT_EQ(sender.remaining_capacity(), 2u);
  EXPECT_EQ(sender.capacity(), 2u);

  EXPECT_TRUE(sender.TrySend(1));
  EXPECT_EQ(sender.remaining_capacity(), 1u);
  EXPECT_EQ(sender.capacity(), 2u);

  channel->Release();
}

TEST(DynamicChannel, AllocationFailure) {
  pw::allocator::test::AllocatorForTest<64> exhausted_alloc;
  exhausted_alloc.Exhaust();
  std::optional<MpmcChannelHandle<int>> channel =
      CreateMpmcChannel<int>(exhausted_alloc, 2);
  ASSERT_FALSE(channel.has_value());
  EXPECT_EQ(exhausted_alloc.metrics().allocated_bytes.value(), 0u);
  EXPECT_EQ(exhausted_alloc.metrics().num_allocations.value(), 0u);
  EXPECT_EQ(exhausted_alloc.metrics().num_deallocations.value(), 0u);

  // Enough space to allocate the deque, but not enough for the channel.
  // Deque should be allocated then deallocated.
  pw::allocator::test::AllocatorForTest<32> deque_only_alloc;
  channel = CreateMpmcChannel<int>(deque_only_alloc, 2);
  ASSERT_FALSE(channel.has_value());
  EXPECT_EQ(deque_only_alloc.metrics().allocated_bytes.value(), 0u);
  EXPECT_EQ(deque_only_alloc.metrics().num_allocations.value(), 1u);
  EXPECT_EQ(deque_only_alloc.metrics().num_deallocations.value(), 1u);
}

}  // namespace
