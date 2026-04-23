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

load("@rules_cc//cc/toolchains:tool.bzl", "cc_tool")
load("@rules_cc//cc/toolchains:tool_map.bzl", "cc_tool_map")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(glob(["llvm/bin/**"]))

cc_tool_map(
    name = "arm_tools",
    tools = {
        "@pigweed//pw_toolchain/action:gcov": ":llvm-cov",
        "@pigweed//pw_toolchain/action:nm": ":llvm-nm",
        "@pigweed//pw_toolchain/action:objdump_disassemble": ":llvm-objdump",
        "@pigweed//pw_toolchain/action:readelf": ":llvm-readelf",
        "@pigweed//pw_toolchain/action:size": ":llvm-size",
        "@rules_cc//cc/toolchains/actions:ar_actions": ":llvm-ar",
        "@rules_cc//cc/toolchains/actions:assembly_actions": ":clang_asm",
        "@rules_cc//cc/toolchains/actions:c_compile_actions": ":clang",
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions": ":clang++",
        "@rules_cc//cc/toolchains/actions:link_actions": ":clang++_link",
        "@rules_cc//cc/toolchains/actions:objcopy_embed_data": ":llvm-objcopy",
        "@rules_cc//cc/toolchains/actions:strip": ":llvm-strip",
    },
)

###############################################################################
# LLVM Tools
###############################################################################

cc_tool(
    name = "llvm-ar",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-ar.exe",
        "//conditions:default": "//:llvm/bin/llvm-ar",
    }),
)

cc_tool(
    name = "clang++",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/clang++.exe",
        "//conditions:default": "//:llvm/bin/clang++",
    }),
    data = glob([
        "llvm/bin/**",
        "llvm/include/**",
        "llvm/lib/**",
    ]),
)

# TODO: https://github.com/bazelbuild/rules_cc/issues/235 - Workaround until
# Bazel has a more robust way to implement `cc_tool_map`.
alias(
    name = "clang_asm",
    actual = ":clang",
)

cc_tool(
    name = "clang",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/clang.exe",
        "//conditions:default": "//:llvm/bin/clang",
    }),
    data = glob([
        "llvm/bin/**",
        "llvm/include/**",
        "llvm/lib/**",
    ]),
)

# This tool is actually just clang++ under the hood, we just enumerate this
# tool differently to specify a different set of additional files if needed.
cc_tool(
    name = "clang++_link",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/clang++.exe",
        "//conditions:default": "//:llvm/bin/clang++",
    }),
    data = glob([
        "llvm/bin/**",
        "llvm/include/**",
        "llvm/lib/**",
    ]),
)

cc_tool(
    name = "llvm-cov",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-cov.exe",
        "//conditions:default": "//:llvm/bin/llvm-cov",
    }),
)

cc_tool(
    name = "llvm-objcopy",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-objcopy.exe",
        "//conditions:default": "//:llvm/bin/llvm-objcopy",
    }),
)

cc_tool(
    name = "llvm-objdump",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-objdump.exe",
        "//conditions:default": "//:llvm/bin/llvm-objdump",
    }),
)

cc_tool(
    name = "llvm-strip",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-strip.exe",
        "//conditions:default": "//:llvm/bin/llvm-strip",
    }),
)

cc_tool(
    name = "llvm-nm",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-nm.exe",
        "//conditions:default": "//:llvm/bin/llvm-nm",
    }),
)

cc_tool(
    name = "llvm-readelf",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-readelf.exe",
        "//conditions:default": "//:llvm/bin/llvm-readelf",
    }),
)

cc_tool(
    name = "llvm-size",
    src = select({
        "@platforms//os:windows": "//:llvm/bin/llvm-size.exe",
        "//conditions:default": "//:llvm/bin/llvm-size",
    }),
)