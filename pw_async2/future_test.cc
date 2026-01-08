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

#include "pw_async2/future.h"

#include "pw_async2/dispatcher_for_test.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/try.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::Context;
using pw::async2::DispatcherForTest;
using pw::async2::is_future_v;
using pw::async2::ListableFutureWithWaker;
using pw::async2::ListFutureProvider;
using pw::async2::PendFuncTask;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;
using pw::async2::SingleFutureProvider;

class SimpleIntFuture;

class SimpleAsyncInt {
 public:
  SimpleIntFuture Get();
  std::optional<SimpleIntFuture> GetSingle();

  void Set(int value) {
    {
      std::lock_guard lock(list_provider_.lock());
      PW_ASSERT(!value_.has_value());
      value_ = value;
    }
    ResolveAllFutures();
  }

  void ResolveAllFutures();

 private:
  friend class SimpleIntFuture;

  // This object stores both a list provider and a single provider for testing
  // purposes. In actual usage, only one of these would be needed, depending on
  // how many consumers the operation allows.
  ListFutureProvider<SimpleIntFuture> list_provider_;
  SingleFutureProvider<SimpleIntFuture> single_provider_;
  std::optional<int> value_;
};

class SimpleIntFuture : public ListableFutureWithWaker<SimpleIntFuture, int> {
 public:
  static constexpr const char kWaitReason[] = "SimpleIntFuture";

  SimpleIntFuture(SimpleIntFuture&& other) noexcept
      : ListableFutureWithWaker(kMovedFrom),
        async_int_(std::exchange(other.async_int_, nullptr)) {
    ListableFutureWithWaker::MoveFrom(other);
  }

  SimpleIntFuture& operator=(SimpleIntFuture&& other) noexcept {
    async_int_ = std::exchange(other.async_int_, nullptr);
    ListableFutureWithWaker::MoveFrom(other);
    return *this;
  }

  Poll<int> DoPend(Context&) {
    PW_ASSERT(async_int_ != nullptr);
    std::lock_guard guard(lock());

    if (!async_int_->value_.has_value()) {
      return Pending();
    }

    return async_int_->value_.value();
  }

 private:
  friend class SimpleAsyncInt;
  friend class ListableFutureWithWaker<SimpleIntFuture, int>;

  SimpleIntFuture(SimpleAsyncInt& async_int,
                  ListFutureProvider<SimpleIntFuture>& provider)
      : ListableFutureWithWaker(provider), async_int_(&async_int) {}

  SimpleIntFuture(SimpleAsyncInt& async_int,
                  SingleFutureProvider<SimpleIntFuture>& provider)
      : ListableFutureWithWaker(provider), async_int_(&async_int) {}

  SimpleAsyncInt* async_int_;
};

SimpleIntFuture SimpleAsyncInt::Get() {
  return SimpleIntFuture(*this, list_provider_);
}

std::optional<SimpleIntFuture> SimpleAsyncInt::GetSingle() {
  if (!single_provider_.has_future()) {
    return SimpleIntFuture(*this, single_provider_);
  }
  return std::nullopt;
}

void SimpleAsyncInt::ResolveAllFutures() {
  while (auto future = list_provider_.Pop()) {
    future->get().Wake();
  }

  if (auto future = single_provider_.Take()) {
    future->get().Wake();
  }
}

class NotificationFuture;

class AsyncNotification {
 public:
  NotificationFuture Wait();

  void Notify() {
    {
      std::lock_guard lock(list_provider_.lock());
      completed_ = true;
    }
    ResolveAllFutures();
  }

  void ResolveAllFutures();

 private:
  friend class NotificationFuture;

  ListFutureProvider<NotificationFuture> list_provider_;
  bool completed_ = false;
};

