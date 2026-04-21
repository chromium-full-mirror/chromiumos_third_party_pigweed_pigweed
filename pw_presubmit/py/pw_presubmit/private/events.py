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
"""Event interfaces for pw_presubmit."""

from __future__ import annotations
import abc
from collections.abc import Sequence
import logging
from pathlib import Path

import pw_cli.color
from pw_cli.plural import plural
from pw_presubmit.private import tools
from pw_presubmit.private.check import (
    Check,
    Program,
    FilteredCheck,
)
from pw_presubmit.private.result import (
    PresubmitResult,
    ProgramResult,
)

_LOG = logging.getLogger(__name__)

_COLOR = pw_cli.color.colors()


class PresubmitEvents(abc.ABC):
    """Abstract base class for presubmit event handlers."""

    @abc.abstractmethod
    def program_start(
        self,
        program: Program,
        checks: Sequence[FilteredCheck],
        paths: Sequence[Path],
    ) -> None:
        """Called with the program and affected files."""

    @abc.abstractmethod
    def warning(self, message: str) -> None:
        """Called with a warning message."""

    @abc.abstractmethod
    def step_start(
        self, check: Check, step_count: int, paths: Sequence[Path]
    ) -> None:
        """Called at the start of a presubmit step."""

    @abc.abstractmethod
    def step_end(
        self,
        check: Check,
        step_count: int,
        result: PresubmitResult,
        duration_s: float,
    ) -> None:
        """Called at the end of a presubmit step."""

    @abc.abstractmethod
    def summary(
        self,
        result: ProgramResult,
        duration_s: float,
    ) -> None:
        """Called with the final summary of the presubmit run."""


class HumanUI(PresubmitEvents):
    """The default terminal-based UI."""

    _SUMMARY_BOX = '══╦╗ ║║══╩╝'
    _CHECK_UPPER = '━━━┓       '
    _CHECK_LOWER = '       ━━━┛'
    _LEFT = 7
    _RIGHT = 11

    def __init__(self, width: int = 80):
        self.width = width
        self._center = self.width - self._LEFT - self._RIGHT - 4
        self.program: Program | None = None
        self.checks: Sequence[FilteredCheck] = []
        self.paths: Sequence[Path] = []

    @staticmethod
    def _print(*args) -> None:
        print(*args, flush=True)

    def _box(self, style: str, left: str, middle: str, right: str) -> str:
        box = tools.make_box('><>')
        return box.format(
            *style,
            section1=left + ('' if left.endswith(' ') else ' '),
            width1=self._LEFT,
            section2=' ' + middle,
            width2=self._center,
            section3=right + ' ',
            width3=self._RIGHT,
        )

    def program_start(
        self,
        program: Program,
        checks: Sequence[FilteredCheck],
        paths: Sequence[Path],
    ) -> None:
        self.program = program
        self.checks = checks
        self.paths = paths

        title = f'{program.name}: {program.title()}'
        msg = f' {title} '.center(self.width - 2)
        formatted_title = tools.make_box('^').format(
            *self._SUMMARY_BOX, section1=msg, width1=len(msg)
        )
        self._print(formatted_title)

    def warning(self, message: str) -> None:
        self._print(_COLOR.yellow(message))

    def step_start(
        self, check: Check, step_count: int, paths: Sequence[Path]
    ) -> None:
        total = len(self.checks) if self.checks else 0
        num_paths = len(paths)
        middle_text = f'{step_count}/{total}'
        self._print(
            self._box(
                self._CHECK_UPPER,
                middle_text,
                check.name,
                plural(num_paths, 'file'),
            )
        )

    def step_end(
        self,
        check: Check,
        step_count: int,
        result: PresubmitResult,
        duration_s: float,
    ) -> None:
        time_str = tools.format_time(duration_s)
        self._print(
            self._box(
                self._CHECK_LOWER,
                result.colorized(self._LEFT),
                check.name,
                time_str,
            )
        )

    def summary(
        self,
        result: ProgramResult,
        duration_s: float,
    ) -> None:
        self._print(
            self._box(
                self._SUMMARY_BOX,
                result.result.colorized(self._LEFT, invert=True),
                f'{result.total} checks on '
                f'{plural(len(self.paths), "file")}: {result.message()}',
                tools.format_time(duration_s),
            )
        )
