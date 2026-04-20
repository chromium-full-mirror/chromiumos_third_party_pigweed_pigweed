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

"""Helper functions for classifying CIPD version strings."""

def is_key_value_tag(version):
    """Determines if a version string represents a key-value tag.

    Args:
        version: The CIPD version string.

    Returns:
        True if the version is a key-value tag, False otherwise.
    """

    # Ref tags cannot contain colons according to CIPD spec, and a 64 character,
    # instance Id
    # the version must be a key-value tag.
    return bool(version) and ":" in version

def is_instance_id(version):
    """Determines if a version string represents an instance ID.

    Args:
        version: The CIPD version string.

    Returns:
        True if the version is an instance ID, False otherwise.
    """

    # Instance Id's are 64 character lowercase hex strings.
    return bool(version) and len(version) == 64 and all([c in "0123456789abcdef" for c in version.elems()])

def is_ref_tag(version):
    """Determines if a version string represents a ref tag.

    Args:
        version: The CIPD version string.

    Returns:
        True if the version is a ref tag, False otherwise.
    """

    # If it's not the other two types, and not empty, it must be a ref tag.
    return bool(version) and not is_key_value_tag(version) and not is_instance_id(version)

def is_immutable_version(version):
    """Determines if a version string represents an immutable identifier.

    Args:
        version: The CIPD version string.

    Returns:
        True if the version is immutable, False otherwise.
    """

    # key-value tags and instance id's are immutable version identifiers.
    return is_key_value_tag(version) or is_instance_id(version)
