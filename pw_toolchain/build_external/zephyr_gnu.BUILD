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

load("@rules_cc//cc/toolchains:tool.bzl", "cc_tool")
load("@rules_cc//cc/toolchains:tool_map.bzl", "cc_tool_map")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

_GNU_PREFIX_EXISTS = len(glob(["gnu/arm-zephyr-eabi/bin/*"], allow_empty = True)) > 0

_ARM_PREFIX = "gnu/arm-zephyr-eabi" if _GNU_PREFIX_EXISTS else "arm-zephyr-eabi"
_X86_64_PREFIX = "gnu/x86_64-zephyr-elf" if _GNU_PREFIX_EXISTS else "x86_64-zephyr-elf"

exports_files(glob([
    "{}/bin/**".format(_ARM_PREFIX),
    "{}/bin/**".format(_X86_64_PREFIX),
]))

cc_tool_map(
    name = "arm_tools",
    tools = {
        "@pigweed//pw_toolchain/action:gcov": ":arm-zephyr-eabi-gcov",
        "@pigweed//pw_toolchain/action:nm": ":arm-zephyr-eabi-nm",
        "@pigweed//pw_toolchain/action:objdump_disassemble": ":arm-zephyr-eabi-objdump",
        "@pigweed//pw_toolchain/action:readelf": ":arm-zephyr-eabi-readelf",
        "@pigweed//pw_toolchain/action:size": ":arm-zephyr-eabi-size",
        "@rules_cc//cc/toolchains/actions:ar_actions": ":arm-zephyr-eabi-ar",
        "@rules_cc//cc/toolchains/actions:assembly_actions": ":arm-zephyr-eabi-asm",
        "@rules_cc//cc/toolchains/actions:c_compile_actions": ":arm-zephyr-eabi-gcc",
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions": ":arm-zephyr-eabi-g++",
        "@rules_cc//cc/toolchains/actions:link_actions": ":arm-zephyr-eabi-ld",
        "@rules_cc//cc/toolchains/actions:objcopy_embed_data": ":arm-zephyr-eabi-objcopy",
    },
)

cc_tool_map(
    name = "x86_64_tools",
    tools = {
        "@pigweed//pw_toolchain/action:gcov": ":x86_64-zephyr-elf-gcov",
        "@pigweed//pw_toolchain/action:nm": ":x86_64-zephyr-elf-nm",
        "@pigweed//pw_toolchain/action:objdump_disassemble": ":x86_64-zephyr-elf-objdump",
        "@pigweed//pw_toolchain/action:readelf": ":x86_64-zephyr-elf-readelf",
        "@pigweed//pw_toolchain/action:size": ":x86_64-zephyr-elf-size",
        "@rules_cc//cc/toolchains/actions:ar_actions": ":x86_64-zephyr-elf-ar",
        "@rules_cc//cc/toolchains/actions:assembly_actions": ":x86_64-zephyr-elf-asm",
        "@rules_cc//cc/toolchains/actions:c_compile_actions": ":x86_64-zephyr-elf-gcc",
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions": ":x86_64-zephyr-elf-g++",
        "@rules_cc//cc/toolchains/actions:link_actions": ":x86_64-zephyr-elf-ld",
        "@rules_cc//cc/toolchains/actions:objcopy_embed_data": ":x86_64-zephyr-elf-objcopy",
    },
)

###############################################################################
# ARM
###############################################################################

cc_tool(
    name = "arm-zephyr-eabi-ar",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-ar.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-ar".format(_ARM_PREFIX),
    }),
)

cc_tool(
    name = "arm-zephyr-eabi-g++",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-g++.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-g++".format(_ARM_PREFIX),
    }),
    data = glob([
        "{}/**".format(_ARM_PREFIX),
        # TODO: https://pwbug.dev/380001331 - Figure out which exact files are needed
        # "{}/**/*.specs".format(_ARM_PREFIX),
        # "{}/{}/sys-include/**".format(_ARM_PREFIX, _ARM_PREFIX),
        # "{}/{}/include/**".format(_ARM_PREFIX, _ARM_PREFIX),
        # "{}/lib/*".format(_ARM_PREFIX),
        # "{}/lib/gcc/{}/*/include/**".format(_ARM_PREFIX, _ARM_PREFIX),
        # "{}/lib/gcc/{}/*/include-fixed/**".format(_ARM_PREFIX, _ARM_PREFIX),
        # "{}/libexec/**".format(_ARM_PREFIX),
    ]),
)

