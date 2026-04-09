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
"""Tests for bazel_query.BazelLabel."""

import unittest

from pw_build.bazel_errors import ParseError
from pw_build.bazel_label import BazelLabel


class TestBazelLabel(unittest.TestCase):
    """Tests for bazel_query.BazelLabel."""

    def test_label_with_repo_package_target(self):
        """Tests a label with a repo, package, and target."""
        label = BazelLabel.from_string('@repo1//foo/bar:baz')
        self.assertEqual(label.repo_name(), 'repo1')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo1//foo/bar:baz')

    def test_label_with_repo_package(self):
        """Tests a label with a repo and package."""
        label = BazelLabel.from_string('@repo1//foo/bar')
        self.assertEqual(label.repo_name(), 'repo1')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(str(label), '@repo1//foo/bar:bar')

    def test_label_with_no_repo(self):
        """Tests a label with no repo."""
        label = BazelLabel.from_string('//foo/bar:baz')
        self.assertEqual(label.repo_name(), '')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@//foo/bar:baz')

    def test_label_with_current_repo_package_target(self):
        """Tests a label with a current repo and target."""
        label = BazelLabel.from_string('@//foo/bar:baz')
        self.assertEqual(label.repo_name(), '')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@//foo/bar:baz')

    def test_label_with_current_repo_package(self):
        """Tests a label with a current repo and package."""
        label = BazelLabel.from_string('@//foo/bar')
        self.assertEqual(label.repo_name(), '')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(str(label), '@//foo/bar:bar')

    def test_label_with_repo_target(self):
        """Tests a label with a repo and target."""
        label = BazelLabel.from_string('@repo1//:baz')
        self.assertEqual(label.repo_name(), 'repo1')
        self.assertEqual(label.package(), '')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo1//:baz')

    def test_invalid_label_with_repo_only(self):
        """Tests an invalid label with a repo only."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('@repo1')

    def test_label_with_arg_repo_name(self):
        """Tests a label with a package and target."""
        label = BazelLabel.from_string('//foo/bar:baz', repo_name='repo2')
        self.assertEqual(label.repo_name(), 'repo2')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo2//foo/bar:baz')

    def test_label_with_arg_repo_name_package(self):
        """Tests a label with a package only."""
        label = BazelLabel.from_string(
            ':baz', repo_name='repo2', package='foo/bar'
        )
        self.assertEqual(label.repo_name(), 'repo2')
        self.assertEqual(label.package(), 'foo/bar')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo2//foo/bar:baz')

    def test_label_with_target_only(self):
        """Tests a label with a target only."""
        label = BazelLabel.from_string(':baz', repo_name='repo2', package='qux')
        self.assertEqual(label.repo_name(), 'repo2')
        self.assertEqual(label.package(), 'qux')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(str(label), '@repo2//qux:baz')

    def test_invalid_label_with_none(self):
        """Tests an invalid label with no name or package."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('', repo_name='repo2', package='qux')

    def test_invalid_label_relative(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('../foo/bar:baz')

    def test_invalid_label_double_colon(self):
        """Tests a label with an invalid (non-absolute) package name."""
        with self.assertRaises(ParseError):
            BazelLabel.from_string('//foo:bar:baz')


if __name__ == '__main__':
    unittest.main()
