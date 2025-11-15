# Copyright 2025 The Pigweed Authors
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
"""CLI entrypoint for changelog automation."""

from pathlib import Path
import os
import sys

from changelog import Changelog


def main():
    """Entrypoint to the changelog tool."""
    if "GEMINI_API_KEY" not in os.environ:
        sys.exit("[ERROR] $GEMINI_API_KEY env var not found")
    year = None
    month = None
    for arg in sys.argv:
        if "--year=" in arg:
            year = arg.replace("--year=", "")
        if "--month=" in arg:
            month = arg.replace("--month=", "")
    if year is None or month is None:
        sys.exit("[ERROR] Usage: ./pw changelog --year=YYYY --month=MM")
    root = Path(os.environ.get("BUILD_WORKSPACE_DIRECTORY"))
    Changelog(root, year, month)
    return 0


if __name__ == '__main__':
    sys.exit(main())