# TODO: https://github.com/bazelbuild/rules_cc/issues/235 - Workaround until
# Bazel has a more robust way to implement `cc_tool_map`.
alias(
    name = "arm-zephyr-eabi-asm",
    actual = ":arm-zephyr-eabi-gcc",
)

cc_tool(
    name = "arm-zephyr-eabi-gcc",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-gcc.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-gcc".format(_ARM_PREFIX),
    }),
    data = glob([
        "{}/**".format(_ARM_PREFIX),
        # TODO: https://pwbug.dev/380001331 - Figure out which exact files are needed
        # "arm-zephyr-eabi/**/*.specs",
        # "arm-zephyr-eabi/picolibc/**",
        # "arm-zephyr-eabi/include/c++/**",
        # "arm-zephyr-eabi/arm-zephyr-eabi/sys-include/**",
        # "arm-zephyr-eabi/lib/*",
        # "arm-zephyr-eabi/lib/gcc/arm-zephyr-eabi/*/include/**",
        # "arm-zephyr-eabi/lib/gcc/arm-zephyr-eabi/*/include-fixed/**",
        # "arm-zephyr-eabi/libexec/**",
    ]),  #+
    # The assembler needs to be explicilty added. Note that the path is
    # intentionally different here as `as` is called from arm-zephyr-eabi-gcc.
    # `arm-zephyr-eabi-as` will not suffice for this context.
    # select({
    #     "@platforms//os:windows": ["//:arm-zephyr-eabi/bin/arm-zephyr-eabi-as.exe"],
    #     "//conditions:default": ["//:arm-zephyr-eabi/bin/arm-zephyr-eabi-as"],
    # }),
)

# This tool is actually just g++ under the hood, we just enumerate this
# tool differently to specify a different set of additional files.
cc_tool(
    name = "arm-zephyr-eabi-ld",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-g++.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-g++".format(_ARM_PREFIX),
    }),
    data = glob([
        "{}/**/*.a".format(_ARM_PREFIX),
        "{}/**/*.ld".format(_ARM_PREFIX),
        "{}/**/*.o".format(_ARM_PREFIX),
        "{}/**/*.specs".format(_ARM_PREFIX),
        "{}/**/*.so".format(_ARM_PREFIX),
        "{}/libexec/**".format(_ARM_PREFIX),
    ]) + [
        "//:{}/bin/arm-zephyr-eabi-ld".format(_ARM_PREFIX),
    ],
)

cc_tool(
    name = "arm-zephyr-eabi-gcov",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-gcov.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-gcov".format(_ARM_PREFIX),
    }),
)

cc_tool(
    name = "arm-zephyr-eabi-objcopy",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-objcopy.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-objcopy".format(_ARM_PREFIX),
    }),
)

cc_tool(
    name = "arm-zephyr-eabi-objdump",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-objdump.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-objdump".format(_ARM_PREFIX),
    }),
)

cc_tool(
    name = "arm-zephyr-eabi-strip",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-strip.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-strip".format(_ARM_PREFIX),
    }),
)

cc_tool(
    name = "arm-zephyr-eabi-nm",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-nm.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-nm".format(_ARM_PREFIX),
    }),
)

cc_tool(
    name = "arm-zephyr-eabi-readelf",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-readelf.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-readelf".format(_ARM_PREFIX),
    }),
)

cc_tool(
    name = "arm-zephyr-eabi-size",
    src = select({
        "@platforms//os:windows": "//:{}/bin/arm-zephyr-eabi-size.exe".format(_ARM_PREFIX),
        "//conditions:default": "//:{}/bin/arm-zephyr-eabi-size".format(_ARM_PREFIX),
    }),
)

