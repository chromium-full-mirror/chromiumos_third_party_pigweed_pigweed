// Copyright 2026 The Pigweed Authors
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

#include <cstdint>
#include <cstring>

#include "pw_allocator_zephyr/synchronized_heap_allocator.h"
#include "pw_allocator_zephyr/system_heap_allocator.h"
#include "pw_unit_test/framework.h"
#include "zephyr/kernel.h"

namespace {

// Define a size for our local heap testing
constexpr size_t kLocalHeapSize = 2048;

template <typename AllocatorType>
void TestAllocateAndFree(AllocatorType& allocator, size_t size) {
  void* ptr = allocator.Allocate(pw::allocator::Layout(size, 4));
  EXPECT_NE(ptr, nullptr);

  if (ptr != nullptr) {
    std::memset(ptr, 0xAA, size);
    allocator.Deallocate(ptr);
  }
}

template <typename AllocatorType>
void TestReallocate(AllocatorType& allocator) {
  void* ptr = allocator.Allocate(pw::allocator::Layout(64, 4));
  EXPECT_NE(ptr, nullptr);

  if (ptr != nullptr) {
    std::memset(ptr, 0xBB, 64);

    void* ptr2 = allocator.Reallocate(ptr, pw::allocator::Layout(128, 4));
    EXPECT_NE(ptr2, nullptr);

    if (ptr2 != nullptr) {
      allocator.Deallocate(ptr2);
    }
  }
}

template <typename AllocatorType>
void TestExhaustion(AllocatorType& allocator,
                    size_t max_allocations,
                    size_t alloc_size) {
  void* ptrs[32] = {nullptr};
  size_t count = 0;

  while (count < max_allocations) {
    void* ptr = allocator.Allocate(pw::allocator::Layout(alloc_size, 4));
    if (ptr == nullptr) {
      break;
    }
    ptrs[count++] = ptr;
  }
  EXPECT_TRUE(count > 0);
  EXPECT_TRUE(count < max_allocations);

  for (size_t i = 0; i < count; ++i) {
    allocator.Deallocate(ptrs[i]);
  }
}

template <typename AllocatorType>
void TestReallocateExhaustion(AllocatorType& allocator,
                              size_t initial_size,
                              size_t increment) {
  void* ptr = allocator.Allocate(pw::allocator::Layout(initial_size, 4));
  EXPECT_NE(ptr, nullptr);

  if (ptr != nullptr) {
    void* last_ptr = ptr;
    size_t size = initial_size;
    while (true) {
      size += increment;
      void* new_ptr =
          allocator.Reallocate(last_ptr, pw::allocator::Layout(size, 4));
      if (new_ptr == nullptr) {
        break;
      }
      last_ptr = new_ptr;
    }
    allocator.Deallocate(last_ptr);
  }
}

template <typename AllocatorType>
void TestAlignment(AllocatorType& allocator, size_t size) {
  size_t alignments[] = {4, 8, 16};
  for (size_t align : alignments) {
    void* ptr = allocator.Allocate(pw::allocator::Layout(size, align));
    EXPECT_NE(ptr, nullptr);
    if (ptr != nullptr) {
      EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % align, 0U);
      allocator.Deallocate(ptr);
    }
  }
}

struct SynchronizedHeapAllocatorTests : public ::testing::Test {
  alignas(struct sys_heap*) char buf_[kLocalHeapSize];
  pw::allocator_zephyr::SynchronizedHeapAllocator allocator_{
      pw::as_writable_bytes(pw::span(buf_, sizeof(buf_)))};
};

TEST_F(SynchronizedHeapAllocatorTests, SynchronizedHeapAllocateAndFree) {
  TestAllocateAndFree(allocator_, 128);
}

TEST_F(SynchronizedHeapAllocatorTests, SynchronizedHeapReallocate) {
  TestReallocate(allocator_);
}

TEST_F(SynchronizedHeapAllocatorTests, SynchronizedHeapAlignment) {
  TestAlignment(allocator_, 64);
}

TEST_F(SynchronizedHeapAllocatorTests, SynchronizedHeapExhaustion) {
  TestExhaustion(allocator_, 32, 256);
}

TEST_F(SynchronizedHeapAllocatorTests, SynchronizedHeapReallocateExhaustion) {
  TestReallocateExhaustion(allocator_, 128, 128);
}

struct SystemHeapAllocatorTests : public ::testing::Test {
  pw::allocator_zephyr::SystemHeapAllocator& allocator_ =
      pw::allocator_zephyr::GetSystemHeapAllocator();
};

TEST_F(SystemHeapAllocatorTests, SystemHeapAllocateAndFree) {
  TestAllocateAndFree(allocator_, 256);
}

TEST_F(SystemHeapAllocatorTests, SystemHeapReallocate) {
  TestReallocate(allocator_);
}

TEST_F(SystemHeapAllocatorTests, SystemHeapAlignment) {
  TestAlignment(allocator_, 64);
}

TEST_F(SystemHeapAllocatorTests, SystemHeapExhaustion) {
  TestExhaustion(allocator_, 128, 128);
}

TEST_F(SystemHeapAllocatorTests, SystemHeapReallocateExhaustion) {
  TestReallocateExhaustion(allocator_, 128, 128);
}

}  // namespace
