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
"""Tests for the pw_build.bazel_exec module."""

import subprocess
import unittest
from unittest import mock

from pw_build.bazel_errors import ExecError
from pw_build.bazel_exec import bazel


class TestBazel(unittest.TestCase):
    """Tests for bazel_exec.bazel."""

    @mock.patch('subprocess.run')
    def test_bazel_list_args(self, mock_run):
        """Test passing arguments as a list."""
        mock_result = mock.MagicMock()
        mock_result.stdout = b'output'
        mock_run.return_value = mock_result

        result = bazel(['query', '//...'])

        mock_run.assert_called_once_with(
            ['bazelisk', 'query', '//...', '--noshow_progress'],
            check=True,
            capture_output=True,
            cwd=None,
        )
        self.assertEqual(result, 'output')

    @mock.patch('subprocess.run')
    def test_bazel_exploded_args(self, mock_run):
        """Test passing arguments as exploded strings."""
        mock_result = mock.MagicMock()
        mock_result.stdout = b'output'
        mock_run.return_value = mock_result

        result = bazel('query', '//...')

        mock_run.assert_called_once_with(
            ['bazelisk', 'query', '//...', '--noshow_progress'],
            check=True,
            capture_output=True,
            cwd=None,
        )
        self.assertEqual(result, 'output')

    @mock.patch('subprocess.run')
    def test_bazel_success_stdout(self, mock_run):
        """Test successful command returning stdout."""
        mock_result = mock.MagicMock()
        mock_result.stdout = b'  output with spaces  \n'
        mock_run.return_value = mock_result

        result = bazel('query', '//...')

        self.assertEqual(result, 'output with spaces')

    @mock.patch('subprocess.run')
    def test_bazel_success_empty_stdout(self, mock_run):
        """Test successful command with empty stdout."""
        mock_result = mock.MagicMock()
        mock_result.stdout = b''
        mock_run.return_value = mock_result

        result = bazel('build', '//...')

        self.assertEqual(result, '')

    @mock.patch('subprocess.run')
    def test_bazel_failure(self, mock_run):
        """Test failure raising CalledProcessError."""
        mock_run.side_effect = subprocess.CalledProcessError(
            returncode=1,
            cmd=['bazelisk', 'query'],
            stderr=b'some error',
        )

        with self.assertRaises(ExecError) as context:
            bazel('query', '//...')

        self.assertIn('failed to exec', str(context.exception))
        self.assertIn('some error', str(context.exception))

    @mock.patch('subprocess.run')
    def test_bazel_cwd(self, mock_run):
        """Test passing cwd."""
        mock_result = mock.MagicMock()
        mock_result.stdout = b'output'
        mock_run.return_value = mock_result

        result = bazel('query', '//...', cwd='/some/path')

        mock_run.assert_called_once_with(
            ['bazelisk', 'query', '//...', '--noshow_progress'],
            check=True,
            capture_output=True,
            cwd='/some/path',
        )
        self.assertEqual(result, 'output')


if __name__ == '__main__':
    unittest.main()
