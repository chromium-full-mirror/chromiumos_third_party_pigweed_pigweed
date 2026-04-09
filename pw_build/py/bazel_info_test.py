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
"""Tests for bazel info."""

import unittest

from unittest import mock
from pathlib import Path

from pw_build import bazel_info


class TestInfo(unittest.TestCase):
    """Test info class"""

    def setUp(self) -> None:
        bazel_info.workspace_root.cache_clear()
        bazel_info.output_base.cache_clear()
        bazel_info.output_path.cache_clear()
        bazel_info.execution_root.cache_clear()
        bazel_info.external_path.cache_clear()
        bazel_info.bazel_bin.cache_clear()
        bazel_info.config_name.cache_clear()

    @mock.patch('pw_build.bazel_info.os.environ')
    def test_workspace_root(self, mock_environ):
        """Test workspace root."""
        mock_environ.__contains__.return_value = True
        mock_environ.__getitem__.return_value = '/tmp/workspace'
        self.assertEqual(bazel_info.workspace_root(), Path('/tmp/workspace'))
        mock_environ.__getitem__.assert_called_with('BUILD_WORKSPACE_DIRECTORY')

    @mock.patch('pw_build.bazel_info.workspace_root')
    @mock.patch('pw_build.bazel_info.bazel')
    def test_output_base(self, mock_bazel, mock_workspace_root):
        """Test output base."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = '/tmp/output_base'
        self.assertEqual(bazel_info.output_base(), Path('/tmp/output_base'))
        mock_bazel.assert_called_once_with(
            'info', 'output_base', cwd=Path('/tmp/workspace')
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_info.workspace_root')
    @mock.patch('pw_build.bazel_info.bazel')
    def test_output_path(self, mock_bazel, mock_workspace_root):
        """Test output path."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = '/tmp/output_path'
        self.assertEqual(bazel_info.output_path(), Path('/tmp/output_path'))
        mock_bazel.assert_called_once_with(
            'info', 'output_path', cwd=Path('/tmp/workspace')
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_info.workspace_root')
    @mock.patch('pw_build.bazel_info.bazel')
    def test_execution_root(self, mock_bazel, mock_workspace_root):
        """Test execution root."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = '/tmp/execution_root'
        self.assertEqual(
            bazel_info.execution_root(), Path('/tmp/execution_root')
        )
        mock_bazel.assert_called_once_with(
            'info', 'execution_root', cwd=Path('/tmp/workspace')
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_info.output_base')
    def test_external_path(self, mock_output_base):
        """Test external path."""
        mock_output_base.return_value = Path('/tmp/output_base')
        self.assertEqual(
            bazel_info.external_path(), Path('/tmp/output_base/external')
        )
        mock_output_base.assert_called_once_with()

    @mock.patch('pw_build.bazel_info.workspace_root')
    @mock.patch('pw_build.bazel_info.bazel')
    def test_bazel_bin(self, mock_bazel, mock_workspace_root):
        """Test bazel bin."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = '/tmp/k8-fastbuild/bin'
        self.assertEqual(bazel_info.bazel_bin(), Path('/tmp/k8-fastbuild/bin'))
        mock_bazel.assert_called_once_with(
            'info', 'bazel-bin', cwd=Path('/tmp/workspace')
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_info.workspace_root')
    @mock.patch('pw_build.bazel_info.bazel')
    def test_bazel_bin_rp2040(self, mock_bazel, mock_workspace_root):
        """Test bazel bin."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = '/tmp/rp2040-fastbuild/bin'
        self.assertEqual(
            bazel_info.bazel_bin(platform='//targets/rp2040:rp2040'),
            Path('/tmp/rp2040-fastbuild/bin'),
        )
        mock_bazel.assert_called_once_with(
            'info',
            'bazel-bin',
            '--platforms=//targets/rp2040:rp2040',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_info.bazel_bin')
    def test_config_name(self, mock_bazel_bin):
        """Test config name."""
        mock_bazel_bin.return_value = Path('/tmp/k8-fastbuild/bin')
        self.assertEqual(bazel_info.config_name(), 'k8-fastbuild')
        mock_bazel_bin.assert_called_once_with(None, None, None)


if __name__ == '__main__':
    unittest.main()
