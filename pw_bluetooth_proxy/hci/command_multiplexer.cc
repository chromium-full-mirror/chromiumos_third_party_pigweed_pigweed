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

namespace pw::bluetooth::proxy::hci {

CommandMultiplexer::CommandMultiplexer(
    [[maybe_unused]] Allocator& allocator,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_host_fn,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_controller_fn,
    [[maybe_unused]] async2::TimeProvider<chrono::SystemClock>& time_provider,
    [[maybe_unused]] chrono::SystemClock::duration command_timeout) {}

CommandMultiplexer::CommandMultiplexer(
    [[maybe_unused]] Allocator& allocator,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_host_fn,
    [[maybe_unused]] Function<void(MultiBuf::Instance&& h4_packet)>&&
        send_to_controller_fn,
    [[maybe_unused]] Function<void()> timeout_fn,
    [[maybe_unused]] chrono::SystemClock::duration command_timeout) {}

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
    [[maybe_unused]] EventCodeVariant event_code, EventHandler&& handler) {
  EventHandler _(std::move(handler));
  // Not implemented.
  return Status::Unimplemented();
}

Result<CommandInterceptor> CommandMultiplexer::RegisterCommandInterceptor(
    [[maybe_unused]] pw::bluetooth::emboss::OpCode op_code,
    CommandHandler&& handler) {
  CommandHandler _(std::move(handler));
  // Not implemented.
  return Status::Unimplemented();
}

}  // namespace pw::bluetooth::proxy::hci
