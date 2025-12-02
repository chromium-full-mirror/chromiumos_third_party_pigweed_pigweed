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

#include "pw_bluetooth_proxy/hci/command_multiplexer.h"

#include <chrono>
#include <cstdint>

#include "pw_allocator/testing.h"
#include "pw_async2/dispatcher_for_test.h"
#include "pw_async2/pend_func_task.h"
#include "pw_async2/simulated_time_provider.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/vector.h"
#include "pw_function/function.h"
#include "pw_unit_test/framework.h"

namespace pw::bluetooth::proxy::hci {
namespace {

TEST(IdentifierTest, UniqueIdentifier) {
  using Int = uint8_t;
  constexpr size_t kMax = std::numeric_limits<Int>::max();

  IdentifierMint<uint8_t> mint;
  pw::Vector<Identifier<Int>, kMax> ids;
  for (size_t i = 0; i < kMax; ++i) {
    auto new_id = mint.MintId([&](Int candidate) {
      return std::find(ids.begin(), ids.end(), candidate) != ids.end();
    });
    ASSERT_TRUE(new_id.has_value());
    ASSERT_TRUE(new_id->is_valid());

    EXPECT_EQ(new_id->value(), i + 1);
    EXPECT_EQ(std::find(ids.begin(), ids.end(), new_id->value()), ids.end());

    ids.push_back(std::move(*new_id));
    EXPECT_FALSE(new_id->is_valid());
  }

  {
    // Allocation exhausted, should return std::nullopt.
    auto result = mint.MintId([&](Int candidate) {
      return std::find(ids.begin(), ids.end(), candidate) != ids.end();
    });
    EXPECT_FALSE(result.has_value());
  }

  ids.erase(std::remove(ids.begin(), ids.end(), 42), ids.end());

  {
    // Allocation opened up at 42, confirm allocation.
    auto result = mint.MintId([&](Int candidate) {
      return std::find(ids.begin(), ids.end(), candidate) != ids.end();
    });
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value(), 42);
    ids.push_back(std::move(*result));
  }

  {
    // Allocation exhausted again, should return std::nullopt.
    auto result = mint.MintId([&](Int candidate) {
      return std::find(ids.begin(), ids.end(), candidate) != ids.end();
    });
    EXPECT_FALSE(result.has_value());
  }
}

class CommandMultiplexerTest : public ::testing::Test {
 public:
  static constexpr chrono::SystemClock::duration kTestTimeoutDuration =
      std::chrono::milliseconds(2);

  // Because we want to test both kinds of CommandMultiplexer (async vs cb),
  // some tests are implemented outside the fixture, this provides access to the
  // appropriate properties and functions.
  //
  // This is required because not all pw_unit_test backends support `TEST_P`
  // style test parameterization.
  class Accessor {
   public:
    CommandMultiplexer& hci_cmd_mux() { return hci_cmd_mux_; }
    Allocator& allocator() { return test_.allocator(); }
    async2::DispatcherForTest& dispatcher() { return test_.dispatcher(); }

   private:
    Accessor(CommandMultiplexerTest& test, CommandMultiplexer& hci_cmd_mux)
        : test_(test), hci_cmd_mux_(hci_cmd_mux) {}
    friend CommandMultiplexerTest;

    CommandMultiplexerTest& test_;
    CommandMultiplexer& hci_cmd_mux_;
  };

 protected:
  CommandMultiplexer& hci_cmd_mux_async2() {
    if (!hci_cmd_mux_async2_.has_value()) {
      hci_cmd_mux_async2_.emplace(allocator_,
                                  make_send_to_host_cb(),
                                  make_send_to_controller_cb(),
                                  time_provider_);
    }
    return *hci_cmd_mux_async2_;
  }

  CommandMultiplexer& hci_cmd_mux_timer() {
    if (!hci_cmd_mux_async2_.has_value()) {
      Function<void()> timeout_fn = [this] { OnTimeout(); };
      hci_cmd_mux_async2_.emplace(allocator_,
                                  make_send_to_host_cb(),
                                  make_send_to_controller_cb(),
                                  std::move(timeout_fn),
                                  kTestTimeoutDuration);
    }
    return *hci_cmd_mux_async2_;
  }

