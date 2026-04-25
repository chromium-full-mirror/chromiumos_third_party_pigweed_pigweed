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

// TODO: b/505754109 - This is a stub to satisfy upstream CHRE test
// dependencies. It should be removed once CHRE removes the dependency on
// android-base or when Pigweed provides a proper thread safety annotation
// abstraction.

#include <set>

#define GUARDED_BY(x)
#define REQUIRES(x)
#define ACQUIRE(x)
#define RELEASE(x)
#define EXCLUDES(x)
