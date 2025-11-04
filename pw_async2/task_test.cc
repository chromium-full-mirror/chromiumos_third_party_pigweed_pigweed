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

#include "pw_async2/task.h"

#include "pw_async2/dispatcher.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_thread/sleep.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace {

using namespace std::chrono_literals;

using pw::async2::Context;
using pw::async2::Dispatcher;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;
using pw::async2::Task;

class BlockingTask : public Task {
 public:
  explicit BlockingTask(Poll<> result_to_return)
      : Task(PW_ASYNC_TASK_NAME("BlockingTask")),
        result_to_return_(result_to_return) {}

  Poll<> DoPend(Context&) override {
    ready_to_deregister_.release();
    wait_for_deregister_.acquire();
    // Don't bother storing a waker; the test will be deregistered immediately.
    return result_to_return_;
  }

  void WaitUntilRunning() { ready_to_deregister_.acquire(); }

  void Unblock() { wait_for_deregister_.release(); }

 private:
  pw::sync::BinarySemaphore ready_to_deregister_;
  pw::sync::BinarySemaphore wait_for_deregister_;

  Poll<> result_to_return_ = Pending();
};

TEST(Task, IsRegistered) {
  BlockingTask task(Ready());
  EXPECT_FALSE(task.IsRegistered());

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(task.IsRegistered());
}

TEST(Task, DeregisterWhileSleeping) {
  Dispatcher dispatcher;
  BlockingTask task(Ready());
  dispatcher.Post(task);

  task.Deregister();
  EXPECT_FALSE(task.IsRegistered());
}

void DeregisterWhileRunning(Poll<> task_return) {
  Dispatcher dispatcher;

  BlockingTask task(task_return);
  dispatcher.Post(task);

  pw::thread::test::TestThreadContext context;
  pw::Thread dispatcher_thread(context.options(),
                               [&dispatcher] { dispatcher.RunToCompletion(); });

  task.WaitUntilRunning();

  // Start a thread to unblock the task. The thread sleeps for a bit, so it
  // MIGHT unblock the task after Deregister() is called, but it's not
  // guaranteed.
  pw::thread::test::TestThreadContext unblock_context;
  pw::Thread unblock_thread(unblock_context.options(), [&task]() {
    pw::this_thread::sleep_for(10ms);
    task.Unblock();
  });

  task.Deregister();

  EXPECT_FALSE(task.IsRegistered());

  unblock_thread.join();
  dispatcher_thread.join();
}

TEST(Task, DeregisterRunningTask_TaskReturnsPending) {
  DeregisterWhileRunning(Pending());
}

TEST(Task, DeregisterRunningTask_TaskReturnsReady) {
  DeregisterWhileRunning(Ready());
}

}  // namespace
