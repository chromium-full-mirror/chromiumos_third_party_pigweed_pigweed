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
"""Orchestrates the changelog automation pipeline."""

from datetime import datetime
from pathlib import Path
from string import Template
from threading import Thread
from typing import Callable, TypedDict, Type, TypeVar
import sys

from google import genai
from google.genai import types
from pw_cli.git_repo import GitRepo
from pw_cli.tool_runner import BasicSubprocessRunner

from commit import Commit
from story import Story


T = TypeVar('T')


def generate(prompt, schema: Type[T]) -> T:
    """Generate structured output with Gemini API.

    Explanation of structured output:
    https://ai.google.dev/gemini-api/docs/structured-output

    Args:
        prompt: The complete prompt to provide to Gemini API.

        schema: The response schema that Gemini API must follow.

    Returns:
        The structured output.
    """
    gemini = genai.Client()
    config = types.GenerateContentConfig(
        # https://ai.google.dev/gemini-api/docs/thinking
        # If using Pro, thinking budget accepts a larger maximum value.
        thinking_config=types.ThinkingConfig(thinking_budget=24576),
        response_mime_type="application/json",
        response_schema=schema,
    )
    # Flash actually yields better results than Pro! And is less prone
    # to quota limit issues.
    model = "gemini-2.5-flash"
    try:
        # TODO: https://pwbug.dev/447633516 - Support retries.
        response = gemini.models.generate_content(
            model=model, contents=prompt, config=config
        )
    except genai.errors.ClientError as e:
        sys.exit("[ERROR] Gemini API failed: {}".format(e))
    except genai.errors.ServerError as e:
        sys.exit("[ERROR] Gemini API failed: {}".format(e))
    return response.parsed  # type: ignore[return-value]


def date_spec() -> str:
    return "%Y-%m-%dT%H:%M:%S"


def normalize_month(month: int) -> str:
    """Provide month as a 2-character string e.g. "09"."""
    spec = date_spec()
    date = datetime.strptime("2025-{}-01T00:00:00".format(month), spec)
    return date.strftime("%m")


def normalize_year(year: int) -> str:
    """Provide year as a 4-character string e.g. "2025"."""
    spec = date_spec()
    date = datetime.strptime("{}-01-01T00:00:00".format(year), spec)
    return date.strftime("%Y")


def find_commit(sha: str, commits: list[Commit]) -> Commit | None:
    """Given a Git SHA, find the Commit instance for that SHA."""
    target = None
    for commit in commits:
        if commit.sha != sha:
            continue
        target = commit
    return target


def find_story(root: Commit, stories: list[Story]) -> Story | None:
    """Find the story that has been created for a root commit."""
    for story in stories:
        if story.sha == root.sha:
            return story
    return None


def load_template(path: Path) -> str:
    """Load a prompt template."""
    with open(path, "r") as f:
        template = f.read()
    return template


def parallelize(items: list, fn: Callable, max_threads: int = 5) -> list:
    """Run a function in parallel across a list of objects."""
    start = 0
    while start < len(items):
        threads = []
        for i in range(start, start + max_threads):
            if i >= len(items):
                continue
            thread = Thread(target=fn, args=(items[i],))
            threads.append(thread)
            thread.start()
        for thread in threads:
            thread.join()
        start += max_threads
    return items


def is_latest(cwd: Path, year: str, month: str) -> bool:
    path = cwd / Path("../../index.rst")
    with open(path, "r") as f:
        src = f.readlines()
    sentinel = "   :start-after: .. changelog_highlights_start\n"
    index = src.index(sentinel) - 1
    last = src[index]
    now = ".. include:: changelog/{}/{}/index.rst\n".format(year, month)
    return now > last


