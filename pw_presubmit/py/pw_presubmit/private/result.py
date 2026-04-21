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
"""Result types for presubmit checks."""

from __future__ import annotations

import dataclasses
import enum

from pathlib import Path

import pw_cli.color

_COLOR = pw_cli.color.colors()


@dataclasses.dataclass(frozen=True)
class Failure:
    """Represents a presubmit failure, optionally with associated location."""

    description: str
    path: Path | None = None
    line: int | None = None

    def message(self) -> str:
        line_part: str = ''
        if self.line is not None:
            line_part = f'{self.line}:'
        return (
            f'{self.path}:{line_part} {self.description}'
            if self.path
            else self.description
        )


class PresubmitFailure(Exception):
    """Throw PresubmitFailure to immediately abort a presubmit check.

    Alternately, call ctx.fail() on the presubmit context.
    """

    def __init__(
        self,
        description: Failure | str = '',
        path: Path | None = None,
        line: int | None = None,
    ):
        if isinstance(description, Failure):
            self.failure = description
        else:
            self.failure = Failure(
                description=description, path=path, line=line
            )
        super().__init__(self.failure.message())


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


@dataclasses.dataclass(frozen=True)
class ProgramResult:
    """Result of running a presubmit program."""

    passed: int
    failed: int
    skipped: int

    @property
    def total(self) -> int:
        return self.passed + self.failed + self.skipped

    @property
    def result(self) -> PresubmitResult:
        if self.failed or self.skipped:
            return PresubmitResult.FAIL
        return PresubmitResult.PASS

    def message(self) -> str:
        summary_items = []
        if self.passed:
            summary_items.append(f'{self.passed} passed')
        if self.failed:
            summary_items.append(f'{self.failed} failed')
        if self.skipped:
            summary_items.append(f'{self.skipped} not run')
        return ', '.join(summary_items) or 'nothing was done'
