#!/usr/bin/env python3
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
"""
Provides a python API for running common bazel cquery commands.
"""

from functools import cache
from pathlib import Path
import json

from pw_build.bazel_exec import bazel
from pw_build.bazel_info import workspace_root
from pw_build.bazel_label import BazelLabel
from pw_build.bazel_mod import external_repo_path


def source_files(
    target: BazelLabel,
    platform: str | None = None,
    compilation_mode: str | None = None,
    cpu: str | None = None,
    depth: int | None = None,
    no_implicit_deps: bool = True,
    flags: list[str] | None = None,
) -> list[Path]:
    """
    Get the source files for a given target and its dependencies.

    Args:
        target: The target to get the source files for. Must be a bazel label.
        platform: The platform to use for the query. Inserted directly into
            `--platforms=<platform>`.
        compilation_mode: The compilation mode to use for the query. Inserted
            directly into `--compilation_mode=<compilation_mode>`.
        cpu: The cpu to use for the query. Inserted directly into
            `--cpu=<cpu>`.
        flags: Additional flags to use for the query. Inserted directly into
            the command.

    Returns:
        A list of source files (as strings) for the given target and its
        dependencies. All paths are relative to the bazel-bin directory (i.e.
        `info.bazel_bin()`). External repository paths are also relative to
        the output base (i.e. `info.output_base()`).
    """
    return _query_kind(
        "source file",
        target,
        platform,
        compilation_mode,
        cpu,
        depth,
        no_implicit_deps,
        ' '.join(flags) if flags else None,
    )


def generated_files(
    target: BazelLabel,
    platform: str | None = None,
    compilation_mode: str | None = None,
    cpu: str | None = None,
    depth: int | None = None,
    no_implicit_deps: bool = True,
    flags: list[str] | None = None,
) -> list[Path]:
    """
    Get the generated files for a given target and its dependencies.

    Args:
        target: The target to get the generated files for. Must be a bazel
            label.
        platform: The platform to use for the query. Inserted directly into
            `--platforms=<platform>`.
        compilation_mode: The compilation mode to use for the query. Inserted
            directly into `--compilation_mode=<compilation_mode>`.
        cpu: The cpu to use for the query. Inserted directly into
            `--cpu=<cpu>`.
        flags: Additional flags to use for the query. Inserted directly into
            the command.

    Returns:
        A list of generated files (as strings) for the given target and its
        dependencies. All paths are relative to the bazel-bin directory (i.e.
        `info.bazel_bin()`). External repository paths are also relative to
        the output base (i.e. `info.output_base()`).
    """
    return _query_kind(
        "generated file",
        target,
        platform,
        compilation_mode,
        cpu,
        depth,
        no_implicit_deps,
        ' '.join(flags) if flags else None,
    )


@cache
def _query_kind(
    kind: str,
    target: BazelLabel,
    platform: str | None = None,
    compilation_mode: str | None = None,
    cpu: str | None = None,
    depth: int | None = None,
    no_implicit_deps: bool = True,
    flags: str | None = None,
) -> list[Path]:
    """
    Get the files of a given kind for a given target and its dependencies.

    Args:
        kind: The kind of files to get. Must be a bazel kind.
        target: The target to get the source files for. Must be a bazel label.
        platform: The platform to use for the query. Inserted directly into
            `--platforms=<platform>`. Default: None.
        compilation_mode: The compilation mode to use for the query. Inserted
            directly into `--compilation_mode=<compilation_mode>`. Default:
            None.
        cpu: The cpu to use for the query. Inserted directly into
            `--cpu=<cpu>`. Default: None.
        depth: The maximum depth to use for the query. If None, no maximum depth
            is used. Default: None.
        no_implicit_deps: Whether to use `--noimplicit_deps` in the query.
            Default: True.
        flags: Additional flags to use for the query. Inserted directly into
            the command. Default: None.

    Returns:
        A list of files (as strings) of the given kind for the given target and
        its dependencies. All paths are relative to the bazel-bin directory
        (i.e. `info.bazel_bin()`). External repository paths also happen to be
        relative to the output base (i.e. `info.output_base()`).
    """
    if depth is None:
        kind_query = f"kind('{kind}', deps({target}))"
    else:
        kind_query = f"kind('{kind}', deps({target}, {depth}))"

    cquery_cmd = [
        'cquery',
        kind_query,
        '--output=jsonproto',
    ]
    if platform is not None:
        cquery_cmd.append(f'--platforms={platform}')
    if compilation_mode is not None:
        cquery_cmd.append(f'--compilation_mode={compilation_mode}')
    if cpu is not None:
        cquery_cmd.append(f'--cpu={cpu}')
    if no_implicit_deps:
        cquery_cmd.append('--noimplicit_deps')
    if flags is not None:
        cquery_cmd.append(flags)

    bazel_result = bazel(*cquery_cmd, cwd=workspace_root())

    query_json = json.loads(bazel_result)
    query_results = query_json.get('results', [])

    files = []
    for result in query_results:
        result_target = result.get('target')
        if result_target is None:
            continue

        if result_target.get('type') == 'SOURCE_FILE':
            label = BazelLabel.from_string(result_target['sourceFile']['name'])
        elif result_target.get('type') == 'GENERATED_FILE':
            label = BazelLabel.from_string(
                result_target['generatedFile']['name']
            )
        else:
            # Skip unknown entry types.
            continue

        if label.repo_name() == '':
            files.append(Path(label.package()) / label.name())
        else:
            repo_path = external_repo_path('@' + label.repo_name())
            files.append(repo_path / label.package() / label.name())

    return files
