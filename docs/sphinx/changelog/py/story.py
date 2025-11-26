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
"""A group of related commits that constitute a newsworthy story."""

from pathlib import Path
import json

from commit import Commit


class Story:
    """Provides data related to a single story.

    Stories are comprised of a series of related commits that occurred within a
    single month. In changelog updates we discuss these stories (which better
    represent larger bodies of work) rather than individual commits.
    """

    def __init__(self, path: Path, year: int, month: int, root: Commit):
        ### Data setup ###
        self.templates = path / Path("templates")
        self.year = year
        self.month = month
        self.root = root
        self.sha = root.sha
        self.commits: list[Commit] = []
        self.add_commit(root)
        self.body: str | None = None
        self.title: str | None = None
        self.position: int | None = None
        self.highlight: str | None = None

    def add_commit(self, new_commit: Commit):
        for commit in self.commits:
            if new_commit.sha == commit.sha:
                return
        self.commits.append(new_commit)