class NotificationFuture
    : public ListableFutureWithWaker<NotificationFuture, void> {
 public:
  static constexpr const char kWaitReason[] = "NotificationFuture";

  NotificationFuture(NotificationFuture&& other) noexcept
      : ListableFutureWithWaker(kMovedFrom),
        async_void_(std::exchange(other.async_void_, nullptr)) {
    ListableFutureWithWaker::MoveFrom(other);
  }

  NotificationFuture& operator=(NotificationFuture&& other) noexcept {
    async_void_ = std::exchange(other.async_void_, nullptr);
    ListableFutureWithWaker::MoveFrom(other);
    return *this;
  }

  Poll<> DoPend(Context&) {
    PW_ASSERT(async_void_ != nullptr);
    std::lock_guard guard(lock());

    if (!async_void_->completed_) {
      return Pending();
    }

    return Ready();
  }

 private:
  friend class AsyncNotification;
  friend class ListableFutureWithWaker<NotificationFuture, void>;

  NotificationFuture(AsyncNotification& async_void,
                     ListFutureProvider<NotificationFuture>& provider)
      : ListableFutureWithWaker(provider), async_void_(&async_void) {}

  AsyncNotification* async_void_;
};

NotificationFuture AsyncNotification::Wait() {
  return NotificationFuture(*this, list_provider_);
}

void AsyncNotification::ResolveAllFutures() {
  while (auto future = list_provider_.Pop()) {
    future->get().Wake();
  }
}

static_assert(!is_future_v<int>);
static_assert(is_future_v<SimpleIntFuture>);
static_assert(is_future_v<NotificationFuture>);

class FakeFuture {
 public:
  using value_type = int;
  Poll<int> Pend(Context& cx);
  bool is_complete() const;
};
static_assert(is_future_v<FakeFuture>);

class MissingValueType {
 public:
  Poll<int> Pend(Context& cx);
  bool is_complete() const;
};
static_assert(!is_future_v<MissingValueType>);

class MissingPend {
 public:
  using value_type = int;
  bool is_complete() const;
};
static_assert(!is_future_v<MissingPend>);

class MissingIsComplete {
 public:
  using value_type = int;
  Poll<int> Pend(Context& cx);
};
static_assert(!is_future_v<MissingIsComplete>);

class WrongPendSignature {
 public:
  using value_type = int;
  Poll<int> Pend();  // Missing Context&
  bool is_complete() const;
};
static_assert(!is_future_v<WrongPendSignature>);

class WrongPendReturnType {
 public:
  using value_type = int;
  void Pend(Context& cx);
  bool is_complete() const;
};
static_assert(!is_future_v<WrongPendReturnType>);

class ExtraArgPend {
 public:
  using value_type = int;
  Poll<int> Pend(Context& cx, int extra = 0);
  bool is_complete() const;
};
static_assert(!is_future_v<ExtraArgPend>);

TEST(Future, Pend) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future = provider.Get();
  int result = -1;

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(27);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result, 27);
}

TEST(Future, VoidFuture) {
  DispatcherForTest dispatcher;
  AsyncNotification notification;

  NotificationFuture future = notification.Wait();
  bool completed = false;

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY(future.Pend(cx));
    completed = true;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());
  EXPECT_FALSE(completed);

  notification.Notify();
  dispatcher.RunToCompletion();
  EXPECT_TRUE(completed);
}

TEST(Future, MoveAssign) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future1 = provider.Get();

  SimpleIntFuture future2 = provider.Get();
  future1 = std::move(future2);

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future1.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(99);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result, 99);
}

TEST(Future, MoveConstruct) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future1 = provider.Get();
  SimpleIntFuture future2(std::move(future1));

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future2.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(99);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result, 99);
}

TEST(Future, DestroyBeforeCompletion) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  {
    [[maybe_unused]] SimpleIntFuture future = provider.Get();
  }

  // The provider should not crash by waking a nonexistent future.
  provider.Set(99);
}

TEST(ListFutureProvider, MultipleFutures) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  SimpleIntFuture future1 = provider.Get();
  SimpleIntFuture future2 = provider.Get();
  int result1 = -1;
  int result2 = -1;

  PendFuncTask task1([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future1.Pend(cx));
    result1 = value;
    return Ready();
  });

  PendFuncTask task2([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future2.Pend(cx));
    result2 = value;
    return Ready();
  });

  dispatcher.Post(task1);
  dispatcher.Post(task2);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(33);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result1, 33);
  EXPECT_EQ(result2, 33);
}

