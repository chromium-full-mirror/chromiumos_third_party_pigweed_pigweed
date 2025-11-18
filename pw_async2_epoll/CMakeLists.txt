# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(pw_async2_epoll STATIC
  HEADERS
    public/pw_async2_epoll/dispatcher.h
  SOURCES
    dispatcher.cc
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_async2.dispatcher
    pw_async2.poll
    pw_status
  PRIVATE_DEPS
    pw_log
)

pw_add_library(pw_async2_epoll.dispatcher_for_test INTERFACE
  HEADERS
    dispatcher_for_test_public_overrides/pw_async2_backend/native_dispatcher_for_test.h
  PUBLIC_DEPS
    pw_async2_epoll
  PUBLIC_INCLUDES
    dispatcher_for_test_public_overrides
)
