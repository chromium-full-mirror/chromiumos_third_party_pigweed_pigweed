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

#include "pw_assert/check.h"
#include "pw_async2/dispatcher.h"
#include "pw_async2/future.h"
#include "pw_async2/pend_func_task.h"
#include "pw_digital_io/digital_io.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_unit_test/framework.h"

namespace {

// DOCSTAG: [pw_async2-examples-custom-future]
class ButtonReceiver;

class ButtonFuture
    : public pw::async2::ListableFutureWithWaker<ButtonFuture, void> {
 public:
  // Provide a descriptive reason which can be used to debug blocked tasks.
  static constexpr const char kWaitReason[] = "Waiting for button press";

  // You are required to implement move semantics for your custom future,
  // and to call `Base::MoveFrom` to ensure the intrusive list is updated.
  ButtonFuture(ButtonFuture&& other) : Base(Base::kMovedFrom) {
    Base::MoveFrom(other);
  }
  ButtonFuture& operator=(ButtonFuture&& other) {
    Base::MoveFrom(other);
    return *this;
  }

 private:
  using Base = pw::async2::ListableFutureWithWaker<ButtonFuture, void>;
  friend Base;
  friend class ButtonReceiver;

  explicit ButtonFuture(
      pw::async2::SingleFutureProvider<ButtonFuture>& provider)
      : Base(provider) {}

  void HandlePress() {
    pressed_ = true;
    Base::Wake();
  }

  pw::async2::Poll<> DoPend(pw::async2::Context&) {
    if (pressed_) {
      return pw::async2::Ready();
    }
    return pw::async2::Pending();
  }

  bool pressed_ = false;
};

class ButtonReceiver {
 public:
  explicit ButtonReceiver(pw::digital_io::DigitalInterrupt& line)
      : line_(line) {
    PW_CHECK_OK(line_.SetInterruptHandler(
        pw::digital_io::InterruptTrigger::kActivatingEdge,
        [this](pw::digital_io::State) { HandleInterrupt(); }));
    PW_CHECK_OK(line_.EnableInterruptHandler());
  }

  // Returns a future that completes when the button is pressed.
  ButtonFuture WaitForPress() {
    PW_ASSERT(!provider_.has_future());
    return ButtonFuture(provider_);
  }

 private:
  // Executed in interrupt context.
  // `SingleFutureProvider` is internally synchronized and interrupt-safe.
  void HandleInterrupt() {
    if (provider_.has_future()) {
      provider_.Take().HandlePress();
    }
  }

  pw::digital_io::DigitalInterrupt& line_;
  pw::async2::SingleFutureProvider<ButtonFuture> provider_;
};
// DOCSTAG: [pw_async2-examples-custom-future]

class DigitalInterruptMock : public pw::digital_io::DigitalInterrupt {
 public:
  DigitalInterruptMock() = default;

  void Trigger() {
    if (handler_) {
      handler_(pw::digital_io::State::kActive);
    }
  }

 private:
  pw::Status DoEnable(bool) override { return pw::OkStatus(); }

  pw::Status DoSetInterruptHandler(
      pw::digital_io::InterruptTrigger,
      pw::digital_io::InterruptHandler&& handler) override {
    handler_ = std::move(handler);
    return pw::OkStatus();
  }

  pw::Status DoEnableInterruptHandler(bool) override { return pw::OkStatus(); }

  pw::digital_io::InterruptHandler handler_;
};

TEST(CustomFuture, CompilesAndRuns) {
  pw::async2::Dispatcher dispatcher;
  DigitalInterruptMock line;

  ButtonReceiver receiver(line);
  ButtonFuture future = receiver.WaitForPress();

  pw::async2::PendFuncTask function_task([&future](pw::async2::Context& cx) {
    return future.Pend(cx).Readiness();
  });

  dispatcher.Post(function_task);

  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());

  // Simulate button press.
  line.Trigger();

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

}  // namespace
