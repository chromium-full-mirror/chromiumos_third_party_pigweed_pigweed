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
"""Introspects bazel configuration."""

from functools import cache
import os
from pathlib import Path

from pw_build.bazel_exec import bazel


@cache
def workspace_root() -> Path:
    """
    Returns the absolute path to the bazel workspace root.

    This is the directory that contains the WORKSPACE file (generally, the root
    of the repository).

    Raises:
        RuntimeError: If pw_bazel is not invoked via `bazel run`.
    """
    assert (
        'BUILD_WORKSPACE_DIRECTORY' in os.environ
    ), 'pw_bazel must be invoked via `bazel run` to work properly.'
    return Path(os.environ['BUILD_WORKSPACE_DIRECTORY'])


@cache
def output_base() -> Path:
    """
    Returns the absolute path to the bazel output base.

    See https://bazel.build/remote/output-directories for more information.
    """
    return Path(bazel('info', 'output_base', cwd=workspace_root()))


@cache
def output_path() -> Path:
    """
    Returns the absolute path to the bazel output path.

    See https://bazel.build/remote/output-directories for more information.
    """
    return Path(bazel('info', 'output_path', cwd=workspace_root()))


@cache
def execution_root() -> Path:
    """
    Returns the absolute path to the bazel execution root.

    See https://bazel.build/remote/output-directories for more information.
    """
    return Path(bazel('info', 'execution_root', cwd=workspace_root()))


@cache
def external_path() -> Path:
    """
    Returns the absolute path to the bazel external directory, where external
    dependencies are downloaded and symlinked.

    See https://bazel.build/remote/output-directories for more information.
    """
    return output_base() / 'external'


@cache
def bazel_bin(
    platform: str | None = None,
    compilation_mode: str | None = None,
    cpu: str | None = None,
) -> Path:
    """
    Returns the absolute path to the bazel bin directory.

    See https://bazel.build/remote/output-directories for more information.

    Args:
        platform: The platform to use for the build. Same as the --platforms
            flag.
        compilation_mode: The compilation mode to use for the build. Same as
            the -c flag.
        cpu: The cpu to use for the build. Same as the --cpu flag.
    """
    args = [
        'info',
        'bazel-bin',
    ]
    if platform is not None:
        args.append(f'--platforms={platform}')
    if compilation_mode is not None:
        args.append(f'--compilation_mode={compilation_mode}')
    if cpu is not None:
        args.append(f'--cpu={cpu}')
    return Path(bazel(*args, cwd=workspace_root()))


@cache
def config_name(
    platform: str | None = None,
    compilation_mode: str | None = None,
    cpu: str | None = None,
) -> str:
    """
    Returns the name of the build configuration. This is generally of the form:
    `<cpu>-<compilation_mode>` (e.g. `k8-fastbuild`).

    Args:
        platform: The platform to use for the build. Same as the --platforms
            flag.
        compilation_mode: The compilation mode to use for the build. Same as
            the -c flag.
        cpu: The cpu to use for the build. Same as the --cpu flag.
    """
    bazel_bin_path = bazel_bin(platform, compilation_mode, cpu)
    return bazel_bin_path.parent.stem
