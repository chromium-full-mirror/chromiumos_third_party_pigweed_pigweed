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

"""Helpers for parsing CIPD digest files."""

def get_digest(digest_file_name, digest_file_content, platform):
    """Locates the client binary sha256 hash digest from the digest file for the desired platform.

    Args:
        digest_file_name: The name of the digest file for any error message
        digest_file_content: The contents of the CIPD digest file.
        platform: The platform string for the current host.

    Returns:
        The client binary sha256 digest, or None if it could not be found.
    """
    digest_platform_prefix = platform + " "
    digest_sha256_prefix = "sha256 "

    for line in digest_file_content.splitlines():
        if not line.startswith(digest_platform_prefix):
            continue
        line = line.removeprefix(digest_platform_prefix).lstrip()
        if not line.startswith(digest_sha256_prefix):
            return None, (("The digest file {} entry for {} is not a sha256 digest. Bazel " +
                           "requires a sha256 digest for downloads.").format(
                digest_file_name,
                platform,
            ))
        digest = line.removeprefix(digest_sha256_prefix).strip()
        if len(digest) != 64:
            return None, (("The digest file {} entry for {} has an invalid sha256 digest. Found " +
                           "a digest with length {} when expecting 64.").format(
                digest_file_name,
                platform,
                len(digest),
            ))
        return digest, None

    return None, "The digest file {} has no entry for {}.".format(digest_file_name, platform)