###############################################################################
# x86_64
###############################################################################

cc_tool(
    name = "x86_64-zephyr-elf-ar",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-ar.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-ar".format(_X86_64_PREFIX),
    }),
)

cc_tool(
    name = "x86_64-zephyr-elf-g++",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-g++.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-g++".format(_X86_64_PREFIX),
    }),
    data = glob([
        "{}/**/*.specs".format(_X86_64_PREFIX),
        "{}/lib/gcc/x86_64-zephyr-elf/*/include/**".format(_X86_64_PREFIX),
        "{}/lib/gcc/x86_64-zephyr-elf/*/include-fixed/**".format(_X86_64_PREFIX),
        "{}/libexec/**".format(_X86_64_PREFIX),
    ]),
)

# TODO: https://github.com/bazelbuild/rules_cc/issues/235 - Workaround until
# Bazel has a more robust way to implement `cc_tool_map`.
alias(
    name = "x86_64-zephyr-elf-asm",
    actual = ":x86_64-zephyr-elf-gcc",
)

cc_tool(
    name = "x86_64-zephyr-elf-gcc",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-gcc.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-gcc".format(_X86_64_PREFIX),
    }),
    data = glob([
        "{}/**/*.specs".format(_X86_64_PREFIX),
        "{}/lib/gcc/x86_64-zephyr-elf/*/include/**".format(_X86_64_PREFIX),
        "{}/lib/gcc/x86_64-zephyr-elf/*/include-fixed/**".format(_X86_64_PREFIX),
        "{}/libexec/**".format(_X86_64_PREFIX),
    ]) +
    # The assembler needs to be explicilty added. Note that the path is
    # intentionally different here as `as` is called from x86_64-zephyr-elf-gcc.
    # `x86_64-zephyr-elf-as` will not suffice for this context.
    select({
        "@platforms//os:windows": ["//:{}/bin/as.exe".format(_X86_64_PREFIX)],
        "//conditions:default": ["//:{}/bin/as".format(_X86_64_PREFIX)],
    }),
)

# This tool is actually just g++ under the hood, we just enumerate this
# tool differently to specify a different set of additional files.
cc_tool(
    name = "x86_64-zephyr-elf-ld",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-g++.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-g++".format(_X86_64_PREFIX),
    }),
    data = glob([
        "{}/**/*.a".format(_X86_64_PREFIX),
        "{}/**/*.ld".format(_X86_64_PREFIX),
        "{}/**/*.o".format(_X86_64_PREFIX),
        "{}/**/*.specs".format(_X86_64_PREFIX),
        "{}/**/*.so".format(_X86_64_PREFIX),
        "{}/libexec/**".format(_X86_64_PREFIX),
    ]) + [
        "//:{}/bin/ld".format(_X86_64_PREFIX),
    ],
)

cc_tool(
    name = "x86_64-zephyr-elf-gcov",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-gcov.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-gcov".format(_X86_64_PREFIX),
    }),
)

cc_tool(
    name = "x86_64-zephyr-elf-objcopy",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-objcopy.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-objcopy".format(_X86_64_PREFIX),
    }),
)

cc_tool(
    name = "x86_64-zephyr-elf-objdump",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-objdump.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-objdump".format(_X86_64_PREFIX),
    }),
)

cc_tool(
    name = "x86_64-zephyr-elf-strip",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-strip.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-strip".format(_X86_64_PREFIX),
    }),
)

cc_tool(
    name = "x86_64-zephyr-elf-nm",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-nm.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-nm".format(_X86_64_PREFIX),
    }),
)

cc_tool(
    name = "x86_64-zephyr-elf-readelf",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-readelf.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-readelf".format(_X86_64_PREFIX),
    }),
)

cc_tool(
    name = "x86_64-zephyr-elf-size",
    src = select({
        "@platforms//os:windows": "//:{}/bin/x86_64-zephyr-elf-size.exe".format(_X86_64_PREFIX),
        "//conditions:default": "//:{}/bin/x86_64-zephyr-elf-size".format(_X86_64_PREFIX),
    }),
)
