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
"""Tests for pw_module create."""

from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from pw_module.create import (
    _ConfigErrors,
    _ModuleConfig,
    _BUILD_FILES,
    _GnBuildFile,
    _BuildFile,
)


class TestModuleConfig(unittest.TestCase):
    """Tests config parsing."""

    @patch('pw_env_setup.config_file.load')
    def test_load_valid_config(self, mock_load):
        """Test loading a valid config."""
        mock_load.return_value = {
            'pw': {
                'pw_module': {
                    'default_build_systems': ['bazel'],
                    'default_languages': ['cc'],
                }
            }
        }
        config = _ModuleConfig.load()
        self.assertEqual(config.default_build_systems, ['bazel'])
        self.assertEqual(config.default_languages, ['cc'])

    @patch('pw_env_setup.config_file.load')
    def test_load_empty_config(self, mock_load):
        """Test loading an empty config."""
        mock_load.return_value = {}
        config = _ModuleConfig.load()
        self.assertEqual(
            config.default_build_systems, list(_BUILD_FILES.keys())
        )
        self.assertEqual(config.default_languages, [])

    @patch('pw_env_setup.config_file.load')
    def test_load_invalid_build_system(self, mock_load):
        mock_load.return_value = {
            'pw': {
                'pw_module': {
                    'default_build_systems': ['ninja'],
                }
            }
        }
        result = _ModuleConfig.load()
        self.assertIsInstance(result, _ConfigErrors)
        self.assertEqual(len(result), 1)
        self.assertIn('Invalid build systems', result[0])
        self.assertIn('ninja', result[0])

    @patch('pw_env_setup.config_file.load')
    def test_load_invalid_language(self, mock_load):
        mock_load.return_value = {
            'pw': {
                'pw_module': {
                    'default_languages': ['cobol'],
                }
            }
        }
        result = _ModuleConfig.load()
        self.assertIsInstance(result, _ConfigErrors)
        self.assertEqual(len(result), 1)
        self.assertIn('Invalid languages', result[0])
        self.assertIn('cobol', result[0])

    @patch('pw_env_setup.config_file.load')
    def test_load_multiple_invalid(self, mock_load):
        mock_load.return_value = {
            'pw': {
                'pw_module': {
                    'default_build_systems': ['bazel', 'ninja'],
                    'default_languages': ['cobol', 'cc'],
                }
            }
        }
        result = _ModuleConfig.load()
        self.assertIsInstance(result, _ConfigErrors)
        self.assertEqual(len(result), 2)
        self.assertTrue(
            any('Invalid build systems' in e and 'ninja' in e for e in result)
        )
        self.assertTrue(
            any('Invalid languages' in e and 'cobol' in e for e in result)
        )


class TestGnModuleCreation(unittest.TestCase):
    """Tests GN module creation."""

    def test_gn_build_file_creation(self):
        """Tests that _GnBuildFile correctly generates a BUILD.gn file."""

        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)

            # Create a minimal context
            mock_ctx = SimpleNamespace(is_upstream=True)

            gn_file = _GnBuildFile(tmp_path, mock_ctx)

            gn_file.add_cc_target(
                _BuildFile.CcTarget(
                    name='pw_async3',
                    headers=[tmp_path / 'public/pw_async3/headers.h'],
                    sources=[tmp_path / 'source.cc'],
                    deps=['//pw_assert'],
                )
            )

            gn_file.add_cc_test(
                _BuildFile.CcTarget(
                    name='pw_async3_test',
                    sources=[tmp_path / 'test.cc'],
                    deps=[':pw_async3'],
                )
            )

            gn_file.add_docs_source('docs.rst')

            with patch('pw_module.create._PW_ROOT', tmp_path):
                gn_file.write()

            generated_file = tmp_path / 'BUILD.gn'
            self.assertTrue(generated_file.exists())

            content = generated_file.read_text()

            self.assertIn('pw_source_set("pw_async3")', content)
            self.assertIn('pw_test("pw_async3_test")', content)
            self.assertIn('pw_doc_group("docs")', content)
            self.assertIn('import("$dir_pw_build/target_types.gni")', content)
            self.assertIn('import("$dir_pw_unit_test/test.gni")', content)
            self.assertIn('import("$dir_pw_docgen/docs.gni")', content)


if __name__ == '__main__':
    unittest.main()
