#!/usr/bin/env python3
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
"""Introspects bazel modules."""

from functools import cache
from pathlib import Path
import json

from pw_build.bazel_exec import bazel
from pw_build.bazel_info import workspace_root
from pw_build.bazel_label import BazelLabel


@cache
def external_repo_canonical_name(name: str | BazelLabel) -> str:
    """
    Returns the canonical name of the given repository.

    Args:
        name: The name of the repository (e.g. "@pw_bazel_test_external").
              May also be a BazelLabel, in which case the repo name will be
              extracted from the label.

    Returns:
        The canonical name of the repository (e.g. "@pw_bazel_test_external").
    """
    if isinstance(name, BazelLabel):
        repo_name = name.repo_name()
    else:
        if not name.startswith('@'):
            raise ValueError(f'Invalid repository name: {name}')
        repo_name = name
    result = bazel(
        'mod',
        'show_repo',
        repo_name,
        '--output=streamed_jsonproto',
        cwd=workspace_root(),
    )
    repo_info = json.loads(result)
    return repo_info['canonicalName']


@cache
def external_repo_path(name: str | BazelLabel) -> Path:
    """
    Returns the path to the target external repository.

    Args:
        name: The name of the external repository (e.g.
              "@pw_bazel_test_external"). May also be a BazelLabel, in which
              case the repo name will be extracted from the label.

    Returns:
        The path to the external repository containing the given target relative
        to the output_base (e.g. "external/pw_bazel_test_external").

        Because of how bazel structures the output directories, the path
        returned will also be relative to the bazel-bin directory.
    """
    canonical_name = external_repo_canonical_name(name)
    return Path('external/' + canonical_name)
