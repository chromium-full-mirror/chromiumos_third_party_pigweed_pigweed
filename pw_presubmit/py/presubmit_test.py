#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
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
"""Tests for presubmit tools."""

import contextlib
import io
import tempfile
from pathlib import Path
import unittest
from unittest import mock

from pw_presubmit import presubmit
from pw_presubmit.events import PresubmitEvents, HumanUI
from pw_presubmit.check import PresubmitResult, Program, ProgramResult, Programs


def _fake_function_1(_):
    """Fake presubmit function."""


def _fake_function_2(_):
    """Fake presubmit function."""


def _all_substeps(program):
    substeps = {}
    for step in program:
        # pylint: disable=protected-access
        for sub in step.substeps():
            substeps[sub.name or step.name] = sub._func
        # pylint: enable=protected-access
    return substeps


class ProgramsTest(unittest.TestCase):
    """Tests the presubmit Programs abstraction."""

    def setUp(self):
        self._programs = Programs(
            first=[_fake_function_1, (), [(_fake_function_2,)]],
            second=[_fake_function_2],
        )

    def test_empty(self) -> None:
        self.assertEqual({}, Programs())

    def test_access_present_members_first(self) -> None:
        self.assertEqual('first', self._programs['first'].name)
        self.assertEqual(
            ('_fake_function_1', '_fake_function_2'),
            tuple(x.name for x in self._programs['first']),
        )

        self.assertEqual(2, len(self._programs['first']))
        substeps = _all_substeps(
            self._programs['first']  # pylint: disable=protected-access
        ).values()
        self.assertEqual(2, len(substeps))
        self.assertEqual((_fake_function_1, _fake_function_2), tuple(substeps))

    def test_access_present_members_second(self) -> None:
        self.assertEqual('second', self._programs['second'].name)
        self.assertEqual(
            ('_fake_function_2',),
            tuple(x.name for x in self._programs['second']),
        )

        self.assertEqual(1, len(self._programs['second']))
        substeps = _all_substeps(
            self._programs['second']  # pylint: disable=protected-access
        ).values()
        self.assertEqual(1, len(substeps))
        self.assertEqual((_fake_function_2,), tuple(substeps))

    def test_access_missing_member(self) -> None:
        with self.assertRaises(KeyError):
            _ = self._programs['not_there']

    def test_all_steps(self) -> None:
        all_steps = self._programs.all_steps()
        self.assertEqual(len(all_steps), 2)
        all_substeps = _all_substeps(all_steps.values())
        self.assertEqual(len(all_substeps), 2)

        # pylint: disable=protected-access
        self.assertEqual(all_substeps['_fake_function_1'], _fake_function_1)
        self.assertEqual(all_substeps['_fake_function_2'], _fake_function_2)
        # pylint: enable=protected-access


class PresubmitEventsTest(unittest.TestCase):
    """Tests for PresubmitEvents."""

    def test_run_calls_events(self) -> None:  # pylint: disable=no-self-use
        """Test that Presubmit.run calls event methods."""
        mock_events = mock.Mock(spec=PresubmitEvents)
        with tempfile.TemporaryDirectory() as tmp_dir_name:
            tmp_dir = Path(tmp_dir_name)
            pre = presubmit.Presubmit(
                root=Path('.'),
                repos=[Path('.')],
                output_directory=tmp_dir / 'out',
                paths=[Path('file.cc')],
                all_paths=[Path('file.cc')],
                package_root=tmp_dir / 'packages',
                override_gn_args={},
                continue_after_build_error=False,
                rng_seed=1,
                full=False,
                events=mock_events,
            )

            program = Program('test_program', [_fake_function_1])
            pre.run(program)

            # Verify calls
            mock_events.program_start.assert_called_once()
            args, _ = mock_events.program_start.call_args
            self.assertEqual(args[0].name, program.name)
            self.assertEqual(tuple(args[0]), tuple(program))
            self.assertEqual(tuple(c.check for c in args[1]), tuple(program))
            self.assertEqual(args[2], (Path('file.cc'),))

            mock_events.step_start.assert_called_once_with(
                program[0], 1, (Path('file.cc'),)
            )

            mock_events.step_end.assert_called_once_with(
                program[0], 1, PresubmitResult.PASS, mock.ANY
            )

            mock_events.summary.assert_called_once_with(
                ProgramResult(passed=1, failed=0, skipped=0), mock.ANY
            )


class HumanUITest(unittest.TestCase):
    """Tests for HumanUI."""

    def test_title(self) -> None:
        """Test title rendering."""
        ui = HumanUI(width=40)
        output = io.StringIO()
        program = Program('test_program', [])
        with contextlib.redirect_stdout(output):
            ui.program_start(program, [], [])

        self.assertIn('test_program', output.getvalue())
        self.assertIn('═', output.getvalue())


if __name__ == '__main__':
    unittest.main()
