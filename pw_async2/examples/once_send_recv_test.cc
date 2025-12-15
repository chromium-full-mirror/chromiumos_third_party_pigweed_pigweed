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

#define PW_LOG_MODULE_NAME "EXAMPLES_ONCE_SEND_RECV"

#include "pw_allocator/libc_allocator.h"
#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_async2/basic_dispatcher.h"
#include "pw_async2/context.h"
#include "pw_async2/coro.h"
#include "pw_async2/coro_or_else_task.h"
#include "pw_async2/dispatcher_for_test.h"
#include "pw_async2/once_sender.h"
#include "pw_async2/poll.h"
#include "pw_async2/try.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_unit_test/framework.h"

namespace examples::manual {

using ::pw::Result;
using ::pw::async2::BasicDispatcher;
using ::pw::async2::Context;
using ::pw::async2::MakeOnceSenderAndReceiver;
using ::pw::async2::OnceReceiver;
using ::pw::async2::Poll;
using ::pw::async2::PollResult;
using ::pw::async2::Ready;
using ::pw::async2::Task;

class ReceiveAndLogValueTask : public Task {
 public:
  // The receiver should take ownership of an appropriate OnceReceiver<T>.
  explicit ReceiveAndLogValueTask(OnceReceiver<int>&& int_receiver)
      : int_receiver_(std::move(int_receiver)) {}

 private:
  Poll<> DoPend(Context& cx) override {
    // DOCSTAG: [pw_async2-examples-once-send-recv-receiving]
    PW_TRY_READY_ASSIGN(Result<int> value, int_receiver_.Pend(cx));
    if (!value.ok()) {
      PW_LOG_ERROR(
          "OnceSender was destroyed without sending a message! Outrageous :(");
    }
    PW_LOG_INFO("Received the integer value: %d", *value);
    // DOCSTAG: [pw_async2-examples-once-send-recv-receiving]
    return Ready();
  }

  OnceReceiver<int> int_receiver_;
};

int main() {
  // DOCSTAG: [pw_async2-examples-once-send-recv-construction]
  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  ReceiveAndLogValueTask task(std::move(receiver));
  // DOCSTAG: [pw_async2-examples-once-send-recv-construction]
  BasicDispatcher dispatcher;
  dispatcher.Post(task);

  // DOCSTAG: [pw_async2-examples-once-send-recv-send-value]
  // Send a value to the task
  sender.emplace(5);
  // DOCSTAG: [pw_async2-examples-once-send-recv-send-value]

  dispatcher.RunToCompletion();

  return 0;
}

}  // namespace examples::manual

using ::pw::allocator::LibCAllocator;
using ::pw::async2::BasicDispatcher;
using ::pw::async2::Coro;
using ::pw::async2::CoroContext;
using ::pw::async2::CoroOrElseTask;
using ::pw::async2::MakeOnceSenderAndReceiver;
using ::pw::async2::OnceReceiver;
using ::pw::async2::Ready;

namespace examples::coro {

// The receiver should take ownership of an appropriate OnceReceiver<T>.
Coro<pw::Status> ReceiveAndLogValue(CoroContext&,
                                    OnceReceiver<int> int_receiver) {
  // DOCSTAG: [pw_async2-examples-once-send-recv-coro-await]
  pw::Result<int> value = co_await int_receiver;
  if (!value.ok()) {
    PW_LOG_ERROR(
        "OnceSender was destroyed without sending a message! Outrageous :(");
    co_return pw::Status::Cancelled();
  }
  PW_LOG_INFO("Got an int: %d", *value);
  // DOCSTAG: [pw_async2-examples-once-send-recv-coro-await]

  co_return pw::OkStatus();
}

int main() {
  LibCAllocator alloc;
  CoroContext coro_cx(alloc);
  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  CoroOrElseTask task(ReceiveAndLogValue(coro_cx, std::move(receiver)),
                      [](pw::Status) { PW_CRASH("Allocation failed"); });

  BasicDispatcher dispatcher;
  dispatcher.Post(task);
  PW_CHECK(dispatcher.RunUntilStalled());

  // Send a value to the task
  sender.emplace(5);

  PW_CHECK(!dispatcher.RunUntilStalled());
  return 0;
}

}  // namespace examples::coro

namespace {

using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::DispatcherForTest;
using ::pw::async2::MakeOnceSenderAndReceiver;
using ::pw::async2::Poll;
using ::pw::async2::PollResult;
using ::pw::async2::Ready;

TEST(OnceSendRecv, ReceiveAndLogValueTask) {
  // Ensure the example code runs.
  examples::manual::main();

  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  examples::manual::ReceiveAndLogValueTask task(std::move(receiver));
  DispatcherForTest dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());
  sender.emplace(5);
  dispatcher.RunToCompletion();
}

TEST(OnceSendRecv, ReceiveAndLogValueCoro) {
  // Ensure the example code runs.
  examples::coro::main();

  AllocatorForTest<256> alloc;
  CoroContext coro_cx(alloc);
  auto [sender, receiver] = MakeOnceSenderAndReceiver<int>();
  auto coro = examples::coro::ReceiveAndLogValue(coro_cx, std::move(receiver));
  DispatcherForTest dispatcher;
  EXPECT_TRUE(dispatcher.RunInTaskUntilStalled(coro).IsPending());
  sender.emplace(5);
  EXPECT_EQ(dispatcher.RunInTaskUntilStalled(coro), Ready(pw::OkStatus()));
}

}  // namespace
