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
"""Helper functions for resolving host platforms to CIPD naming schemas."""

_CIPD_OS_NAME_WINDOWS = "windows"
_CIPD_OS_NAME_LINUX = "linux"
_CIPD_OS_NAME_MAC = "mac"

_CIPD_ARCH_AMD64 = "amd64"
_CIPD_ARCH_ARM64 = "arm64"

def get_cipd_os_name(os):
    """Gets the CIPD compatible name for the host OS.

    Args:
        os: Struct containing name and arch (a module_ctx.os or
            repository_ctx.os).

    Returns:
        The CIPD compatible OS name, or None.
    """

    # The value of `os.name` in Bazel is obtained by taking the result of a
    # `System.getProperty("os.name")` call, and converting it to lower case.
    #
    # There is no formal list of possible return values, except those that have
    # been gathered over time and posted as responses to questions on Stack
    # Overflow and the like.
    #
    # This is not exhaustive, but covers the cases we care about.
    #
    # For Microsoft Windows, the string is typically something like
    # "windows xp", "windows 11".
    #
    # For Linux, the string is expected to always be "linux".
    #
    # For macOS (aka "OS X"), the string is expected to always be "mac os x".
    # and note this value is reportedly returned even for "darwin".

    if os.name.startswith("windows"):
        return _CIPD_OS_NAME_WINDOWS
    elif "linux" == os.name:
        return _CIPD_OS_NAME_LINUX
    elif "mac os x" == os.name:
        return _CIPD_OS_NAME_MAC
    return None

def get_cipd_arch(os):
    """Gets the CIPD compatible architecture name for the host OS.

    Args:
        os: Struct containing name and arch (a module_ctx.os or
            repository_ctx.os).

    Returns:
        The CIPD compatible architecture name, or None
    """

    # The value of `os.arch` in Bazel is obtained by taking the result of a
    # `System.getProperty("os.arch")` call, and converting it to lower case.
    #
    # inclusive-language: disable
    # As noted at
    # https://github.com/apache/commons-lang/blob/master/src/main/java/org/apache/commons/lang3/ArchUtils.java
    # inclusive-language: enable
    #
    # > The `os.arch` system property returns the architecture used by the JVM
    # > not of the operating system.
    #
    # As an improvement over os.name, that Java source file at least gives
    # examples of what values are expected.

    if os.arch == "aarch64":
        return _CIPD_ARCH_ARM64
    if os.arch in ("x86_64", "amd64"):
        return _CIPD_ARCH_AMD64
    return None

def get_cipd_platform(os):
    """Gets a CIPD compatible platform string for the host OS.

    Args:
        os: Struct containing name and arch (a module_ctx.os or
            repository_ctx.os).

    Returns:
        The CIPD compatible platform string (e.g., 'linux-amd64'), or None.
    """
    os_name = get_cipd_os_name(os)
    if not os_name:
        return None
    os_arch = get_cipd_arch(os)
    if not os_arch:
        return None
    return "{}-{}".format(os_name, os_arch)
