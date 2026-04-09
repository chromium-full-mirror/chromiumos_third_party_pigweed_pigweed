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
"""Tests for bazel query."""

import json
import unittest
from unittest import mock
from pathlib import Path

from pw_build.bazel_label import BazelLabel
from pw_build import bazel_cquery

_RESULTS_SOURCE_FILES1 = {
    'results': [
        {
            'target': {
                'type': 'SOURCE_FILE',
                'sourceFile': {
                    'name': '//foo/bar:baz.cc',
                },
            },
        },
    ],
}
_RESULTS_SOURCE_FILES2 = {
    'results': [
        {
            'target': {
                'type': 'SOURCE_FILE',
                'sourceFile': {
                    'name': '//foo/bar:baz.cc',
                },
            },
        },
        {
            'target': {
                'type': 'SOURCE_FILE',
                'sourceFile': {
                    'name': '//foo/bar:qux.cc',
                },
            },
        },
    ],
}
_RESULTS_EXTERNAL_FILES = {
    'results': [
        {
            'target': {
                'type': 'SOURCE_FILE',
                'sourceFile': {
                    'name': '@external_repo//foo/bar:baz.cc',
                },
            },
        },
    ],
}
_RESULTS_GENERATED_FILES2 = {
    'results': [
        {
            'target': {
                'type': 'GENERATED_FILE',
                'generatedFile': {
                    'name': '//foo/bar:baz.cc',
                },
            },
        },
        {
            'target': {
                'type': 'GENERATED_FILE',
                'generatedFile': {
                    'name': '//foo/bar:qux.cc',
                },
            },
        },
    ],
}


class TestCQuery(unittest.TestCase):
    """Test query class"""

    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_source_files_target_only(self, mock_bazel, mock_workspace_root):
        """Test source files with target only."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_SOURCE_FILES2)
        self.assertEqual(
            bazel_cquery.source_files(BazelLabel.from_string('//foo/bar:baz')),
            [
                Path('foo/bar/baz.cc'),
                Path('foo/bar/qux.cc'),
            ],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('source file', deps(@//foo/bar:baz))",
            '--output=jsonproto',
            '--noimplicit_deps',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_source_files_depth_1(self, mock_bazel, mock_workspace_root):
        """Test source files with depth 1."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_SOURCE_FILES1)
        self.assertEqual(
            bazel_cquery.source_files(
                BazelLabel.from_string('//foo/bar:baz'), depth=1
            ),
            [Path('foo/bar/baz.cc')],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('source file', deps(@//foo/bar:baz, 1))",
            '--output=jsonproto',
            '--noimplicit_deps',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_source_files_depth_2(self, mock_bazel, mock_workspace_root):
        """Test source files with depth 2."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_SOURCE_FILES2)
        self.assertEqual(
            bazel_cquery.source_files(
                BazelLabel.from_string('//foo/bar:baz'), depth=2
            ),
            [
                Path('foo/bar/baz.cc'),
                Path('foo/bar/qux.cc'),
            ],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('source file', deps(@//foo/bar:baz, 2))",
            '--output=jsonproto',
            '--noimplicit_deps',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_cquery.external_repo_path')
    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_source_files_external(
        self, mock_bazel, mock_workspace_root, mock_external_repo_path
    ):
        """Test external repo source files."""
        mock_external_repo_path.return_value = Path('external/external_repo')
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_EXTERNAL_FILES)
        self.assertEqual(
            bazel_cquery.source_files(
                BazelLabel.from_string('@external_repo//foo/bar:baz')
            ),
            [Path('external/external_repo/foo/bar/baz.cc')],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('source file', deps(@external_repo//foo/bar:baz))",
            '--output=jsonproto',
            '--noimplicit_deps',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()
        mock_external_repo_path.assert_called_once_with('@external_repo')

    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_source_files_with_implicit_deps(
        self, mock_bazel, mock_workspace_root
    ):
        """Test source files with implicit deps."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_SOURCE_FILES2)
        self.assertNotEqual(
            bazel_cquery.source_files(
                BazelLabel.from_string('//pw_bazel/test:bar'),
                no_implicit_deps=False,
            ),
            [
                Path('pw_bazel/test/bar.cc'),
                Path('pw_bazel/test/foo.cc'),
            ],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('source file', deps(@//pw_bazel/test:bar))",
            '--output=jsonproto',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_source_files_with_flags(self, mock_bazel, mock_workspace_root):
        """Test source files with flags."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_SOURCE_FILES2)
        self.assertEqual(
            bazel_cquery.source_files(
                BazelLabel.from_string('//foo/bar:baz'),
                flags=['--config=rp2040'],
            ),
            [
                Path('foo/bar/baz.cc'),
                Path('foo/bar/qux.cc'),
            ],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('source file', deps(@//foo/bar:baz))",
            '--output=jsonproto',
            '--noimplicit_deps',
            '--config=rp2040',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_generated_files_target_only(self, mock_bazel, mock_workspace_root):
        """Test generated files."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_GENERATED_FILES2)
        self.assertEqual(
            bazel_cquery.generated_files(
                BazelLabel.from_string('//foo/bar:baz')
            ),
            [
                Path('foo/bar/baz.cc'),
                Path('foo/bar/qux.cc'),
            ],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('generated file', deps(@//foo/bar:baz))",
            '--output=jsonproto',
            '--noimplicit_deps',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_cquery.workspace_root')
    @mock.patch('pw_build.bazel_cquery.bazel')
    def test_generated_files_with_implicit_deps(
        self, mock_bazel, mock_workspace_root
    ):
        """Test generated files with implicit deps."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = json.dumps(_RESULTS_GENERATED_FILES2)
        self.assertEqual(
            bazel_cquery.generated_files(
                BazelLabel.from_string('//foo/bar:baz'), no_implicit_deps=False
            ),
            [
                Path('foo/bar/baz.cc'),
                Path('foo/bar/qux.cc'),
            ],
        )
        mock_bazel.assert_called_once_with(
            'cquery',
            "kind('generated file', deps(@//foo/bar:baz))",
            '--output=jsonproto',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()


if __name__ == '__main__':
    unittest.main()
