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
"""Provides and caches data related to a single Git commit."""

from pathlib import Path
import json


class Commit:  # pylint: disable=too-many-instance-attributes
    """A single Git commit."""

    # Public

    def __init__(self, sha, repo, path, year, month):
        # Caller-provided data
        self.sha = sha
        self.repo = repo
        self.path = path
        self.year = year
        self.month = month
        self.templates = path / Path("templates")
        # Parsed data
        self.full_sha = repo.commit_hash(commit=sha, short=False)
        self.message = repo.commit_message(sha)
        self.author = repo.commit_author(sha)
        self.issues = repo.commit_issues(sha)
        self.gerrit = repo.commit_review_url(sha)
        self.subject = self.message.splitlines()[0]
        self.reverts = None
        if self.subject.startswith('Revert "'):
            line = self.message.splitlines()[2]
            self.reverts = line.replace("This reverts commit ", "")[:-1]
        self.diff = repo.diff("{}^!".format(sha))
        self.files = repo.commit_files(sha)
        self.tags = self._tags()
        self.title = self.message.splitlines()[0]
        self.date = repo.commit_date(sha)
        # Gemini-generated data
        self.newsworthy = None
        self.reason = None
        self.summary = None
        self.parent = None
        self.children = []
        # Cache setup
        self.cache = path / Path(
            "cache/{}/{}/commits/{}.json".format(year, month, sha)
        )
        if not self.cache.parent.is_dir():
            self.cache.parent.mkdir(parents=True)
        if self.cache.exists():
            self._load()

    def save(self):
        """Save Gemini-generated data to the cache.

        Only cache Gemini-generated data. When iterating on the changelog tool,
        if this data is not cached, iterations become unbearably long.
        """
        data = {
            "newsworthy": self.newsworthy,
            "reason": self.reason,
            "summary": self.summary,
            "parent": self.parent,
            "children": self.children,
        }
        with open(self.cache, "w") as f:
            json.dump(data, f, indent=2)

    # Private

    def _load(self):
        """Load data from the cache."""
        with open(self.cache, "r") as f:
            cache = json.load(f)
        self.newsworthy = cache["newsworthy"]
        self.reason = cache["reason"]
        self.summary = cache["summary"]
        self.parent = cache["parent"]
        self.children = cache["children"]

    def _tags(self):
        """Parse the tags from the first line of the commit message.

        E.g. given `pw_foo: Bar the baz` as the first line, `pw_foo` is the
        tag.

        It's possible for the commit to contain multiple tags e.g.
        `pw_{foo,bar,baz}: Fix the bork`
        """
        if ":" not in self.subject:
            return None
        start = 0
        end = self.subject.index(":")
        prefix = self.subject[start:end]
        if not prefix.startswith("pw_{"):
            return [prefix]
        start = prefix.index("{") + 1
        end = prefix.index("}")
        return [
            "pw_{}".format(name.strip())
            for name in prefix[start:end].split(",")
        ]
