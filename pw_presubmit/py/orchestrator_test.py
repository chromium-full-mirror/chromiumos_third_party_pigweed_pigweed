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
"""Tests for orchestrator."""

import tempfile
from pathlib import Path
import unittest
from unittest import mock


from pw_presubmit.private.events import PresubmitEvents
from pw_presubmit.private.orchestrator import (
    Orchestrator,
    run,
    Mode,
)
from pw_presubmit.private.result import PresubmitResult
from pw_presubmit.private.step import Step, Context

from pw_cli.file_filter import FileFilter
from pw_cli import git_repo
from pw_cli.git_repo import RepoFiles


class PassStep(Step):
    def run(self, ctx: Context) -> None:
        pass


class FailStep(Step):
    def run(self, ctx: Context) -> None:
        ctx.fail('failure')


class FixableStep(Step):
    def __init__(self):
        super().__init__()
        self.run_count = 0

    def run(self, ctx: Context) -> None:
        self.run_count += 1
        if self.run_count == 1:
            ctx.fail('needs fix')

    def fix(self, ctx: Context) -> bool:
        return True


class OrchestratorTest(unittest.TestCase):
    """Tests for Orchestrator."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.output_dir = Path(self.tmp_dir.name)
        self.events = mock.Mock(spec=PresubmitEvents)

    def tearDown(self):
        self.tmp_dir.cleanup()

    def test_run_zero_checks(self):
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(),
            all_paths=(),
            events=self.events,
        )
        result = orch.run('Presubmit', [])
        self.assertTrue(result.success)

    def test_run_multiple_checks(self):
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        result = orch.run('Presubmit', [PassStep(), PassStep()])
        self.assertTrue(result.success)

    def test_run_failure(self):
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        result = orch.run('Presubmit', [FailStep()])
        self.assertFalse(result.success)

    def test_fix_and_rerun(self):
        fixable = FixableStep()
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        result = orch.run('Presubmit', [fixable], mode=Mode.FIX)
        self.assertFalse(result.success)
        self.assertEqual(result.result, PresubmitResult.FIXED)
        self.assertEqual(fixable.run_count, 2)

    def test_filtering(self):
        class PyStep(Step):
            def __init__(self):
                super().__init__(file_filter=FileFilter(endswith='.py'))
                self.run_called = False

            def run(self, ctx: Context) -> None:
                self.run_called = True

        pystep = PyStep()
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        result = orch.run('Presubmit', [pystep])
        self.assertTrue(result.success)
        self.assertFalse(pystep.run_called)

    def test_run_calls_events(self):
        """Test that Orchestrator.run calls event methods."""
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        step = PassStep()
        orch.run('Presubmit', [step])

        self.events.program_start.assert_called_once()
        self.events.step_start.assert_called_once()
        self.events.step_end.assert_called_once()
        self.events.summary.assert_called_once()

    def test_run_takes_name(self):
        """Test that Orchestrator.run takes a name and passes it to events."""
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        step = PassStep()
        orch.run('MyCustomRun', [step])

        self.events.program_start.assert_called_once()
        args, _ = self.events.program_start.call_args
        self.assertEqual(args[0], 'MyCustomRun')

    def test_fix_fails_aborts(self):
        """Test that presubmit aborts if fix fails to fix the issue."""

        class FixFailsStep(FixableStep):
            def run(self, ctx: Context) -> None:
                self.run_count += 1
                ctx.fail('needs fix')

        step = FixFailsStep()
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        result = orch.run('Presubmit', [step], mode=Mode.FIX)

        self.assertEqual(step.run_count, 2)
        self.assertFalse(result.success)

    def test_fix_returns_false(self):
        """Test that if fix() returns False, the check fails normally."""

        class FixReturnsFalseStep(FixableStep):
            def fix(self, _ctx: Context) -> bool:
                return False

        step = FixReturnsFalseStep()
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        result = orch.run('Presubmit', [step], mode=Mode.FIX)

        self.assertEqual(step.run_count, 1)
        self.assertFalse(result.success)

    def test_fix_not_called_when_fix_false(self):
        step = FixableStep()
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        orch.run('Presubmit', [step])

        self.assertEqual(step.run_count, 1)

    def test_fix_not_called_on_passing_check(self):
        class PassingStepWithFix(Step):
            def __init__(self):
                super().__init__()
                self.fix_called = False
                self.run_count = 0

            def run(self, _ctx: Context) -> None:
                self.run_count += 1

            def fix(self, _ctx: Context) -> bool:
                self.fix_called = True
                return True

        step = PassingStepWithFix()
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        orch.run('Presubmit', [step], mode=Mode.FIX)

        self.assertFalse(step.fix_called)
        self.assertEqual(step.run_count, 1)

    def test_fix_crashes(self):
        """Test that an exception in fix() fails the check."""

        class FixCrashesStep(FixableStep):
            def fix(self, _ctx: Context) -> bool:
                raise Exception('Fix crashed!')

        step = FixCrashesStep()
        orch = Orchestrator(
            root=Path('.'),
            output_dir=self.output_dir,
            paths=(Path('file.cc'),),
            all_paths=(Path('file.cc'),),
            events=self.events,
        )
        result = orch.run('Presubmit', [step], mode=Mode.FIX)

        self.assertEqual(step.run_count, 1)
        self.assertFalse(result.success)


class ModuleRunTest(unittest.TestCase):
    """Tests for the module-level run function."""

    def setUp(self) -> None:
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.output_dir = Path(self.tmp_dir.name)

    def tearDown(self) -> None:
        self.tmp_dir.cleanup()

    def test_run_success(self) -> None:
        with mock.patch.object(git_repo, 'collect_files') as mock_collect:
            mock_collect.return_value = RepoFiles(
                paths=[Path('file.cc')], modified_paths=[Path('file.cc')]
            )
            mock_events = mock.Mock(spec=PresubmitEvents)

            step = PassStep()
            result = run(
                name='MyRun',
                steps=[step],
                root=Path('.'),
                events=mock_events,
                output_dir=self.output_dir,
                base='HEAD',
            )

            self.assertTrue(result.success)
            mock_collect.assert_called_once()

    def test_run_failure(self) -> None:
        with mock.patch.object(git_repo, 'collect_files') as mock_collect:
            mock_collect.return_value = RepoFiles(
                paths=[Path('file.cc')], modified_paths=[Path('file.cc')]
            )
            mock_events = mock.Mock(spec=PresubmitEvents)

            step = FailStep()
            result = run(
                name='MyRun',
                steps=[step],
                root=Path('.'),
                events=mock_events,
                output_dir=self.output_dir,
                base='HEAD',
            )

            self.assertFalse(result.success)


if __name__ == '__main__':
    unittest.main()
