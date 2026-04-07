/*
 * Copyright 2026 The Pigweed Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

/* __logical_binary_start is defined in the pico-sdk linker script to
 * mark the start of the text section
 */
__pw_code_begin = __logical_binary_start;
__pw_code_end = __logical_binary_start + SIZEOF(.text);