TEST(SingleFutureProvider, VendsAndResolvesFuture) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  std::optional<SimpleIntFuture> future = provider.GetSingle();

  ASSERT_TRUE(future.has_value());

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future->Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(96);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result, 96);
}

TEST(SingleFutureProvider, OnlyAllowsOneFutureToExist) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  {
    std::optional<SimpleIntFuture> future1 = provider.GetSingle();
    std::optional<SimpleIntFuture> future2 = provider.GetSingle();
    EXPECT_TRUE(future1.has_value());
    EXPECT_FALSE(future2.has_value());
  }

  // `future1` went out of scope, so we should be allowed to get a new one.
  std::optional<SimpleIntFuture> future = provider.GetSingle();
  ASSERT_TRUE(future.has_value());

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future->Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(93);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result, 93);

  // The operation has resolved, so a new future should be obtainable.
  std::optional<SimpleIntFuture> new_future = provider.GetSingle();
  ASSERT_TRUE(new_future.has_value());
}

TEST(ListableFutureWithWaker, RelistsItselfOnPending) {
  DispatcherForTest dispatcher;
  SimpleAsyncInt provider;

  std::optional<SimpleIntFuture> future = provider.GetSingle();
  ASSERT_TRUE(future.has_value());

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future->Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);

  // ResolveAllFutures pops the future off the list. Since no value is set,
  // the task will still be pending. The future should re-add itself to the list
  // to prevent the task from being permanently stalled.
  provider.ResolveAllFutures();
  EXPECT_TRUE(dispatcher.RunUntilStalled());
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);  // Task ran again.

  provider.Set(99);
  dispatcher.RunToCompletion();
  EXPECT_EQ(dispatcher.tasks_polled(), 3u);
  EXPECT_EQ(result, 99);
}

#if PW_NC_TEST(FutureWaitReasonMustBeCharArray)
PW_NC_EXPECT("kWaitReason must be a character array");
class BadFuture : public ListableFutureWithWaker<BadFuture, int> {
 public:
  BadFuture() : ListableFutureWithWaker(kMovedFrom) {}
  static constexpr const char* kWaitReason = "this is a char* not an array";
  Poll<int> DoPend(Context&) { return 5; }
};

void ShouldAssert() {
  BadFuture future;
  PendFuncTask task([&](Context& cx) { return future.Pend(cx).Readiness(); });
}
#endif  // PW_NC_TEST

class TestAsyncInt;

class TestIntFuture {
 public:
  using value_type = int;

  TestIntFuture(TestIntFuture&& other) noexcept
      : core_(std::move(other.core_)),
        async_int_(std::exchange(other.async_int_, nullptr)) {}

  TestIntFuture& operator=(TestIntFuture&& other) noexcept {
    if (this != &other) {
      core_ = std::move(other.core_);
      async_int_ = std::exchange(other.async_int_, nullptr);
    }
    return *this;
  }

  Poll<int> Pend(Context& cx) { return core_.DoPend(*this, cx); }

  // Exposed for testing.
  const pw::async2::FutureCore& core() const { return core_; }

  [[nodiscard]] bool is_complete() const { return core_.is_complete(); }

 private:
  friend class TestAsyncInt;
  friend class pw::async2::FutureCore;

  static constexpr const char kWaitReason[] = "TestIntFuture";

  TestIntFuture(TestAsyncInt& async_int);

  TestIntFuture(TestAsyncInt& async_int,
                pw::async2::FutureCore::ReadyForCompletion)
      : core_(pw::async2::FutureCore::kReadyForCompletion),
        async_int_(&async_int) {}

  Poll<int> DoPend(Context&);

  pw::async2::FutureCore core_;
  TestAsyncInt* async_int_;
};

class TestAsyncInt {
 public:
  TestIntFuture Get() { return TestIntFuture(*this); }

  TestIntFuture SetAndGet(int value) {
    PW_ASSERT(!value_.has_value());
    value_ = value;
    return TestIntFuture(*this, pw::async2::FutureCore::kReadyForCompletion);
  }

  void Set(int value) {
    PW_ASSERT(!value_.has_value());
    value_ = value;
    list_.ResolveAll();
  }

  void WakeAllFuturesWithoutSettingValue() {
    TestIntFuture* future;
    while ((future = list_.PopIfAvailable()) != nullptr) {
      future->core_.Wake();
    }
  }

