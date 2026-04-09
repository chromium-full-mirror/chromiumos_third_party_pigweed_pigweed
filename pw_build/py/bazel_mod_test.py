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
"""Tests for bazel mod."""

import unittest

from unittest import mock
from pathlib import Path

from pw_build import bazel_mod


def _generate_mod_show_repo_output(
    canonical_name: str, apparent_name: str
) -> str:
    return f"""{{
  "canonicalName": "{canonical_name}",
  "repoRuleName": "git_repository",
  "repoRuleBzlLabel": "@@bazel_tools//tools/build_defs/repo:git.bzl",
  "apparentName": "{apparent_name}",
  "attribute": [
    {{
      "name": "$config_dependencies",
      "type": "LABEL_LIST",
      "explicitlySpecified": false,
      "nodep": false
    }},
    {{
      "name": "$configure",
      "type": "BOOLEAN",
      "intValue": 0,
      "stringValue": "false",
      "explicitlySpecified": false,
      "booleanValue": false
    }},
    {{
      "name": "$environ",
      "type": "STRING_LIST",
      "explicitlySpecified": false,
      "nodep": false
    }},
    {{
      "name": "$local",
      "type": "BOOLEAN",
      "intValue": 0,
      "stringValue": "false",
      "explicitlySpecified": false,
      "booleanValue": false
    }},
    {{
      "name": ":action_listener",
      "type": "LABEL_LIST",
      "explicitlySpecified": false,
      "nodep": false
    }},
    {{
      "name": "aspect_hints",
      "type": "LABEL_LIST",
      "explicitlySpecified": false,
      "nodep": false
    }},
    {{
      "name": "branch",
      "type": "STRING",
      "stringValue": "",
      "explicitlySpecified": false,
      "nodep": false
    }},
    {{
      "name": "build_file",
      "type": "LABEL",
      "explicitlySpecified": false,
      "nodep": false
    }},
    {{
      "name": "build_file_content",
      "type": "STRING",
      "stringValue": "",
      "explicitlySpecified": false,
      "nodep": false
    }},
    {{
      "name": "commit",
      "type": "STRING",
      "stringValue": "1cc19a2130c24c9bb4946bb167dd8c6105081412",
      "explicitlySpecified": true,
      "nodep": false
    }}
  ]
}}"""


class TestMod(unittest.TestCase):
    """Test mod class"""

    @mock.patch('pw_build.bazel_mod.workspace_root')
    @mock.patch('pw_build.bazel_mod.bazel')
    def test_external_repo_canonical_name(
        self, mock_bazel, mock_workspace_root
    ):
        """Test external repo canonical name."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = _generate_mod_show_repo_output(
            canonical_name="@bazel_tools",
            apparent_name="@bazel_tools",
        )
        canonical_name = bazel_mod.external_repo_canonical_name("@bazel_tools")
        self.assertEqual(canonical_name, "@bazel_tools")
        mock_bazel.assert_called_once_with(
            'mod',
            'show_repo',
            '@bazel_tools',
            '--output=streamed_jsonproto',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()

    @mock.patch('pw_build.bazel_mod.workspace_root')
    @mock.patch('pw_build.bazel_mod.bazel')
    def test_external_repo_path(self, mock_bazel, mock_workspace_root):
        """Test external path."""
        mock_workspace_root.return_value = Path('/tmp/workspace')
        mock_bazel.return_value = _generate_mod_show_repo_output(
            canonical_name="pico-sdk+",
            apparent_name="@pico-sdk",
        )
        path = bazel_mod.external_repo_path("@pico-sdk")
        self.assertEqual(path, Path("external/pico-sdk+"))
        mock_bazel.assert_called_once_with(
            'mod',
            'show_repo',
            '@pico-sdk',
            '--output=streamed_jsonproto',
            cwd=Path('/tmp/workspace'),
        )
        mock_workspace_root.assert_called_once_with()


if __name__ == '__main__':
    unittest.main()
