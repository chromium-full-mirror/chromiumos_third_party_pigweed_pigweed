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
import enum
import logging
from pathlib import Path

import pw_cli.color
from pw_cli.collect_files import file_summary
from pw_cli.plural import plural
from pw_presubmit import tools

_LOG = logging.getLogger(__name__)

_COLOR = pw_cli.color.colors()


class PresubmitResult(enum.Enum):
    PASS = 'PASSED'  # Check completed successfully.
    FAIL = 'FAILED'  # Check failed.
    CANCEL = 'CANCEL'  # Check didn't complete.

    def colorized(self, width: int, invert: bool = False) -> str:
        if self is PresubmitResult.PASS:
            color = _COLOR.black_on_green if invert else _COLOR.green
        elif self is PresubmitResult.FAIL:
            color = _COLOR.black_on_red if invert else _COLOR.red
        elif self is PresubmitResult.CANCEL:
            color = _COLOR.yellow
        else:

            def color(value):
                return value

        padding = (width - len(self.value)) // 2 * ' '
        return padding + color(self.value) + padding


class PresubmitEvents(abc.ABC):
    """Abstract base class for presubmit event handlers."""

    @abc.abstractmethod
    def title(self, title: str) -> None:
        """Called with the title of the presubmit run."""

    @abc.abstractmethod
    def warning(self, message: str) -> None:
        """Called with a warning message."""

    @abc.abstractmethod
    def file_summary(self, paths: Sequence[Path]) -> None:
        """Called with a summary of files being checked."""

    @abc.abstractmethod
    def step_header(
        self, count: int, total: int, name: str, num_paths: int
    ) -> None:
        """Called at the start of a presubmit step."""

    @abc.abstractmethod
    def step_footer(
        self, result: PresubmitResult, name: str, time_str: str
    ) -> None:
        """Called at the end of a presubmit step."""

    @abc.abstractmethod
    def summary(
        self,
        result: PresubmitResult,
        total: int,
        num_files: int,
        summary: str,
        time_str: str,
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

    def title(self, title: str) -> None:
        msg = f' {title} '.center(self.width - 2)
        formatted_title = tools.make_box('^').format(
            *self._SUMMARY_BOX, section1=msg, width1=len(msg)
        )
        self._print(formatted_title)

    def warning(self, message: str) -> None:
        self._print(_COLOR.yellow(message))

    def file_summary(self, paths: Sequence[Path]) -> None:
        self._print()
        for line in file_summary(paths):
            self._print(line)
        self._print()

    def step_header(
        self, count: int, total: int, name: str, num_paths: int
    ) -> None:
        middle_text = f'{count}/{total}'
        self._print(
            self._box(
                self._CHECK_UPPER,
                middle_text,
                name,
                plural(num_paths, 'file'),
            )
        )

    def step_footer(
        self, result: PresubmitResult, name: str, time_str: str
    ) -> None:
        self._print(
            self._box(
                self._CHECK_LOWER, result.colorized(self._LEFT), name, time_str
            )
        )

    def summary(
        self,
        result: PresubmitResult,
        total: int,
        num_files: int,
        summary: str,
        time_str: str,
    ) -> None:
        self._print(
            self._box(
                self._SUMMARY_BOX,
                result.colorized(self._LEFT, invert=True),
                f'{total} checks on {plural(num_files, "file")}: {summary}',
                time_str,
            )
        )