 private:
  friend class TestIntFuture;

  pw::async2::FutureList<&TestIntFuture::core_> list_;
  std::optional<int> value_;
};

TestIntFuture::TestIntFuture(TestAsyncInt& async_int)
    : core_(pw::async2::FutureCore::kPending), async_int_(&async_int) {
  async_int_->list_.Push(core_);
}

Poll<int> TestIntFuture::DoPend(Context&) {
  PW_ASSERT(async_int_ != nullptr);
  if (async_int_->value_.has_value()) {
    return Ready(*async_int_->value_);
  }
  if (!core_.in_list()) {
    async_int_->list_.Push(core_);
  }
  return Pending();
}

static_assert(!is_future_v<pw::async2::FutureCore>);
static_assert(is_future_v<TestIntFuture>);

TEST(FutureCore, Pend) {
  DispatcherForTest dispatcher;
  TestAsyncInt provider;

  TestIntFuture future = provider.Get();
  EXPECT_TRUE(future.core().is_pendable());
  EXPECT_FALSE(future.core().is_ready());
  EXPECT_FALSE(future.core().is_complete());

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future.Pend(cx));
    EXPECT_EQ(value, 42);
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(42);
  dispatcher.RunToCompletion();

  EXPECT_TRUE(future.core().is_complete());
  EXPECT_FALSE(future.core().is_pendable());
}

TEST(FutureCore, PendReady) {
  DispatcherForTest dispatcher;
  TestAsyncInt provider;

  TestIntFuture future = provider.SetAndGet(65535);
  EXPECT_TRUE(future.core().is_pendable());
  EXPECT_TRUE(future.core().is_ready());
  EXPECT_FALSE(future.core().is_complete());

  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future.Pend(cx));
    EXPECT_EQ(value, 65535);
    return Ready();
  });

  dispatcher.Post(task);
  dispatcher.RunToCompletion();

  EXPECT_TRUE(future.core().is_complete());
  EXPECT_FALSE(future.core().is_pendable());
}

TEST(FutureCore, MoveAssign) {
  DispatcherForTest dispatcher;
  TestAsyncInt provider;

  TestIntFuture future1 = provider.Get();
  TestIntFuture future2 = provider.Get();

  future1 = std::move(future2);

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future1.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(100);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result, 100);
}

TEST(FutureCore, MoveConstruct) {
  DispatcherForTest dispatcher;
  TestAsyncInt provider;

  TestIntFuture future1 = provider.Get();
  TestIntFuture future2(std::move(future1));

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future2.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(101);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result, 101);
}

TEST(FutureCore, MultipleFutures) {
  DispatcherForTest dispatcher;
  TestAsyncInt provider;

  TestIntFuture future1 = provider.Get();
  TestIntFuture future2 = provider.Get();
  int result1 = -1;
  int result2 = -1;

  PendFuncTask task1([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future1.Pend(cx));
    result1 = value;
    return Ready();
  });

  PendFuncTask task2([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future2.Pend(cx));
    result2 = value;
    return Ready();
  });

  dispatcher.Post(task1);
  dispatcher.Post(task2);
  EXPECT_TRUE(dispatcher.RunUntilStalled());

  provider.Set(77);
  dispatcher.RunToCompletion();
  EXPECT_EQ(result1, 77);
  EXPECT_EQ(result2, 77);
}

TEST(FutureCore, RelistsItselfOnPending) {
  DispatcherForTest dispatcher;
  TestAsyncInt provider;

  TestIntFuture future = provider.Get();

  int result = -1;
  PendFuncTask task([&](Context& cx) -> Poll<> {
    PW_TRY_READY_ASSIGN(int value, future.Pend(cx));
    result = value;
    return Ready();
  });

  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled());
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);

  // Spuriously wake the futures.
  provider.WakeAllFuturesWithoutSettingValue();

  EXPECT_TRUE(dispatcher.RunUntilStalled());
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);

  provider.Set(88);
  dispatcher.RunToCompletion();
  EXPECT_EQ(dispatcher.tasks_polled(), 3u);
  EXPECT_EQ(result, 88);
}

}  // namespace
