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
"""Helpers for expanding variables in package paths."""

load("//pw_env_setup/bazel/cipd/private/platforms:platforms.bzl", "get_cipd_arch", "get_cipd_os_name", "get_cipd_platform")

def _substitute_impl(path, mapping):
    original = path
    for _ in range(len(path)):
        completed, sep, remainder = path.partition("${")
        if not sep:
            # No more placeholders to resolve.
            return completed, None

        name, sep, remainder = remainder.partition("}")
        if not sep:
            return None, "Unterminated \"${{\" found in path: {}".format(original)

        name, sep, allowed = name.partition("=")
        if not sep:
            allowed = None

        value = mapping.get(name)
        if value == None:
            return None, "Unknown variable \"${{{}}}\" in path: {}".format(name, original)

        if not completed:
            return None, "Cannot start package path with \"${{{}}}\" in path: {}'".format(name, original)

        if allowed != None:
            allowed = [option.strip() for option in allowed.split(",")]
            if value not in allowed:
                return "", None

        path = completed + value + remainder

    # The for loop should have returned before ending.
    return None, "Unreachable code path"

def expand_vars(path, os):
    """Substitutes for placeholders in a package path.

    Implements the same subsitutions done by CIPD, as defined here:

    https://chromium.googlesource.com/infra/luci/luci-go/+/29e9a814821a9816b0489f9efe03cdb92ed8a6d1/cipd/client/cipd/ensure/doc.go#71

    Supported placeholders:

        ${platform} -> e.g., "linux-amd64"
        ${os} -> e.g., "linux"
        ${arch} -> e.g., "amd64"

    Restricted subtitutions (for advanced use-cases):

        ${var=value1,value2} -> expands ${var} but only if it matches one of the
        listed values. If there is no match, then an empty string is returned
        instead.

    Args:
        path: The package path string to resolve.
        os: A module_ctx.os structure.

    Returns:
        A tuple of (path, error), where the output path has had all its
        placeholders replaced. The path can be an empty string if a restriction
        was not satisifed.

        If there is an error, the returned path is None, and the error is a
        brief error message meant to be used in forming a contextual error
        message.

    """

    return _substitute_impl(path, {
        "arch": get_cipd_arch(os),
        "os": get_cipd_os_name(os),
        "platform": get_cipd_platform(os),
    })
