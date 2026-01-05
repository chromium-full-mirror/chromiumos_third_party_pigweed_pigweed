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

#include "pw_bluetooth_proxy/gatt/gatt.h"

#include <mutex>

#include "lib/stdcompat/utility.h"
#include "pw_assert/check.h"
#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_containers/algorithm.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy::gatt {

void Client::Delegate::HandleNotification(ConnectionHandle connection_handle,
                                          AttributeHandle value_handle,
                                          FlatConstMultiBuf&& value) {
  DoHandleNotification(connection_handle, value_handle, std::move(value));
}

void Client::Delegate::HandleError(Error error,
                                   ConnectionHandle connection_handle) {
  DoHandleError(error, connection_handle);
}

Client::Client()
    : client_id_(ClientId{0}),
      connection_handle_(ConnectionHandle{0}),
      gatt_(nullptr) {}

Client::~Client() { Close(); }

Client::Client(Client&& other) { Move(std::move(other)); }

Client& Client::operator=(Client&& other) {
  Move(std::move(other));
  return *this;
}

void Client::Move(Client&& other) {
  if (gatt_ != nullptr) {
    Close();
  }
  client_id_ = other.client_id_;
  connection_handle_ = other.connection_handle_;
  gatt_ = std::exchange(other.gatt_, nullptr);
}

Client::Client(ClientId client_id,
               ConnectionHandle connection_handle,
               Gatt& gatt)
    : client_id_(client_id),
      connection_handle_(connection_handle),
      gatt_(&gatt) {}

void Client::Close() {
  if (gatt_ == nullptr) {
    // Already closed
    return;
  }
  gatt_->UnregisterClient(client_id_, connection_handle_);
  gatt_ = nullptr;
}

Status Client::InterceptNotification(AttributeHandle value_handle) {
  return gatt_->InterceptNotification(
      client_id_, connection_handle_, value_handle);
}

Status Client::CancelInterceptNotification(AttributeHandle value_handle) {
  return gatt_->CancelInterceptNotification(
      client_id_, connection_handle_, value_handle);
}

Gatt::Gatt(L2capChannelManagerInterface& l2cap,
           Allocator& allocator,
           MultiBufAllocator& multibuf_allocator)
    : l2cap_(l2cap),
      allocator_(allocator),
      multibuf_allocator_(multibuf_allocator) {}

Gatt::~Gatt() { ResetConnections(); }

Result<Client> Gatt::CreateClient(ConnectionHandle connection_handle,
                                  Client::Delegate& delegate) {
  std::lock_guard lock(mutex_);

  if (next_client_id_ == std::numeric_limits<uint16_t>::max()) {
    return Status::ResourceExhausted();
  }

  ClientId client_id{next_client_id_++};
  UniquePtr<ClientState> client =
      allocator_.MakeUnique<ClientState>(client_id, delegate);
  if (client == nullptr) {
    return Status::Unavailable();
  }

  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    UniquePtr<Connection> connection =
        allocator_.MakeUnique<Connection>(connection_handle, allocator_);
    if (connection == nullptr) {
      return Status::Unavailable();
    }

    Result<UniquePtr<ChannelProxy>> channel_result =
        InterceptAttChannel(connection_handle);
    if (!channel_result.ok()) {
      return Status::Unavailable();
    }

    connection->att_channel = std::move(channel_result.value());
    auto [iter, inserted] = connections_.insert(*connection.Release());
    PW_CHECK(inserted);
    conn_iter = iter;
  }

  auto [_, inserted] = conn_iter->clients.insert(*client.Release());
  PW_CHECK(inserted);

  return Client(client_id, connection_handle, *this);
}

void Gatt::UnregisterClient(ClientId client_id,
                            ConnectionHandle connection_handle) {
  Client::Delegate* delegate = nullptr;
  {
    std::lock_guard lock(mutex_);
    auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
    if (conn_iter == connections_.end()) {
      return;
    }
    auto client_iter = conn_iter->clients.find(cpp23::to_underlying(client_id));
    if (client_iter == conn_iter->clients.end()) {
      return;
    }
    for (auto iter = conn_iter->intercepted_notifications_.begin();
         iter != conn_iter->intercepted_notifications_.end();) {
      if (iter->second == client_id) {
        iter = conn_iter->intercepted_notifications_.erase(iter);
        continue;
      }
      ++iter;
    }
    ClientState& client = *client_iter;
    delegate = &client.delegate;
    conn_iter->clients.erase(client_iter);
    allocator_.Delete(&client);
  }

  // Call outside of lock to avoid deadlock.
  delegate->HandleError(Error::kClosedByClient, connection_handle);

  // Leave connection/channel in connections_ map even if there are no clients
  // remaining.
}

Status Gatt::InterceptNotification(ClientId client,
                                   ConnectionHandle connection_handle,
                                   AttributeHandle value_handle) {
  std::lock_guard lock(mutex_);

  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return Status::NotFound();
  }

  auto notification_iter =
      pw::containers::Find(conn_iter->intercepted_notifications_,
                           std::make_pair(value_handle, client));
  if (notification_iter != conn_iter->intercepted_notifications_.end()) {
    return Status::AlreadyExists();
  }

  bool success = conn_iter->intercepted_notifications_.try_emplace_back(
      value_handle, client);

  if (!success) {
    return Status::Unavailable();
  }

  return OkStatus();
}

Status Gatt::CancelInterceptNotification(ClientId client,
                                         ConnectionHandle connection_handle,
                                         AttributeHandle value_handle) {
  std::lock_guard lock(mutex_);
  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return Status::NotFound();
  }

  auto notification_iter =
      pw::containers::Find(conn_iter->intercepted_notifications_,
                           std::make_pair(value_handle, client));
  if (notification_iter == conn_iter->intercepted_notifications_.end()) {
    return Status::NotFound();
  }

  conn_iter->intercepted_notifications_.erase(notification_iter);
  return OkStatus();
}

