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

#include "pw_async2/callback_task.h"

#include <optional>

#include "pw_async2/dispatcher.h"
#include "pw_async2/value_future.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::async2::Dispatcher;
using ::pw::async2::FutureCallbackTask;
using ::pw::async2::Pending;
using ::pw::async2::Ready;
using ::pw::async2::ValueFuture;
using ::pw::async2::ValueProvider;

TEST(FutureCallbackTask, PendsFutureUntilReady) {
  ValueProvider<char> provider;
  char result = '\0';

  FutureCallbackTask<ValueFuture<char>> task(provider.Get(),
                                             [&result](char c) { result = c; });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(result, '\0');

  provider.Resolve('b');

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(result, 'b');

  EXPECT_FALSE(task.IsRegistered());
}

TEST(FutureCallbackTask, ImmediatelyReturnsReady) {
  char result = '\0';

  FutureCallbackTask<ValueFuture<char>> task(ValueFuture<char>::Resolved('b'),
                                             [&result](char c) { result = c; });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(result, 'b');

  EXPECT_FALSE(task.IsRegistered());
}

TEST(FutureCallbackTask, VoidFuture) {
  ValueProvider<void> provider;

  bool completed = false;

  FutureCallbackTask<ValueFuture<void>> task(
      provider.Get(), [&completed]() { completed = true; });

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_FALSE(completed);

  provider.Resolve();

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_TRUE(completed);

  EXPECT_FALSE(task.IsRegistered());
}

}  // namespace
