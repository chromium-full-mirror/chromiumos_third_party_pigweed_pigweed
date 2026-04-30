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
"""Orchestrator for running presubmit checks."""

from __future__ import annotations

import logging
import os
import shutil
from pathlib import Path
import re
import contextlib
import collections
import enum
import tempfile
import time
from typing import Iterable, Sequence

from pw_cli.file_filter import FileFilter
from pw_cli import git_repo
from pw_presubmit.private.events import PresubmitEvents
from pw_presubmit.private.result import (
    PresubmitFailure,
    PresubmitResult,
    ProgramResult,
)
from pw_presubmit.private.step import Context, Step, FilteredStep
from pw_presubmit.private.tools import relative_paths
from pw_presubmit.tools import PresubmitToolRunner

_LOG = logging.getLogger("pw_presubmit")


class Mode(enum.Enum):
    STOP = 'stop'  # Stop on errors
    CONTINUE = 'continue'  # Continue past errors
    FIX = 'fix'  # Attempt to fix errors

    def __str__(self) -> str:
        return self.value


class Orchestrator:
    """Manages the execution of presubmit checks.

    This class coordinates running a set of checks against a set of files,
    handling filtering, output directory management, and reporting results
    via events.

    It also supports automatically applying fixes if the check provides them
    and the user requested it.

    This class is a streamlined alternative to the older `Presubmit` runner.
    It simplifies the execution flow and avoids heavy dependencies, while
    maintaining compatibility with existing check logic.
    """

    def __init__(
        self,
        root: Path,
        output_dir: Path,
        paths: Sequence[Path],
        all_paths: Sequence[Path],
        events: PresubmitEvents,
    ) -> None:
        """Initializes the orchestrator.

        Args:
            root: The root directory of the project.
            output_dir: The directory where output files will be written.
            paths: Selected / modified files.
            all_paths: All files that the presubmit checks apply to.
            events: Event handler for reporting progress and results.
        """
        self._root = root.resolve()
        self._output_dir = output_dir.resolve()
        self._paths = tuple(paths)
        self._all_paths = tuple(all_paths)
        self._relative_paths = tuple(relative_paths(self._paths, self._root))
        self._events = events

    def run(
        self,
        name: str,
        steps: Iterable[Step],
        *,
        mode: Mode = Mode.STOP,
    ) -> ProgramResult:
        """Executes a series of presubmit checks."""
        # Group steps by their FileFilter to avoid redundant evaluations.
        filter_to_steps = collections.defaultdict(list)
        for s in steps:
            filter_to_steps[s.filter].append(s)

        checks_to_paths = {}
        for filt, steps_with_filt in filter_to_steps.items():
            rel_paths = filt.filter(self._relative_paths)
            abs_paths = tuple(self._root / p for p in rel_paths)
            for s in steps_with_filt:
                checks_to_paths[s] = abs_paths

        # Reconstruct all_steps in original order.
        all_steps = [FilteredStep(s, checks_to_paths[s]) for s in steps]

        filtered_steps = tuple(s for s in all_steps if s.paths)

        _LOG.debug(
            'Running %d of %d steps', len(filtered_steps), len(all_steps)
        )

        self._events.program_start(
            name,
            [s.name for s in all_steps],
            [s.name for s in filtered_steps],
            self._relative_paths,
        )

        passed: list[FilteredStep] = []
        failed: list[FilteredStep] = []
        fixed: list[FilteredStep] = []
        skipped: list[FilteredStep] = []

        start_time = time.time()

        for i, step in enumerate(filtered_steps, 1):
            try:
                result = self._run_step(step, i, mode=mode)
            except RuntimeError as e:
                _LOG.critical('%s', e)
                failed.append(step)
                skipped = list(filtered_steps[i:])
                break

            if result is PresubmitResult.PASS:
                passed.append(step)
            elif result is PresubmitResult.FIXED:
                fixed.append(step)
            elif result is PresubmitResult.CANCEL:
                skipped = list(filtered_steps[i:])
                break
            else:
                failed.append(step)
                if mode is not Mode.CONTINUE:
                    skipped = list(filtered_steps[i:])
                    break

        duration = time.time() - start_time

        program_result = ProgramResult(
            passed=[s.name for s in passed],
            failed=[s.name for s in failed],
            skipped=[s.name for s in skipped],
            fixed=[s.name for s in fixed],
            success=not failed and not fixed,
        )
        self._events.summary(program_result, duration)

        return program_result

    def _run_step(
        self, step: FilteredStep, index: int, mode: Mode
    ) -> PresubmitResult:
        """Runs a single check step, applying fixes if supported."""
        self._events.step_start(step.name, index, step.paths)
        start_time = time.time()

        if (result := self._execute_run_impl(step)) is PresubmitResult.FAIL:
            result = self._attempt_fix(step, fix=mode is Mode.FIX)

        duration = time.time() - start_time
        self._events.step_end(step.name, index, result, duration)

        return result

    @contextlib.contextmanager
    def _context(self, step: FilteredStep):
        """Creates a context and manages its output directory."""
        sanitized_name = re.sub(r'[\W_]+', '_', step.name).lower()
        step_output_dir = self._output_dir / sanitized_name

        shutil.rmtree(step_output_dir, ignore_errors=True)
        os.makedirs(step_output_dir)

        ctx = Context(
            root=self._root,
            output_dir=step_output_dir,
            paths=tuple(step.paths),
            all_paths=self._all_paths,
        )

        try:
            yield ctx
        finally:
            if not ctx.failed:
                shutil.rmtree(step_output_dir, ignore_errors=True)

    def _execute_run_impl(self, step: FilteredStep) -> PresubmitResult:
        """Executes Step.run with a Context and handles exceptions."""
        with self._context(step) as ctx:
            try:
                step.step.run(ctx)
                if ctx.failed:
                    return PresubmitResult.FAIL
                return PresubmitResult.PASS
            except PresubmitFailure as failure:
                if str(failure):
                    _LOG.warning('%s', failure)
                return PresubmitResult.FAIL
            except Exception:  # pylint: disable=broad-except
                _LOG.exception(
                    'Presubmit step %s failed unexpectedly!', step.name
                )
                ctx.fail('Step failed unexpectedly with an exception.')
                return PresubmitResult.FAIL
            except KeyboardInterrupt:
                print()
                return PresubmitResult.CANCEL

    def _attempt_fix(
        self,
        step: FilteredStep,
        fix: bool,
    ) -> PresubmitResult:
        """Attempts to fix failures."""
        if not fix:
            if type(step.step).fix is not Step.fix:
                _LOG.info('Automatic fix available; run with --fix to apply')
            return PresubmitResult.FAIL

        if type(step.step).fix is Step.fix:
            return PresubmitResult.FAIL

        _LOG.debug('Applying fix for %s', step.name)

        with self._context(step) as ctx:
            try:
                if not step.step.fix(ctx):
                    return PresubmitResult.FAIL
            except Exception:  # pylint: disable=broad-exception-caught
                _LOG.exception(
                    'Fix implementation for %s crashed!',
                    step.name,
                )
                ctx.fail('Fix implementation crashed with an exception.')
                return PresubmitResult.FAIL

        result = self._execute_run_impl(step)

        if result is PresubmitResult.PASS:
            return PresubmitResult.FIXED

        raise RuntimeError(
            f'{step.name} failed after running its fix() '
            f'implementation! This is invalid; '
            f'the fix implementation is broken.'
        )


def run(  # pylint: disable=too-many-arguments
    name: str,
    steps: Iterable[Step],
    root: Path,
    events: PresubmitEvents,
    base: str,
    *,
    repos: Iterable[Path] | None = None,
    pathspecs: Iterable[str] = (),
    output_dir: Path | None = None,
    file_filter: FileFilter = FileFilter(),
    mode: Mode = Mode.STOP,
) -> ProgramResult:
    """Simplifies running presubmit checks on one or more repos.

    This collects files from the provided git repositorites, instantiates an
    Orchestrator, and runs the presubmit.
    """
    if repos is None:
        repos = [root]

    with contextlib.ExitStack() as stack:
        if output_dir is None:
            output_dir = Path(
                stack.enter_context(tempfile.TemporaryDirectory())
            )

        repo_files = git_repo.collect_files(
            repos=repos,
            pathspecs=pathspecs,
            base=base,
            file_filter=file_filter,
            root=root,
            tool_runner=PresubmitToolRunner(),
        )

        orchestrator = Orchestrator(
            root=root,
            output_dir=output_dir,
            paths=repo_files.modified_paths,
            all_paths=repo_files.paths,
            events=events,
        )

        return orchestrator.run(name, steps, mode=mode)
