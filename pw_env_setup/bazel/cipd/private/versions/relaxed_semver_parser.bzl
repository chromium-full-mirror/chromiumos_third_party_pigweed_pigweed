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

"""Helper functions for parsing relaxed semantic versions for CIPD."""

_DIGIT_CHARSET = set([x for x in "0123456789".elems()])
_ALPHANUM_CHARSET = _DIGIT_CHARSET | set([x for x in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ".elems()])
_RELEASE_PART_CHARSET = _ALPHANUM_CHARSET | set(["."])
_PRERELEASE_PART_CHARSET = _RELEASE_PART_CHARSET | set(["-"])
_BUILD_PART_CHARSET = _PRERELEASE_PART_CHARSET

def _matches_charset(s, charset):
    return len(s) > 0 and all([c in charset for c in s.elems()])

def _form_prerelease_tuples(s):
    result = []
    for part in s.split("."):
        if len(part) == 0:
            return None
        if _matches_charset(part, _DIGIT_CHARSET):
            if part[0] == "0" and len(part) > 1:
                return None
            result.append((0, int(part), part))
        else:
            result.append((1, 0, part))

    return tuple(result)

def _split_for_digits_and_non_digits(s):
    result = []

    mode = 0 if s[0].isdigit() else 1
    start = 0
    for i, c in enumerate(s.elems()):
        if mode == 0 and not c.isdigit():
            result.append((0, int(s[start:i]), s[start:i]))
            mode = 1
            start = i
        elif mode == 1 and c.isdigit():
            result.append((1, 0, s[start:i]))
            mode = 0
            start = i

    if mode == 0:
        result.append((0, int(s[start:]), s[start:]))
    else:
        result.append((1, 0, s[start:]))

    return tuple(result)

def _form_release_tuples(s):
    result = []
    for part in s.split("."):
        if len(part) == 0:
            return None
        result.append(_split_for_digits_and_non_digits(part))

    return tuple(result)

def parse(version):
    """Attempts to parse a CIPD version as a relaxed semantic version.

    The "relaxed" semver parse is based on the relaxed version parsing done by
    Bazel it self when resolving the dependencies introduced through
    `bazel_dep`, but for CIPD some additional leading prefixes are allowed.

    There are a few other differences between the Bazel implementation and this
    as we shoehorn the lexicographic comparison a bit to have it work when
    comparing tuples.

    1) To be parsed as a semver, the version must start with the prefix
       "version:".
    2) The next portion of the version is optional, but if present must match
       the regex /[0-9]+@/. This is the CIPD epoch. If not present, the epoch
       is considered to have the value zero.
    3) The next portion of the version is considered the "release" portion, and
       must match the regex /[0-9A-Za-z.]+/
    4) The next portion of the version is optional, and is considered to be the
       "prerelease" portion of the version. If present, it must match the regex
       /[0-9A-Za-z.-]+/.
    5) The final portion of the version is also optional, and is considered to
       be the "build" portion of the version. If present it begins with a '+',
       but allows the same characters as the prerelease portion.

    Once the version is successfully parsed into these four components, the
    "build" portion of the version is discarded, as it is not useful for
    comparing versions, which is ultimate goal of this function.

    The parser then splits the "release" portion of the version strings at every
    period character, yielding an array of substrings. Each substring is then
    further split into sections where there are contiguous digits, and sections
    where there are non-digits. A tuple of these sections is then created, where
    each section with digits is turned into `(0, int(digits), digits_str)`, and
    each non-digit string is turned into `(1, 0, non_digit_str)`.

    As an example, if the <release> string was "1.2a.3", this is turned into:

        `(((0, 1, "1"),), ((0, 2, "2"), (1, 0, "a")), ((0, 3, "3"),))`

    If the prerelease string is present, a similar but simpler split is done.
    Each substring after splitting at a period is converted into the tuple
    `(0, int(substring), substring)` if it is all digits, or the tuple
    `(1, 0, substring)` if it is not. The sequence of tuples after the
    period split becomes the release or prerelease component of the final
    tuple.

    As an example, if the <prerelease> string was "1.2a.3", this is turned into:

        `((0, 1, "1"), (1, 0, "2a"), (0, 3, "3"))`

    Finally the function returns one of:

      - `(epoch, release_tuple, 1)` # If there is no prerelease portion
      - `(epoch, release_tuple, 0, prerelease_tuple)` # otherwise

    This last detail ensures that a prerelease version (e.g. "1.0-rc1") is
    always considered to be less than a release version ("1.0").

    The net result of all these steps is that the output tuple can be compared
    with other tuples produced by this function using standard comparison
    operators, to properly versions.

    By way of example

    ```
    parse("1.0.0-rc1") -> (0, ((1, "1"), (0, "0"), (0, "0")), 0, ((0, "-rc1"),))
    parse("1.0.0")     -> (0, ((1, "1"), (0, "0"), (0, "0")), 1)
    parse("1.0.1-rc1") -> (0, ((1, "1"), (0, "0"), (1, "1")), 0, ((0, "-rc1"),))
    ```

    The order of those above is the same order the output tuples would appear
    if sorted.

    Args:
        version: The CIPD version string.

    Returns:
        A somewhat complex tuple representing the parsed version, but which is
        constructed in such a way that comparing tuples produced by this
        function against each other yields a correct version ordering.
    """

    remaining = version
    if not remaining.startswith("version:"):
        return None
    remaining = remaining.removeprefix("version:")

    # Parse the optional epoch
    epoch = 0
    left, mid, right = remaining.partition("@")
    if mid:
        remaining = right
        if not _matches_charset(left, _DIGIT_CHARSET):
            return None
        epoch = int(left)

    release = None
    prerelease = None
    build = None

    # Split off the build portion first (everything after the first '+')
    rel_and_pre, plus_sign, build = remaining.partition("+")

    # Split what remains at the first '-' to get release and prerelease
    release, hyphen, prerelease = rel_and_pre.partition("-")

    # Normalize empty strings back to None
    prerelease = prerelease if hyphen else None
    build = build if plus_sign else None

    # Ensure each portion is composed of the allowed characters.
    if not _matches_charset(release, _RELEASE_PART_CHARSET):
        return None
    if prerelease and not _matches_charset(prerelease, _PRERELEASE_PART_CHARSET):
        return None
    if build and not _matches_charset(build, _BUILD_PART_CHARSET):
        return None

    # Return the tuple representation of the version.
    if prerelease:
        release = _form_release_tuples(release)
        if release == None:
            return None
        prerelease = _form_prerelease_tuples(prerelease)
        if prerelease == None:
            return None
        return (epoch, release, 0, prerelease)

    release = _form_release_tuples(release)
    if release == None:
        return None
    return (epoch, release, 1)