Result<UniquePtr<ChannelProxy>> Gatt::InterceptAttChannel(
    ConnectionHandle connection_handle) {
  return l2cap_.InterceptBasicModeChannel(
      connection_handle,
      static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL),
      static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_ATTRIBUTE_PROTOCOL),
      AclTransportType::kLe,
      pw::bind_member<&Gatt::OnSpanReceivedFromController>(this),
      pw::bind_member<&Gatt::OnSpanReceivedFromHost>(this),
      [this, connection_handle](L2capChannelEvent event) {
        OnL2capEvent(event, connection_handle);
      });
}

bool Gatt::OnSpanReceivedFromController(ConstByteSpan payload,
                                        ConnectionHandle connection_handle,
                                        uint16_t /*local_channel_id*/,
                                        uint16_t /*remote_channel_id*/) {
  std::lock_guard lock(mutex_);
  auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
  if (conn_iter == connections_.end()) {
    return false;
  }

  if (payload.size() < sizeof(emboss::AttOpcode)) {
    return false;
  }

  if (payload[0] != std::byte{static_cast<uint8_t>(
                        emboss::AttOpcode::ATT_HANDLE_VALUE_NTF)}) {
    return false;
  }

  const size_t attribute_size =
      payload.size() - emboss::AttHandleValueNtf::MinSizeInBytes();

  Result<emboss::AttHandleValueNtfView> view =
      MakeEmbossView<emboss::AttHandleValueNtfView>(
          attribute_size,
          reinterpret_cast<const uint8_t*>(payload.data()),
          payload.size());
  if (!view.ok()) {
    PW_LOG_WARN("Received invalid ATT_HANDLE_VALUE_NTF");
    return false;
  }

  PW_CHECK(view->attribute_opcode().Read() ==
           emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);

  AttributeHandle att_handle{view->attribute_handle().Read()};

  bool intercepted = false;
  for (auto& [intercepted_handle, client_id] :
       conn_iter->intercepted_notifications_) {
    if (att_handle != intercepted_handle) {
      continue;
    }
    intercepted = true;

    auto client_iter = conn_iter->clients.find(cpp23::to_underlying(client_id));
    PW_CHECK(client_iter != conn_iter->clients.end());

    std::optional<FlatMultiBufInstance> buffer =
        MultiBufAdapter::Create(multibuf_allocator_, attribute_size);
    if (!buffer.has_value()) {
      PW_LOG_WARN("Failed to allocate multibuf for attribute value");
      return true;
    }

    pw::span<const uint8_t> backing_storage(
        view->attribute_value().BackingStorage().data(),
        view->attribute_value().SizeInBytes());
    size_t bytes_copied = MultiBufAdapter::Copy(
        /*dst=*/buffer.value(),
        /*dst_offset=*/0,
        /*src=*/as_bytes(backing_storage));
    PW_CHECK_UINT_EQ(bytes_copied, attribute_size);

    client_iter->delegate.HandleNotification(
        ConnectionHandle{connection_handle},
        att_handle,
        std::move(MultiBufAdapter::Unwrap(buffer.value())));
  }

  return intercepted;
}

bool Gatt::OnSpanReceivedFromHost(ConstByteSpan /*payload*/,
                                  ConnectionHandle /*connection_handle*/,
                                  uint16_t /*local_channel_id*/,
                                  uint16_t /*remote_channel_id*/) {
  // Intercepting outbound ATT packets is not supported.
  return false;
}

void Gatt::OnL2capEvent(L2capChannelEvent event,
                        ConnectionHandle connection_handle) {
  if (event == L2capChannelEvent::kReset) {
    ResetConnections();
  } else if (event == L2capChannelEvent::kChannelClosedByOther) {
    OnChannelClosedEvent(connection_handle);
  }
}

void Gatt::OnChannelClosedEvent(ConnectionHandle connection_handle) {
  IntrusiveMap<std::underlying_type_t<ClientId>, ClientState> closing_clients;

  {
    std::lock_guard lock(mutex_);

    auto conn_iter = connections_.find(cpp23::to_underlying(connection_handle));
    if (conn_iter == connections_.end()) {
      return;
    }

    Connection& conn = *conn_iter;
    closing_clients.swap(conn.clients);
    connections_.erase(conn_iter);
    allocator_.Delete(&conn);
  }

  // Notify delegates outside of mutex to avoid deadlock.
  while (!closing_clients.empty()) {
    auto client_iter = closing_clients.begin();
    ClientState& client = *client_iter;
    client.delegate.HandleError(Error::kDisconnection, connection_handle);
    closing_clients.erase(client_iter);
    allocator_.Delete(&client);
  }
}

void Gatt::ResetConnections() {
  IntrusiveMap<std::underlying_type_t<ConnectionHandle>, Connection>
      closed_connections;

  {
    std::lock_guard lock(mutex_);
    closed_connections.swap(connections_);
  }

  // Notify delegates outside of mutex to avoid deadlock.
  while (!closed_connections.empty()) {
    auto conn_iter = closed_connections.begin();
    Connection& conn = *conn_iter;

    while (!conn.clients.empty()) {
      auto client_iter = conn.clients.begin();
      ClientState& client = *client_iter;
      client.delegate.HandleError(Error::kReset, ConnectionHandle{conn.key()});
      conn.clients.erase(client_iter);
      allocator_.Delete(&client);
    }

    closed_connections.erase(conn_iter);
    allocator_.Delete(&conn);
  }
}

}  // namespace pw::bluetooth::proxy::gatt
