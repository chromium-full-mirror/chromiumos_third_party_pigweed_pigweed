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

#include "pw_transfer/handler_allocator.h"

#include <array>
#include <cstddef>

#include "pw_allocator/best_fit.h"
#include "pw_allocator/block/small_block.h"
#include "pw_stream/null_stream.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_transfer/transfer.h"
#include "pw_unit_test/framework.h"

namespace pw::transfer {
namespace {

class TransferHandlerAllocatorTest : public ::testing::Test {
 protected:
  TransferHandlerAllocatorTest()
      : thread_(chunk_buffer_, encode_buffer_),
        system_thread_(system_thread_context_.options(), thread_),
        transfer_service_(thread_, 1024U),
        allocator_(memory_pool_),
        handler_allocator_(transfer_service_, allocator_) {}

  static constexpr size_t kMaxHandlers = 4;

  ~TransferHandlerAllocatorTest() override {
    thread_.Terminate();
    system_thread_.join();
  }

  std::array<std::byte, 1> chunk_buffer_;
  std::array<std::byte, 1> encode_buffer_;
  transfer::Thread<1, 1> thread_;
  pw::thread::test::TestThreadContext system_thread_context_;
  pw::Thread system_thread_;
  TransferService transfer_service_;
  std::array<std::byte,
             TransferHandlerAllocator::GetAllocatorSize(
                 kMaxHandlers, allocator::SmallBlock::kBlockOverhead)>
      memory_pool_;
  allocator::BestFitAllocator<allocator::SmallBlock> allocator_;
  TransferHandlerAllocator handler_allocator_;
};

TEST_F(TransferHandlerAllocatorTest, AllocateReader) {
  stream::NullStream reader;
  auto resource_id = handler_allocator_.AllocateReader(reader);
  PW_TEST_EXPECT_OK(resource_id);
}

TEST_F(TransferHandlerAllocatorTest, AllocateWriter) {
  stream::NullStream writer;
  auto resource_id = handler_allocator_.AllocateWriter(writer);
  PW_TEST_EXPECT_OK(resource_id);
}

TEST_F(TransferHandlerAllocatorTest, AllocateReadWriter) {
  stream::NullStream reader_writer;
  auto resource_id = handler_allocator_.AllocateReadWriter(reader_writer);
  PW_TEST_EXPECT_OK(resource_id);
}

TEST_F(TransferHandlerAllocatorTest, CloseInvalidId) {
  TransferResource invalid(handler_allocator_, 123);
}

TEST_F(TransferHandlerAllocatorTest, MultipleAllocationsAndCloses) {
  stream::NullStream reader1;
  stream::NullStream writer1;
  stream::NullStream reader_writer1;
  stream::NullStream reader2;

  auto id1 = handler_allocator_.AllocateReader(reader1);
  auto id2 = handler_allocator_.AllocateWriter(writer1);
  auto id3 = handler_allocator_.AllocateReadWriter(reader_writer1);
  auto id4 = handler_allocator_.AllocateReader(reader2);

  PW_TEST_ASSERT_OK(id1);
  PW_TEST_ASSERT_OK(id2);
  PW_TEST_ASSERT_OK(id3);
  PW_TEST_ASSERT_OK(id4);

  // Past kMaxHandlers should fail
  EXPECT_EQ(handler_allocator_.AllocateReader(reader1).status(),
            Status::ResourceExhausted());

  *id1 = {};

  // After clearing one, we can allocate one again.
  PW_TEST_ASSERT_OK(handler_allocator_.AllocateReader(reader1));
}

}  // namespace
}  // namespace pw::transfer
