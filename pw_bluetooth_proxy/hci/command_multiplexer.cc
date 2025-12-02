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

#include "pw_assert/check.h"

namespace pw::bluetooth::proxy::hci {

class CommandMultiplexer::InterceptorStateWrapper
    : public InterceptorMap::Pair {
 public:
  virtual ~InterceptorStateWrapper() = default;
  virtual std::variant<EventInterceptorWrapper*, CommandInterceptorWrapper*>
  Downcast() = 0;

 protected:
  InterceptorStateWrapper(InterceptorId::ValueType interceptor_id)
      : InterceptorMap::Pair(interceptor_id) {}
};

class CommandMultiplexer::EventInterceptorState
    : public EventInterceptorMap::Pair {
 public:
  EventInterceptorState(EventCodeVariant key, EventHandler handler)
      : EventInterceptorMap::Pair(key), handler_(std::move(handler)) {}
  EventHandler& handler() { return handler_; }

 private:
  EventHandler handler_;
};

class CommandMultiplexer::EventInterceptorWrapper
    : public InterceptorStateWrapper {
 public:
  EventInterceptorWrapper(InterceptorId::ValueType interceptor_id,
                          EventCodeVariant key,
                          EventHandler&& handler)
      : InterceptorStateWrapper(interceptor_id),
        wrapped_(key, std::move(handler)) {}

  std::variant<EventInterceptorWrapper*, CommandInterceptorWrapper*> Downcast()
      override {
    return this;
  }

  EventInterceptorState& state() { return wrapped_; }

 private:
  EventInterceptorState wrapped_;
};

class CommandMultiplexer::CommandInterceptorState
    : public CommandInterceptorMap::Pair {
 public:
  CommandInterceptorState(pw::bluetooth::emboss::OpCode op_code,
                          CommandHandler&& handler)
      : CommandInterceptorMap::Pair(op_code), handler_(std::move(handler)) {}
  CommandHandler& handler() { return handler_; }

 private:
  CommandHandler handler_;
};

class CommandMultiplexer::CommandInterceptorWrapper
    : public InterceptorStateWrapper {
 public:
  CommandInterceptorWrapper(InterceptorId::ValueType interceptor_id,
                            pw::bluetooth::emboss::OpCode op_code,
                            CommandHandler&& handler)
      : InterceptorStateWrapper(interceptor_id),
        wrapped_(op_code, std::move(handler)) {}

  CommandInterceptorState& state() { return wrapped_; }

  std::variant<EventInterceptorWrapper*, CommandInterceptorWrapper*> Downcast()
      override {
    return this;
  }

 private:
  CommandInterceptorState wrapped_;
};

CommandMultiplexer::CommandMultiplexer(
    Allocator& allocator,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_host_fn,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_controller_fn,
    [[maybe_unused]] async2::TimeProvider<chrono::SystemClock>& time_provider,
    [[maybe_unused]] chrono::SystemClock::duration command_timeout)
    : allocator_(allocator) {}

CommandMultiplexer::CommandMultiplexer(
    Allocator& allocator,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_host_fn,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_controller_fn,
    [[maybe_unused]] Function<void()> timeout_fn,
    [[maybe_unused]] chrono::SystemClock::duration command_timeout)
    : allocator_(allocator) {}

Result<async2::Poll<>> CommandMultiplexer::PendCommandTimeout(
    [[maybe_unused]] async2::Context& cx) {
  // Not implemented.
  return Status::Unimplemented();
}

void CommandMultiplexer::HandleH4FromHost(MultiBuf::Instance&& h4_packet) {
  MultiBuf::Instance _(std::move(h4_packet));
  // Not implemented.
}

void CommandMultiplexer::HandleH4FromController(
    MultiBuf::Instance&& h4_packet) {
  MultiBuf::Instance _(std::move(h4_packet));
  // Not implemented.
}

expected<void, FailureWithBuffer> CommandMultiplexer::SendCommand(
    CommandPacket&& command,
    EventHandler&& event_handler,
    [[maybe_unused]] EventCodeVariant complete_event_code,
    [[maybe_unused]] pw::span<pw::bluetooth::emboss::OpCode> exclusions) {
  EventHandler _(std::move(event_handler));
  // Not implemented.
  return unexpected(
      FailureWithBuffer{Status::Unimplemented(), std::move(command.buffer)});
}

expected<void, FailureWithBuffer> CommandMultiplexer::SendEvent(
    EventPacket&& event) {
  // Not implemented.
  return unexpected(
      FailureWithBuffer{Status::Unimplemented(), std::move(event.buffer)});
}

Result<EventInterceptor> CommandMultiplexer::RegisterEventInterceptor(
    EventCodeVariant event_code, EventHandler&& handler) {
  std::lock_guard lock(mutex_);
  if (event_interceptors_.find(event_code) != event_interceptors_.end()) {
    return Status::AlreadyExists();
  }

  std::optional<InterceptorId> id = AllocateInterceptorId();
  if (!id.has_value() || !id->is_valid()) {
    // Exhausted ID space.
    return Status::Unavailable();
  }

  auto* interceptor = allocator_.New<EventInterceptorWrapper>(
      id->value(), event_code, std::move(handler));
  if (!interceptor) {
    // Exhausted allocator space.
    return Status::Unavailable();
  }

  interceptors_.insert(*interceptor);
  event_interceptors_.insert(interceptor->state());

  return EventInterceptor(*this, std::move(*id));
}

Result<CommandInterceptor> CommandMultiplexer::RegisterCommandInterceptor(
    pw::bluetooth::emboss::OpCode op_code, CommandHandler&& handler) {
  std::lock_guard lock(mutex_);
  if (command_interceptors_.find(op_code) != command_interceptors_.end()) {
    return Status::AlreadyExists();
  }

  std::optional<InterceptorId> id = AllocateInterceptorId();
  if (!id.has_value() || !id->is_valid()) {
    // Exhausted ID space.
    return Status::Unavailable();
  }

  auto* interceptor = allocator_.New<CommandInterceptorWrapper>(
      id->value(), op_code, std::move(handler));
  if (!interceptor) {
    // Exhausted allocator space.
    return Status::Unavailable();
  }

  interceptors_.insert(*interceptor);
  command_interceptors_.insert(interceptor->state());

  return CommandInterceptor(*this, std::move(*id));
}

void CommandMultiplexer::UnregisterInterceptor(InterceptorId id) {
  PW_CHECK(id.is_valid(), "Attempt to unregister invalid ID.");
  std::lock_guard lock(mutex_);
  auto iter = interceptors_.find(id.value());
  // Should be impossible to fail this check from type safety, failing would
  // require forging or reusing an ID.
  PW_CHECK(iter != interceptors_.end());

  auto downcast = iter->Downcast();
  static_assert(std::variant_size_v<decltype(downcast)> == 2,
                "Unhandled variant members.");
  if (auto** event = std::get_if<EventInterceptorWrapper*>(&downcast)) {
    event_interceptors_.erase((*event)->state().key());
  } else if (auto** cmd = std::get_if<CommandInterceptorWrapper*>(&downcast)) {
    command_interceptors_.erase((*cmd)->state().key());
  } else {
    PW_UNREACHABLE;
  }

  auto& interceptor = *iter;
  interceptors_.erase(iter);
  allocator_.Delete(&interceptor);
}

std::optional<CommandMultiplexer::InterceptorId>
CommandMultiplexer::AllocateInterceptorId() {
  // Ignoring lock safety, the capability doesn't pass through MintId, but this
  // function is annotated with PW_EXCLUSIVE_LOCKS_REQUIRED(mutex_), so we
  // should always hold mutex_ here.
  return id_mint_.MintId(
      [&](InterceptorId::ValueType candidate) PW_NO_LOCK_SAFETY_ANALYSIS {
        return interceptors_.find(candidate) != interceptors_.end();
      });
}

}  // namespace pw::bluetooth::proxy::hci
