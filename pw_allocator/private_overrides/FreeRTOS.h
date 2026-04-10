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
#pragma once

// This file defines the symbols used by FreeRtosAllocator and is a stub for an
// actual FreeRTOS installation. It is only meant to be used for unit testing.

#include <cstddef>

#define portBYTE_ALIGNMENT 8

void* pvPortMalloc(size_t xWantedSize);

void vPortFree(void* pv);
