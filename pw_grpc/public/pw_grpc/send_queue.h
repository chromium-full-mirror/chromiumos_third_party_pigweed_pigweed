// Copyright 2024 The Pigweed Authors
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

#include <optional>

#include "pw_allocator/allocator.h"
#include "pw_async/dispatcher.h"
#include "pw_async_basic/dispatcher.h"
#include "pw_containers/dynamic_deque.h"
#include "pw_multibuf/multibuf.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_thread/thread_core.h"

namespace pw::grpc {

// SendQueue is a queue+thread that serializes sending buffers to
// a blocking stream. The queue length is implicitly limited by the allocator
// that is creating the buffers to send.
class SendQueue : public thread::ThreadCore {
 public:
  SendQueue(stream::ReaderWriter& socket, Allocator& allocator)
      : socket_(socket),
        multibuf_send_task_(
            pw::bind_member<&SendQueue::ProcessMultibufQueue>(this)),
        send_task_(pw::bind_member<&SendQueue::ProcessSendQueue>(this)),
        queue_(allocator) {}

  // TODO(b/453996049): Remove after migration
  // Thread safe. Queues buffer to be sent on send thread.
  void QueueSend(multibuf::MultiBuf&& buffer) PW_LOCKS_EXCLUDED(send_mutex_);

  // Thread safe. Queues buffer to be sent on send thread. Returns false if
  // there was no queue space available from allocator.
  bool QueueSend(UniquePtr<std::byte[]>&& buffer)
      PW_LOCKS_EXCLUDED(send_mutex_);

  // ThreadCore impl.
  void Run() override { send_dispatcher_.Run(); }
  // Call before attempting to join thread.
  void RequestStop() { send_dispatcher_.RequestStop(); }

 private:
  // TODO(b/453996049): Remove after migration
  void ProcessMultibufQueue(async::Context& context, Status status)
      PW_LOCKS_EXCLUDED(send_mutex_);

  void ProcessSendQueue(async::Context& context, Status status)
      PW_LOCKS_EXCLUDED(send_mutex_);

  UniquePtr<std::byte[]> PopNext();

  stream::ReaderWriter& socket_;
  async::BasicDispatcher send_dispatcher_;
  async::Task multibuf_send_task_;
  async::Task send_task_;
  sync::Mutex send_mutex_;
  multibuf::MultiBuf buffer_to_write_ PW_GUARDED_BY(send_mutex_);
  DynamicDeque<UniquePtr<std::byte[]>> queue_ PW_GUARDED_BY(send_mutex_);
};

}  // namespace pw::grpc