  Allocator& allocator() { return allocator_; }
  async2::DispatcherForTest& dispatcher() { return dispatcher_; }

  Accessor accessor_async2() { return Accessor(*this, hci_cmd_mux_async2()); }

  Accessor accessor_timer() { return Accessor(*this, hci_cmd_mux_timer()); }

 private:
  void OnTimeout() {}

  Function<void(MultiBuf::Instance&&)> make_send_to_host_cb() {
    return [this](MultiBuf::Instance&& packet) {
      // Intentionally fail assert if allocation fails, this is test code.
      packets_to_host_.push_back(std::move(packet));
    };
  }

  Function<void(MultiBuf::Instance&&)> make_send_to_controller_cb() {
    return [this](MultiBuf::Instance&& packet) {
      // Intentionally fail assert if allocation fails, this is test code.
      packets_to_controller_.push_back(std::move(packet));
    };
  }

  async2::DispatcherForTest dispatcher_{};
  async2::SimulatedTimeProvider<chrono::SystemClock> time_provider_{};
  pw::allocator::test::AllocatorForTest<208> allocator_{};

  pw::DynamicDeque<MultiBuf::Instance> packets_to_host_{allocator_};
  pw::DynamicDeque<MultiBuf::Instance> packets_to_controller_{allocator_};

  std::optional<CommandMultiplexer> hci_cmd_mux_async2_{std::nullopt};
  std::optional<CommandMultiplexer> hci_cmd_mux_timer_{std::nullopt};
};

using Accessor = CommandMultiplexerTest::Accessor;

TEST_F(CommandMultiplexerTest, AsyncTimeout) {
  auto& hci_cmd_mux = hci_cmd_mux_async2();

  std::optional<Result<async2::Poll<>>> pend_result;
  async2::PendFuncTask task{[&](async2::Context& cx) {
    pend_result = hci_cmd_mux.PendCommandTimeout(cx);
    return async2::Ready();
  }};

  dispatcher().Post(task);
  dispatcher().RunToCompletion();

  ASSERT_TRUE(pend_result.has_value());
  ASSERT_FALSE(pend_result->ok());
  // Not yet implemented.
  EXPECT_EQ(pend_result->status(), Status::Unimplemented());
}

TEST_F(CommandMultiplexerTest, AsyncTimeoutFailsSync) {
  auto& hci_cmd_mux = hci_cmd_mux_timer();

  std::optional<Result<async2::Poll<>>> pend_result;
  async2::PendFuncTask task{[&](async2::Context& cx) {
    pend_result = hci_cmd_mux.PendCommandTimeout(cx);
    return async2::Ready();
  }};

  dispatcher().Post(task);
  dispatcher().RunToCompletion();

  ASSERT_TRUE(pend_result.has_value());
  ASSERT_FALSE(pend_result->ok());
  EXPECT_EQ(pend_result->status(), Status::Unimplemented());
}

void TestSendCommand(Accessor test) {
  MultiBuf::Instance buffer(test.allocator());
  // Not yet implemented.
  auto result = test.hci_cmd_mux().SendCommand({std::move(buffer)}, nullptr);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().status(), Status::Unimplemented());
}

TEST_F(CommandMultiplexerTest, SendCommandAsync) {
  TestSendCommand(accessor_async2());
}
TEST_F(CommandMultiplexerTest, SendCommandTimer) {
  TestSendCommand(accessor_timer());
}

void TestSendEvent(Accessor test) {
  MultiBuf::Instance buffer(test.allocator());
  // Not yet implemented.
  auto result = test.hci_cmd_mux().SendEvent({std::move(buffer)});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().status(), Status::Unimplemented());
}

TEST_F(CommandMultiplexerTest, SendEventAsync) {
  TestSendEvent(accessor_async2());
}
TEST_F(CommandMultiplexerTest, SendEventTimer) {
  TestSendEvent(accessor_timer());
}

