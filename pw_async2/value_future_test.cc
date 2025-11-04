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

#include "pw_async2/value_future.h"

#include "pw_async2/dispatcher.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/try.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::BroadcastValueProvider;
using pw::async2::Context;
using pw::async2::Dispatcher;
using pw::async2::PendFuncTask;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;
using pw::async2::ValueFuture;
using pw::async2::ValueProvider;
using pw::async2::VoidFuture;

TEST(ValueFuture, Pend) {
  Dispatcher dispatcher;
  BroadcastValueProvider<int> provider;

  ValueFuture<int> future = provider.Get();
  int result = -1;

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  provider.Resolve(27);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(result, 27);
}

TEST(ValueFuture, Resolved) {
  Dispatcher dispatcher;
  auto future = ValueFuture<int>::Resolved(42);
  int result = -1;

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(result, 42);
}

TEST(ValueFuture, ResolvedInPlace) {
  Dispatcher dispatcher;
  auto future = ValueFuture<std::pair<int, int>>::Resolved(9, 3);

  std::optional<std::pair<int, int>> result;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(auto value, future.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, 9);
  EXPECT_EQ(result->second, 3);
}

TEST(ValueProvider, VendsAndResolvesFuture) {
  Dispatcher dispatcher;
  ValueProvider<int> provider;

  std::optional<ValueFuture<int>> future = provider.Get();
  ASSERT_TRUE(future.has_value());

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future->Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  provider.Resolve(91);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(result, 91);
}

TEST(ValueProvider, OnlyAllowsOneFutureToExist) {
  Dispatcher dispatcher;
  ValueProvider<int> provider;

  {
    std::optional<ValueFuture<int>> future1 = provider.TryGet();
    std::optional<ValueFuture<int>> future2 = provider.TryGet();
    EXPECT_TRUE(future1.has_value());
    EXPECT_FALSE(future2.has_value());
  }

  // `future1` went out of scope, so we should be allowed to get a new one.
  std::optional<ValueFuture<int>> future = provider.Get();
  ASSERT_TRUE(future.has_value());

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future->Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  provider.Resolve(82);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(result, 82);

  // The operation has resolved, so a new future should be obtainable.
  std::optional<ValueFuture<int>> new_future = provider.Get();
  EXPECT_TRUE(new_future.has_value());
}

TEST(ValueProvider, ResolveInPlace) {
  Dispatcher dispatcher;
  ValueProvider<std::pair<int, int>> provider;

  std::optional<ValueFuture<std::pair<int, int>>> future = provider.Get();
  ASSERT_TRUE(future.has_value());

  std::optional<std::pair<int, int>> result;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(auto value, future->Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  provider.Resolve(9, 3);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, 9);
  EXPECT_EQ(result->second, 3);
}

}  // namespace

TEST(VoidFuture, Pend) {
  Dispatcher dispatcher;
  BroadcastValueProvider<void> provider;

  VoidFuture future = provider.Get();
  bool completed = false;

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY(future.Pend(cx));
    completed = true;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_FALSE(completed);

  provider.Resolve();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_TRUE(completed);
}

TEST(VoidFuture, Resolved) {
  Dispatcher dispatcher;
  auto future = VoidFuture::Resolved();
  bool completed = false;

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY(future.Pend(cx));
    completed = true;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_TRUE(completed);
}

TEST(ValueProviderVoid, VendsAndResolvesFuture) {
  Dispatcher dispatcher;
  ValueProvider<void> provider;

  std::optional<VoidFuture> future = provider.Get();
  ASSERT_TRUE(future.has_value());

  bool completed = false;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY(future->Pend(cx));
    completed = true;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_FALSE(completed);

  provider.Resolve();
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_TRUE(completed);
}
