#!/bin/bash
# Copyright 2026 The Pigweed Authors
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

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <cc_out> <h_out>"
  exit 1
fi

CC_OUT="$1"
H_OUT="$2"

# Use basename for the include to make it local
H_BASE=$(basename "$H_OUT")

echo "Writing to $CC_OUT"
cat << EOF > "$CC_OUT"
#include "$H_BASE"

int generated_func() {
  return 42;
}
EOF

echo "Writing to $H_OUT"
cat << 'EOF' > "$H_OUT"
#pragma once

int generated_func();
EOF