void TestRegisterEventInterceptor(Accessor test) {
  // Register an interceptor.
  auto result1 = test.hci_cmd_mux().RegisterEventInterceptor(
      emboss::EventCode::HARDWARE_ERROR, nullptr);
  EXPECT_TRUE(result1.ok());

  // Register a second interceptor.
  auto result2 = test.hci_cmd_mux().RegisterEventInterceptor(
      emboss::EventCode::INQUIRY_COMPLETE, nullptr);
  EXPECT_TRUE(result2.ok());

  // Ensure we can't register to an already-existing interceptor.
  auto result3 = test.hci_cmd_mux().RegisterEventInterceptor(
      emboss::EventCode::HARDWARE_ERROR, nullptr);
  ASSERT_FALSE(result3.ok());
  EXPECT_EQ(result3.status(), Status::AlreadyExists());

  // Reset result1, allowing us to register a different interceptor for the
  // same code.
  result1 = Status::Cancelled();
  auto result4 = test.hci_cmd_mux().RegisterEventInterceptor(
      emboss::EventCode::HARDWARE_ERROR, nullptr);
  EXPECT_TRUE(result4.ok());

  // Reset both active interceptors, clearing the maps.
  result2 = Status::Cancelled();
  result4 = Status::Cancelled();

  // Now register two more interceptors to ensure clearing the maps worked.
  auto result5 = test.hci_cmd_mux().RegisterEventInterceptor(
      emboss::EventCode::HARDWARE_ERROR, nullptr);
  EXPECT_TRUE(result5.ok());
  auto result6 = test.hci_cmd_mux().RegisterEventInterceptor(
      emboss::EventCode::INQUIRY_COMPLETE, nullptr);
  EXPECT_TRUE(result6.ok());
}

TEST_F(CommandMultiplexerTest, RegisterEventInterceptorAsync) {
  TestRegisterEventInterceptor(accessor_async2());
}
TEST_F(CommandMultiplexerTest, RegisterEventInterceptorTimer) {
  TestRegisterEventInterceptor(accessor_timer());
}

void TestRegisterCommandInterceptor(Accessor test) {
  // Register an interceptor.
  auto result1 = test.hci_cmd_mux().RegisterCommandInterceptor(
      emboss::OpCode::INQUIRY, nullptr);
  EXPECT_TRUE(result1.ok());

  // Register a second interceptor.
  auto result2 = test.hci_cmd_mux().RegisterCommandInterceptor(
      emboss::OpCode::DISCONNECT, nullptr);
  EXPECT_TRUE(result2.ok());

  // Ensure we can't register to an already-existing interceptor.
  auto result3 = test.hci_cmd_mux().RegisterCommandInterceptor(
      emboss::OpCode::INQUIRY, nullptr);
  ASSERT_FALSE(result3.ok());
  EXPECT_EQ(result3.status(), Status::AlreadyExists());

  // Reset result1, allowing us to register a different interceptor for the
  // same code.
  result1 = Status::Cancelled();
  auto result4 = test.hci_cmd_mux().RegisterCommandInterceptor(
      emboss::OpCode::INQUIRY, nullptr);
  EXPECT_TRUE(result4.ok());

  // Reset both active interceptors, clearing the maps.
  result2 = Status::Cancelled();
  result4 = Status::Cancelled();

  // Now register two more interceptors to ensure clearing the maps worked.
  auto result5 = test.hci_cmd_mux().RegisterCommandInterceptor(
      emboss::OpCode::INQUIRY, nullptr);
  EXPECT_TRUE(result5.ok());
  auto result6 = test.hci_cmd_mux().RegisterCommandInterceptor(
      emboss::OpCode::DISCONNECT, nullptr);
  EXPECT_TRUE(result6.ok());
}

TEST_F(CommandMultiplexerTest, RegisterCommandInterceptorAsync) {
  TestRegisterCommandInterceptor(accessor_async2());
}
TEST_F(CommandMultiplexerTest, RegisterCommandInterceptorTimer) {
  TestRegisterCommandInterceptor(accessor_timer());
}

}  // namespace
}  // namespace pw::bluetooth::proxy::hci
