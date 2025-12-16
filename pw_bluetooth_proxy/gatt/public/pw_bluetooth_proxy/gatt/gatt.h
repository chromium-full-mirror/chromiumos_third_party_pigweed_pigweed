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

#pragma once

#include "pw_allocator/allocator.h"
#include "pw_bluetooth_proxy/channel_proxy.h"
#include "pw_bluetooth_proxy/connection_handle.h"
#include "pw_bluetooth_proxy/internal/multibuf.h"
#include "pw_bluetooth_proxy/l2cap_channel_manager_interface.h"
#include "pw_containers/dynamic_vector.h"
#include "pw_containers/intrusive_map.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy::gatt {

enum class AttributeHandle : uint16_t {};
enum class ClientId : uint16_t {};

/// New values may be added.
enum class Error : uint8_t {
  kDisconnection,
  kReset,
  kClosedByClient,
};

class Gatt;

/// Client represents the client role of a GATT connection to a remote device.
/// Many Clients can exist per connection.
///
/// This class is NOT thread-safe and requires external synchronization if used
/// by multiple threads.
class Client final {
 public:
  /// This delegate uses the NVI pattern.
  /// The `Delegate` must live until a call to `HandleError`. Closing the
  /// `Client` will send the error `kClosedByClient`.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    void HandleNotification(ConnectionHandle connection_handle,
                            AttributeHandle value_handle,
                            FlatConstMultiBuf&& value);

    void HandleError(Error error, ConnectionHandle connection_handle);

   private:
    virtual void DoHandleNotification(ConnectionHandle connection_handle,
                                      AttributeHandle value_handle,
                                      FlatConstMultiBuf&& value) = 0;

    /// Called when a fatal error occurs, invalidating this delegate and Client.
    /// The Client may be destroyed/closed inside of this function.
    /// @param error The GATT error that occurred.
    /// @param connection_handle The connection handle of the Client this
    /// delegate corresponds to.
    virtual void DoHandleError(Error error,
                               ConnectionHandle connection_handle) = 0;
  };

  Client();
  ~Client();
  Client(Client& other) = delete;
  Client& operator=(Client& other) = delete;
  Client(Client&& other);
  Client& operator=(Client&& other);

  /// Unregisters the client and delegate.
  /// Alternatively, the client may be destroyed.
  /// The delegate will be notified with a kClosedByClient error.
  void Close();

  /// Starts intercepting notifications for handle `value_handle`. Notifications
  /// will be delivered to Delegate::DoHandleNotification.
  ///
  /// @returns
  /// * @OK: The attribute is now being intercepted.
  /// * @NOT_FOUND: The connection or attribute does not exist or is invalid.
  /// * @ALREADY_EXISTS: The notification is already being intercepted.
  /// * @UNAVAILABLE: Allocation failed.
  Status InterceptNotification(AttributeHandle value_handle);

  /// Stop intercepting the notification identified by `value_handle`.
  ///
  /// @returns
  /// * @OK: The notification is no longer being intercepted.
  /// * @NOT_FOUND: The interception does not exist.
  Status CancelInterceptNotification(AttributeHandle value_handle);

 private:
  friend class Gatt;

  Client(ClientId client_id, ConnectionHandle connection_handle, Gatt& gatt);

  void Move(Client&& other);

  ClientId client_id_;
  ConnectionHandle connection_handle_;

  // nullptr when the client is closed, default constructed, or moved from.
  Gatt* gatt_ = nullptr;
};

/// The Gatt class multiplexes the ATT fixed channels for each connection. Gatt
/// provides an API for multiple clients to perform GATT operations.
/// This class is thread-safe.
class Gatt {
 public:
  /// @param l2cap The L2CAP interface to be used to obtain ATT channels. This
  /// is normally ProxyHost/L2capChannelManager, but a fake can be used for
  /// tests.
  /// @param allocator The allocator to use for internal state.
  /// @param multibuf_allocator The allocator to use for allocating buffers.
  Gatt(L2capChannelManagerInterface& l2cap,
       Allocator& allocator,
       MultiBufAllocator& multibuf_allocator);

  Gatt(const Gatt&) = delete;
  Gatt(Gatt&&) = delete;
  Gatt& operator=(const Gatt&) = delete;
  Gatt& operator=(Gatt&&) = delete;

  ~Gatt();

  /// Create a GATT Client object corresponding to a specific connection.
  /// @param connection_handle The handle of the connection the client should
  /// use.
  /// @param delegate The Delegate must be valid until Delegate::HandleError()
  /// is called or the Client is closed/destroyed.
  ///
  /// @returns
  /// * @OK: The attribute is now being intercepted.
  /// * @UNAVAILABLE: Allocation failed or the ATT channel could not be
  /// intercepted.
  Result<Client> CreateClient(ConnectionHandle connection_handle,
                              Client::Delegate& delegate);

 private:
  friend class Client;

  void UnregisterClient(ClientId client_id, ConnectionHandle connection_handle);

  Status InterceptNotification(ClientId client,
                               ConnectionHandle connection_handle,
                               AttributeHandle value_handle);

  Status CancelInterceptNotification(ClientId client,
                                     ConnectionHandle connection_handle,
                                     AttributeHandle value_handle);

  Result<UniquePtr<ChannelProxy>> InterceptAttChannel(
      ConnectionHandle connection_handle);

  bool OnSpanReceivedFromController(ConstByteSpan payload,
                                    uint16_t connection_handle,
                                    uint16_t channel_id);

  bool OnSpanReceivedFromHost(ConstByteSpan payload,
                              uint16_t connection_handle,
                              uint16_t channel_id);

  void OnL2capEvent(L2capChannelEvent event,
                    ConnectionHandle connection_handle);

  void OnChannelClosedEvent(ConnectionHandle connection_handle)
      PW_LOCKS_EXCLUDED(mutex_);

  void ResetConnections() PW_LOCKS_EXCLUDED(mutex_);

  struct ClientState final
      : IntrusiveMap<std::underlying_type_t<ClientId>, ClientState>::Pair {
    ClientState(ClientId client_id, Client::Delegate& client_delegate)
        : IntrusiveMap<std::underlying_type_t<ClientId>, ClientState>::Pair(
              cpp23::to_underlying(client_id)),
          delegate(client_delegate) {}

    Client::Delegate& delegate;
  };

  struct Connection final
      : IntrusiveMap<std::underlying_type_t<ConnectionHandle>,
                     Connection>::Pair {
    Connection(ConnectionHandle handle, Allocator& allocator)
        : IntrusiveMap<std::underlying_type_t<ConnectionHandle>,
                       Connection>::Pair(cpp23::to_underlying(handle)),
          intercepted_notifications_(allocator) {}

    UniquePtr<ChannelProxy> att_channel;
    DynamicVector<std::pair<AttributeHandle, ClientId>>
        intercepted_notifications_;
    // Entries are allocated with allocator_.
    IntrusiveMap<std::underlying_type_t<ClientId>, ClientState> clients;
  };

  L2capChannelManagerInterface& l2cap_;
  Allocator& allocator_;
  MultiBufAllocator& multibuf_allocator_;

  sync::Mutex mutex_;

  // Entries are allocated with allocator_.
  IntrusiveMap<std::underlying_type_t<ConnectionHandle>, Connection>
      connections_ PW_GUARDED_BY(mutex_);

  uint16_t next_client_id_ PW_GUARDED_BY(mutex_) = 0;
};

}  // namespace pw::bluetooth::proxy::gatt
