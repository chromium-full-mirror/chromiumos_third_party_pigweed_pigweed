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
"""
Provides a python API for executing bazel.
"""

from pathlib import Path
import subprocess

from pw_build.bazel_errors import ExecError


def bazel(*args: str, cwd: str | Path | None = None) -> str:
    """
    Execute a bazel command in the workspace.

    Args:
        *args: Positional arguments to pass to bazel. May also be a single list
            of strings.
        cwd: The working directory to execute the command in. If None, the
            current working directory is used.

    Returns:
        The stdout of the command, stripped of leading/trailing whitespace.

    Raises:
        ExecError: If the command fails.
    """
    if len(args) == 1 and isinstance(args[0], list):
        items = args[0]
    else:
        items = args

    if isinstance(cwd, Path):
        cwd = str(cwd.resolve())

    cmd = ['bazelisk'] + list(items) + ['--noshow_progress']
    try:
        result = subprocess.run(
            cmd,
            check=True,
            capture_output=True,
            cwd=cwd,
        )
        # The extra 'str()' facilitates MagicMock.
        return str(result.stdout.decode('utf-8')).strip()
    except subprocess.CalledProcessError as error:
        errmsg = error.stderr.decode('utf-8')
        cmdline = ' '.join(cmd)
        raise ExecError(f'failed to exec `{cmdline}`: {errmsg}')
