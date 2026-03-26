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
"""Tests for the root `./pw` script's freshness bypass logic."""

import os
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


def _find_root_pw() -> Path:
    """Search upwards for the root 'pw' script.

    This is necessary as tests can run in generated output directories.
    """
    current = Path(__file__).resolve().parent
    while current != current.parent:
        candidate = current / 'pw'
        if candidate.is_file() and (current / 'PIGWEED_MODULES').is_file():
            return candidate
        current = current.parent

    raise unittest.SkipTest("Could not find root 'pw' script.")


class TestPwWrapperBypass(unittest.TestCase):
    """Verifies that the `./pw` script can bypass Bazel for fast execution."""

    def setUp(self):
        self.tmp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp_dir.name)

        self.real_pw = _find_root_pw()
        self.pw_script = self.root / 'pw'

        # Create a fake project directory containing the real copied `pw` script
        # alongside a stubbed source and bazel build tree.
        shutil.copy2(self.real_pw, self.pw_script)

        (self.root / 'BUILD.bazel').touch()
        (self.root / 'pw_build' / 'py').mkdir(parents=True)
        (self.root / 'pw_cli' / 'py').mkdir(parents=True)
        (self.root / 'pw_config_loader' / 'py').mkdir(parents=True)

        out_dir = self.root / 'out' / 'workflows_launcher'
        bin_dir = out_dir / 'bazel-bin'
        bin_dir.mkdir(parents=True)

        # Fake prebuilt workflows runner.
        fake_runner = bin_dir / 'pw.exe'
        fake_runner.write_text(
            '#!/bin/sh\necho "EXE_CALLED" >> '
            f'{str(self.root / "run_log.txt")}\n'
        )
        fake_runner.chmod(0o755)

        # Fake bazelisk executable.
        self.fake_bin = self.root / 'bin'
        self.fake_bin.mkdir()
        fake_bazel = self.fake_bin / 'bazelisk'
        fake_bazel.write_text(
            '#!/bin/sh\necho "BAZEL_CALLED" >> '
            f'{str(self.root / "run_log.txt")}\n'
        )
        fake_bazel.chmod(0o755)

        self.marker = out_dir / '.pw_fresh'

        # Inject the fake bazelisk into the script path.
        self.test_env = os.environ.copy()
        self.test_env['PATH'] = f'{self.fake_bin}:{self.test_env["PATH"]}'
        # Prevent picking up real bazel caches
        self.test_env['HOME'] = str(self.root / 'fake_home')
        Path(self.test_env['HOME']).mkdir()

        self.log_file = self.root / 'run_log.txt'

    def tearDown(self):
        self.tmp_dir.cleanup()

    def test_pw_phases(self):
        # 1. Cold start should call the fake bazelisk.
        subprocess.run([str(self.pw_script), '--help'], env=self.test_env)
        self.assertTrue(self.log_file.exists())
        log = self.log_file.read_text()
        self.assertIn('BAZEL_CALLED', log)
        self.log_file.unlink()

        # 2. Re-run should bypass bazelisk and use the fake runner directly.
        subprocess.run([str(self.pw_script), '--help'], env=self.test_env)
        self.assertTrue(self.log_file.exists())
        log = self.log_file.read_text()
        self.assertIn('EXE_CALLED', log)
        self.assertNotIn('BAZEL_CALLED', log)
        self.log_file.unlink()

        # 3. After a python file under pw_build is modified, the script should
        #    all bazelisk again.
        build_py_file = self.root / 'pw_build' / 'py' / 'do_everything.py'
        build_py_file.write_text('print("did everything")')

        # Ensure mtime is newer than the last run's freshness marker.
        future_time = time.time() + 10
        os.utime(build_py_file, (future_time, future_time))

        subprocess.run([str(self.pw_script), '--help'], env=self.test_env)
        log = self.log_file.read_text()
        self.assertIn('BAZEL_CALLED', log)


if __name__ == '__main__':
    unittest.main()