class Changelog:
    """Orchestrate the changelog automation pipeline.

    The changelog automation pipeline is basically a series of
    functional-programming-style transformations over the commits that
    occurred in upstream Pigweed over a 1-month timeframe:

    1. Gather all commits

    2. Remove reverted commits

    3. Determine "newsworthy" commits

    4. Group the commits into related "stories" (e.g. all commits related to
       enabling IPC in pw_kernel are grouped into a single story)

    5. Draft content for each story

    6. Copyedit the drafts

    7. Sort the stories in order of newsworthyness

    8. Determine which 15 stories are most interesting and drop the first

    9. Transform the data into reStructuredText (reST)

    10. Glue the new reST file into the docs system
    """

    def __init__(self, root: Path, year: str, month: str):
        # Setup
        self.root = root
        self._path = root / Path("docs/sphinx/changelog/py")
        self._year = int(year)
        self._month = int(month)
        self._repo = GitRepo(root, BasicSubprocessRunner())
        self._outdir = self._path / Path("../{}/{}".format(year, month))
        if not self._outdir.is_dir():
            self._outdir.mkdir(parents=True)
        # Lifecycle
        commits = self._populate_commits()
        commits = self._remove_reverted_commits(commits)
        commits = self._triage_newsworthy_commits(commits)
        self._find_parent(commits)
        stories = self._group(commits)
        self._draft(stories)
        stories = self._drop(stories, commits)
        stories = self._sort(stories, commits)
        self._write(stories)
        self._glue()

    #################### Lifecycle methods ####################

    def _populate_commits(self) -> list[Commit]:
        """Create Commit instances for every commit within the timeframe."""
        spec = "%Y-%m-%dT%H:%M:%S"
        start = datetime.strptime(
            "{}-{}-01T00:00:00".format(self._year, self._month), spec
        )
        end_year = self._year + 1 if self._month == 12 else self._year
        end_month = 1 if self._month == 12 else self._month + 1
        end = datetime.strptime(
            "{}-{}-01T00:00:00".format(end_year, end_month),
            spec,
            # Tip: Use the following end date to speed up development.
            # Processing only the first 5 days rather than an entire month of
            # commits lets you run through the end-to-end changelog automation
            # much faster.
            # "{}-{}-05T00:00:00".format(self._year, self._month), spec
        )
        shas = self._repo.commits(start, end)
        commit_year = start.strftime("%Y")
        commit_month = start.strftime("%m")
        commits = []
        for sha in shas:
            commit = Commit(
                sha, self._repo, self._path, commit_year, commit_month
            )
            # There is no way that a commit without a Gerrit URL has merged into
            # upstream Pigweed so we should ignore it.
            if commit.gerrit is None:
                continue
            # Ignore roll commits.
            if commit.tags is not None and "roll" in commit.tags:
                continue
            commits.append(commit)
        return commits

    def _remove_reverted_commits(  # pylint: disable=no-self-use
        self, all_commits: list[Commit]
    ) -> list[Commit]:
        """Remove commits that have been reverted.

        Remove commits that revert earlier commits. Also remove the reverted
        commits. This is all done via Python parsing of the commit message.
        Gemini is not involved in this step.
        """
        reverted = [
            commit.reverts
            for commit in all_commits
            if commit.reverts is not None
        ]
        commits = []
        for commit in all_commits:
            if commit.full_sha in reverted:
                continue
            commits.append(commit)
        return commits

    def _triage_newsworthy_commits(  # pylint: disable=no-self-use
        self, commits: list[Commit]
    ) -> list[Commit]:
        """Remove non-newsworthy commits.

        Send each commit to Gemini API along with a criteria of "newsworthyness"
        (newsworthy.tmpl) and let Gemini decide whether the commit is
        newsworthy.

        At this stage we also have Gemini generate a summary for each commit.
        """

        def triage(commit):
            """Analyze a single commit for newsworthyness."""

            class Response(TypedDict):
                newsworthy: bool
                reason: str
                summary: str

            template_str = load_template(
                commit.templates / Path("newsworthy.tmpl")
            )
            template = Template(template_str)
            context = "{}\n{}".format(commit.message, commit.diff)
            prompt = template.substitute(context=context)
            results: Response = generate(prompt, Response)
            commit.newsworthy = results["newsworthy"]
            commit.reason = results["reason"]
            commit.summary = results["summary"]
            print("[INFO] \"{}\" ({})".format(commit.title, commit.sha))
            print("[INFO] Newsworthy: {}".format(commit.newsworthy))
            print("[INFO] Reason: {}".format(commit.reason))
            print("[INFO] Summary: {}\n".format(commit.summary))

        max_threads = 10
        commits = parallelize(commits, triage, max_threads)
        newsworthy_commits = [commit for commit in commits if commit.newsworthy]
        return newsworthy_commits

    def _find_parent(  # pylint: disable=no-self-use
        self, commits: list[Commit]
    ) -> None:
        """Find commits that are related to each other.

        Send each commit (the "target") to Gemini API, along with the list of
        commits that occurred before it, and let Gemini decide whether the
        target is related to a previous commit.

        If commits were associated with issues, we could do this
        deterministically with Python. But most Pigweed commits aren't
        associated to issues.
        """

        def find(commit_with_ancestors):
            target = commit_with_ancestors[0]
            ancestors = commit_with_ancestors[1]
            if isinstance(target.parent, (str, bool)):
                print(
                    "[INFO] Skipping parent search: \"{}\" ({})\n".format(
                        target.title, target.sha
                    )
                )
                return
            data = []
            for commit in ancestors:
                data.append({"sha": commit.sha, "summary": commit.summary})
            template_str = load_template(target.templates / Path("parent.tmpl"))
            template = Template(template_str)
            prompt = template.substitute(data=data, target=target.summary)

            class Response(TypedDict):
                sha: str | None
                reason: str

            results: Response = generate(prompt, Response)
            candidate = str(results["sha"])
            if candidate is None:
                target.parent = False
                return
            # Verify that the target is related to the proposed parent by having
            # Gemini compare their commit messages and diffs.
            template_str = load_template(target.templates / Path("verify.tmpl"))
            template = Template(template_str)
            parent = find_commit(candidate, ancestors)
            if parent is None:
                target.parent = False
                return
            a = "".join("{}\n{}\n".format(target.message, target.diff))
            b = "".join("{}\n{}\n".format(parent.message, parent.diff))
            prompt = template.substitute(a=a, b=b)

            class Verify(TypedDict):
                related: bool
                reason: str

            results: Verify = generate(prompt, Verify)
            if results["related"]:
                target.parent = parent.sha
                parent.children.append(target.sha)
            else:
                target.parent = False
            print("[INFO] Target: \"{}\" ({})".format(target.title, target.sha))
            print("[INFO] Parent: \"{}\" ({})".format(parent.title, parent.sha))
            print("[INFO] Related: {}".format(results["related"]))
            print("[INFO] Reason: {}\n".format(results["reason"]))

        # Chronologically sort the commits so that the most recent commits are
        # at the front of the list.
        commits.sort(key=lambda commit: commit.date, reverse=True)
        commits_with_ancestors = []
        for index, commit in enumerate(commits):
            start = index + 1
            # Commits that merged earlier than the target.
            ancestors = commits[start:]
            commits_with_ancestors.append([commit, ancestors])
        max_threads = 10
        parallelize(commits_with_ancestors, find, max_threads)

    def _group(self, commits: list[Commit]) -> list[Story]:
        """Organize commits into stories.

        In the last step, Gemini proposed a "parent" for each commit. This is
        just an earlier commit that seems related to the target commit. In the
        event that commit A is the parent of commit B, and commit B is the
        parent of commit C (A <= B <= C), we want A, B, and C all organized
        into one story. In later steps we provide the entire story of commits
        to Gemini for analysis.
        """

        def find_root(node: Commit, commits: list[Commit]) -> Commit | None:
            root = None
            while isinstance(node.parent, str):
                found_node = find_commit(node.parent, commits)
                if found_node is None:
                    break
                node = found_node
                root = node
            return root

        stories: list[Story] = []
        for commit in commits:
            root = find_root(commit, commits)
            root = commit if root is None else root
            story = find_story(root, stories)
            if story is None:
                story = Story(
                    self._path,
                    self._year,
                    self._month,
                    root,
                )
                stories.append(story)
            else:
                story.add_commit(commit)
        return stories

    def _draft(  # pylint: disable=no-self-use
        self, stories: list[Story]
    ) -> None:
        """Draft the content for each story.

        For each story, provide Gemini API details about each commit in the
        story (message, diff, summary) along with guidelines for crafting a
        story about this story of commits (draft.tmpl).

        Pass the initial draft again through Gemini for some copyediting
        (copyedit.tmpl).
        """

        def draft(story: Story) -> None:
            template_str = load_template(story.templates / Path("draft.tmpl"))
            template = Template(template_str)
            context = ""
            for commit in story.commits:
                context += "Message for commit {}:\n\n".format(commit.sha)
                context += "{}\n\n".format(commit.message)
                context += "Diff for commit {}:\n\n".format(commit.sha)
                context += "{}\n\n".format(commit.diff)
                context += "Summary for commit {}:\n\n".format(commit.sha)
                context += "{}\n\n".format(commit.summary)
            prompt = template.substitute(context=context)

            class Response(TypedDict):
                body: str
                title: str
                highlight: str

            results: Response = generate(prompt, Response)
            title = results["title"]
            body = results["body"]
            highlight = results["highlight"]
            # Copyedit the content to be valid reST, remove links, etc.
            template_str = load_template(
                story.templates / Path("copyedit.tmpl")
            )
            template = Template(template_str)
            context_dict = {
                "title": title,
                "body": body,
                "highlight": highlight,
            }
            prompt = template.substitute(context=str(context_dict))
            copyedit_results: Response = generate(prompt, Response)
            story.title = copyedit_results["title"]
            story.body = copyedit_results["body"]
            story.highlight = copyedit_results["highlight"]
            print("[INFO] {}".format(story.title))
            print("[INFO] {}".format(story.body))
            print("[INFO] {}".format(story.highlight))
            print()

        max_threads = 10
        parallelize(stories, draft, max_threads)

    def _drop(  # pylint: disable=no-self-use
        self, stories: list[Story], commits: list[Commit]
    ) -> list[Story]:
        """Find the 15 most interesting stories and drop the rest.

        Provide Gemini API the draft content for all stories along with
        criteria (drop.tmpl) outlining what stories we consider most important.

        TODO: https://pwbug.dev/447634705 - Make the story count configurable.
        """
        template_str = load_template(stories[0].templates / Path("drop.tmpl"))
        template = Template(template_str)
        context = {}
        for story in stories:
            context[story.sha] = {
                "title": story.title,
                "body": story.body,
                "highlight": story.highlight,
            }
        prompt = template.substitute(context=context)

        class Topic(TypedDict):
            id: str
            reason: str

        results: list[Topic] = generate(prompt, list[Topic])
        out = []
        for topic in results:
            sha = topic["id"]
            root = find_commit(sha, commits)
            if root is None:
                print("[WARN] Gemini hallucinated a SHA: {}".format(sha))
                continue
            story_found = find_story(root, stories)
            if story_found is None:
                print("[WARN] Story not found for root SHA: {}".format(sha))
                continue
            out.append(story_found)
        return out

    def _sort(  # pylint: disable=no-self-use
        self, stories: list[Story], commits: list[Commit]
    ) -> list[Story]:
        """Sort the top 15 stories by newsworthyness.

        Provide Gemini API the draft content for the 15 remaining stories along
        with criteria (sort.tmpl) outlining what stories we consider most
        interesting. The stories are published in the final doc in this order.
        """
        hallucinations: list[str] = []
        # Don't proceed until all stories have been assigned a position
        while len([story for story in stories if story.position is None]) > 0:
            template_str = load_template(
                stories[0].templates / Path("sort.tmpl")
            )
            template = Template(template_str)
            topics = {}
            for story in stories:
                topics[story.sha] = {
                    "title": story.title,
                    "body": story.body,
                    "highlight": story.highlight,
                }
            prompt = template.substitute(
                topics=topics, hallucinations=hallucinations
            )

            class Topic(TypedDict):
                id: str
                reason: str

            results: list[Topic] = generate(prompt, list[Topic])
            for index, topic in enumerate(results):
                sha = topic["id"]
                root = find_commit(sha, commits)
                if root is None:
                    if sha not in hallucinations:
                        hallucinations.append(sha)
                    print("[WARN] Gemini hallucinated a SHA: {}".format(sha))
                    continue
                story_found = find_story(root, stories)
                if story_found is None:
                    print("[WARN] Story not found for root SHA: {}".format(sha))
                    continue
                if story_found.position is None:
                    story_found.position = index
        return sorted(stories, key=lambda story: story.position or 0)

    def _write(self, stories):
        """Transform the data into reST."""

        def section_id(sha):
            return "changelog-{}-{}-{}".format(
                normalize_year(self._year), normalize_month(self._month), sha
            )

        def wrap(text, limit=80):
            lines = []
            while text:
                if len(text) <= limit:
                    lines.append(text)
                    break
                split_point = text.rfind(' ', 0, limit + 1)
                if split_point == -1:
                    lines.append(text[:limit])
                    text = text[limit:]
                else:
                    lines.append(text[:split_point])
                    text = text[split_point:].strip()
            return "\n".join(lines)

        def escape(subject: str) -> str:
            subject = subject.replace("`", r"\`")
            return subject

        date = datetime.strptime(
            "{}-{}".format(self._year, self._month), "%Y-%m"
        ).strftime("%B %Y")
        title = "What's new ({})".format(date)
        rst = ".. _changelog-{}-{}:\n\n".format(
            normalize_year(self._year), normalize_month(self._month)
        )
        rst += "{}\n".format("=" * len(title))
        rst += "{}\n".format(title)
        rst += "{}\n\n".format("=" * len(title))
        rst += ".. changelog_highlights_start\n\n"
        rst += "Highlights:\n\n"
        for story in stories[0:5]:
            rst += "* :ref:`{}`: {}\n\n".format(
                section_id(story.sha), story.highlight
            )
        rst += ".. changelog_highlights_end\n\n"
        for story in stories:
            rst += ".. _{}:\n\n".format(section_id(story.sha))
            rst += "{}\n".format("-" * len(story.title))
            rst += "{}\n".format(story.title)
            rst += "{}\n".format("-" * len(story.title))
            rst += "{}\n\n".format(wrap(story.body, limit=80))
            rst += "Changes:\n\n"
            for commit in story.commits:
                rst += "* `{}\n".format(escape(commit.subject))
                rst += "  <{}>`__\n\n".format(commit.gerrit)
        path = self._outdir / Path("index.rst")
        with open(path, "w") as f:
            f.write(rst)

    def _glue(self):
        """Glue the newly generated reST file into the docs build system."""

        def read(path: Path) -> list[str]:
            with open(path, "r") as f:
                return f.readlines()

        def write(path: Path, lines: list[str]) -> None:
            with open(path, "w") as f:
                f.write("".join(lines))

        def update_homepage(
            cwd, year: str, month: str, month_long: str
        ) -> None:
            if not is_latest(cwd, year, month):
                return
            home_path = cwd / Path("../../index.rst")
            src = read(home_path)
            for index, line in enumerate(src):
                if line.startswith("What's new ("):
                    title = "What's new ({} {})\n".format(month_long, year)
                    src[index - 1] = "{}\n".format("-" * len(title))
                    src[index] = title
                    src[index + 1] = "{}\n".format("-" * len(title))
                if line.startswith(".. include:: changelog"):
                    include = ".. include:: changelog/{}/{}/index.rst\n".format(
                        year, month
                    )
                    src[index] = include
                if line.startswith("And more! See :ref:`changelog-"):
                    ref = "And more! See :ref:`changelog-{}-{}`.\n".format(
                        year, month
                    )
                    src[index] = ref
            write(home_path, src)

        def update_build_file(cwd, year: str, month: str):
            build_path = cwd / Path("../BUILD.bazel")
            src = read(build_path)
            index = src.index("        # sentinel start\n") + 1
            now = '        "{}/{}/index.rst",\n'.format(year, month)
            if now in src:
                return
            src.insert(index, now)
            write(build_path, src)

        def update_changelog_index(
            cwd, year: str, month_short: str, month_long: str
        ):
            index_path = cwd / Path("../index.rst")
            src = read(index_path)
            now = '   {month_long} {year} <{year}/{month_short}/index>\n'.format(  # pylint: disable=line-too-long
                month_long=month_long, year=year, month_short=month_short
            )
            if now in src:
                return
            index = src.index(".. toctree::\n") + 3
            src.insert(index, now)
            write(index_path, src)

        path = self._path
        year = normalize_year(self._year)
        month = normalize_month(self._month)
        month_long = datetime.strptime("{}".format(self._month), "%m").strftime(
            "%B"
        )
        update_build_file(path, year, month)
        update_changelog_index(path, year, month, month_long)
        update_homepage(path, year, month, month_long)
