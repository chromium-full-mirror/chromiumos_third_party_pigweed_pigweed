# Copyright 2025 The Pigweed Authors
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
"""Rules to expose active tools from C/C++ toolchains."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")

# This is easy to misuse. Don't expose publicly for now.
visibility("private")

def _pw_cc_tool_for_action_impl(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)

    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )

    tool_path = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = ctx.attr.action_name,
    )
    tool_file = None
    for file in cc_toolchain.all_files.to_list():
        if file.path == tool_path:
            tool_file = file
    if not tool_file:
        fail("Failed to find", tool_path, "in cc_toolchain.all_files")
    executable_tool_file = ctx.actions.declare_file(tool_file.basename)
    ctx.actions.symlink(
        output = executable_tool_file,
        target_file = tool_file,
        is_executable = True,
    )
    return DefaultInfo(
        executable = executable_tool_file,
        files = depset(
            direct = [executable_tool_file],
            transitive = [cc_toolchain.all_files],
        ),
        runfiles = ctx.runfiles(
            files = [executable_tool_file],
            transitive_files = cc_toolchain.all_files,
        ),
    )

pw_cc_tool_for_action = rule(
    implementation = _pw_cc_tool_for_action_impl,
    attrs = {
        "action_name": attr.string(mandatory = True),
    },
    doc = """Exposes the C/C++ toolchain tool for the specified action as a runnable binary.

This is intended exclusively to create `bazel run`-able targets for interactive
or debugging purposes.

WARNING: This should never be used as a dependency of another rule, as it
interacts with target/exec configurations in confusing ways. If used behind
`cfg="target"`, a rule might get a tool that is not `exec_compatible_with` the
executor of the consuming rule. If used behind `cfg="exec"`, a rule will always
get the version of the tool needed to build binaries for the exec platform of
the executor of the consuming rule (i.e. you'll never get an MCU-targeted
toolchain).

These interactions are less catastrophic when only a single execution platform
is registered, but still confusing.
""",
    executable = True,
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
)
