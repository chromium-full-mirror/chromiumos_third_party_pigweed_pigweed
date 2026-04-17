# Copyright 2020 The Pigweed Authors
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
"""Tools for running presubmit checks in a Git repository.

Presubmit checks are defined as a function or other callable. The function may
take either no arguments or a list of the paths on which to run. Presubmit
checks communicate failure by raising any exception.

For example, either of these functions may be used as presubmit checks:

  @pw_presubmit.filter_paths(endswith='.py')
  def file_contains_ni(ctx: PresubmitContext):
      for path in ctx.paths:
          with open(path) as file:
              contents = file.read()
              if 'ni' not in contents and 'nee' not in contents:
                  raise PresumitFailure('Files must say "ni"!', path=path)

  def run_the_build():
      subprocess.run(['make', 'release'], check=True)

Presubmit checks that accept a list of paths may use the filter_paths decorator
to automatically filter the paths list for file types they care about. See the
pragma_once function for an example.

See pigweed_presbumit.py for an example of how to define presubmit checks.
"""

from __future__ import annotations

import collections
import contextlib
import itertools
import json
import logging
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import types
from typing import (
    Any,
    Callable,
    Collection,
    Iterable,
    Iterator,
    Pattern,
    Sequence,
    Set,
)


import pw_cli.env
from pw_cli.plural import plural
from pw_cli.file_filter import FileFilter, exclude_paths
from pw_package import package_manager
from pw_presubmit.events import PresubmitEvents, HumanUI
from pw_presubmit.check import (
    PresubmitResult,
    Program,
    Check,
    ProgramResult,
    FilteredCheck,
)
from pw_presubmit import git_repo, tools
from pw_presubmit.presubmit_context import (
    FormatOptions,
    LuciContext,
    PRESUBMIT_CONTEXT,
    PresubmitContext,
    PresubmitFailure,
)

_LOG: logging.Logger = logging.getLogger(__name__)


class Programs(collections.abc.Mapping):
    """A mapping of presubmit check programs.

    Use is optional. Helpful when managing multiple presubmit check programs.
    """

    def __init__(self, **programs: Sequence):
        """Initializes a name: program mapping from the provided keyword args.

        A program is a sequence of presubmit check functions. The sequence may
        contain nested sequences, which are flattened.
        """
        self._programs: dict[str, Program] = {
            name: Program(name, checks) for name, checks in programs.items()
        }

    def all_steps(self) -> dict[str, Check]:
        return {c.name: c for c in itertools.chain(*self.values())}

    def __getitem__(self, item: str) -> Program:
        return self._programs[item]

    def __iter__(self) -> Iterator[str]:
        return iter(self._programs)

    def __len__(self) -> int:
        return len(self._programs)


def _print_ui(*args) -> None:
    """Prints to stdout and flushes to stay in sync with logs on stderr."""
    print(*args, flush=True)


# pylint: disable=too-many-instance-attributes
class Presubmit:
    """Runs a series of presubmit checks on a list of files."""

    def __init__(  # pylint: disable=too-many-arguments
        self,
        root: Path,
        repos: Sequence[Path],
        output_directory: Path,
        paths: Sequence[Path],
        all_paths: Sequence[Path],
        package_root: Path,
        override_gn_args: dict[str, str],
        continue_after_build_error: bool,
        rng_seed: int,
        full: bool,
        events: PresubmitEvents | None = None,
    ):
        self._root = root.resolve()
        self._repos = tuple(repos)
        self._output_directory = output_directory.resolve()
        self._paths = tuple(paths)
        self._all_paths = tuple(all_paths)

        self._package_root = package_root.resolve()
        self._override_gn_args = override_gn_args
        self._continue_after_build_error = continue_after_build_error
        self._rng_seed = rng_seed
        self._full = full
        self.events = events or HumanUI()

    def run(
        self,
        program: Program,
        keep_going: bool = False,
        substep: str | None = None,
        dry_run: bool = False,
    ) -> bool:
        """Executes a series of presubmit checks on the paths."""
        filtered_checks = program.filter(self._root, self._paths)
        if substep:
            assert (
                len(filtered_checks) == 1
            ), 'substeps not supported with multiple steps'
            filtered_checks[0].substep = substep

        _LOG.debug('Running %s for %s', program.title(), self._root.name)
        self.events.program_start(program, filtered_checks, self._paths)

        _LOG.info(
            '%d of %d checks apply to %s in %s',
            len(filtered_checks),
            len(program),
            plural(self._paths, 'file'),
            self._root,
        )

        if not self._paths:
            self.events.warning('No files are being checked!')

        _LOG.debug('Checks:\n%s', '\n'.join(c.name for c in filtered_checks))

        start_time: float = time.time()
        passed, failed, skipped = self._execute_checks(
            filtered_checks, keep_going, dry_run
        )
        self._log_summary(time.time() - start_time, passed, failed, skipped)

        return not failed and not skipped

    def _log_summary(
        self, time_s: float, passed: int, failed: int, skipped: int
    ) -> None:
        program_result = ProgramResult(
            passed=passed, failed=failed, skipped=skipped
        )

        _LOG.debug(
            'Finished running %d checks on %s in %.1f s',
            program_result.total,
            plural(self._paths, 'file'),
            time_s,
        )
        _LOG.debug(
            'Presubmit checks %s: %s',
            program_result.result.value,
            program_result.message(),
        )

        self.events.summary(program_result, time_s)

    def _create_presubmit_context(  # pylint: disable=no-self-use
        self, **kwargs
    ):
        """Create a PresubmitContext. Override if needed in subclasses."""
        return PresubmitContext(**kwargs)

    @contextlib.contextmanager
    def _context(self, filtered_check: FilteredCheck, dry_run: bool = False):
        # There are many characters banned from filenames on Windows. To
        # simplify things, just strip everything that's not a letter, digit,
        # or underscore.
        sanitized_name = re.sub(r'[\W_]+', '_', filtered_check.name).lower()
        output_directory = self._output_directory.joinpath(sanitized_name)
        os.makedirs(output_directory, exist_ok=True)

        failure_summary_log = output_directory / 'failure-summary.log'
        failure_summary_log.unlink(missing_ok=True)

        handler = logging.FileHandler(
            output_directory.joinpath('step.log'), mode='w'
        )
        handler.setLevel(logging.DEBUG)

        try:
            _LOG.addHandler(handler)

            yield self._create_presubmit_context(
                root=self._root,
                repos=self._repos,
                output_dir=output_directory,
                failure_summary_log=failure_summary_log,
                paths=filtered_check.paths,
                all_paths=self._all_paths,
                package_root=self._package_root,
                override_gn_args=self._override_gn_args,
                continue_after_build_error=self._continue_after_build_error,
                rng_seed=self._rng_seed,
                full=self._full,
                luci=LuciContext.create_from_environment(),
                format_options=FormatOptions.load(),
                dry_run=dry_run,
            )

        finally:
            _LOG.removeHandler(handler)

    def _execute_checks(
        self,
        checks: Sequence[FilteredCheck],
        keep_going: bool,
        dry_run: bool = False,
    ) -> tuple[int, int, int]:
        """Runs presubmit checks; returns (passed, failed, skipped) lists."""
        passed = failed = 0

        for i, filtered_check in enumerate(checks, 1):
            with self._context(filtered_check, dry_run) as ctx:
                self.events.step_start(filtered_check.check, i, ctx.paths)
                start_time = time.time()
                result = filtered_check.run(ctx)
                duration = time.time() - start_time
                self.events.step_end(filtered_check.check, i, result, duration)

            if result is PresubmitResult.PASS:
                passed += 1
            elif result is PresubmitResult.CANCEL:
                break
            else:
                failed += 1
                if not keep_going:
                    break

        return passed, failed, len(checks) - passed - failed


def _process_pathspecs(
    repos: Iterable[Path], pathspecs: Iterable[str]
) -> dict[Path, list[str]]:
    pathspecs_by_repo: dict[Path, list[str]] = {repo: [] for repo in repos}
    repos_with_paths: Set[Path] = set()

    for pathspec in pathspecs:
        # If the pathspec is a path to an existing file, only use it for the
        # repo it is in.
        if os.path.exists(pathspec):
            # Raise an exception if the path exists but is not in a known repo.
            repo = git_repo.within_repo(pathspec)
            if repo not in pathspecs_by_repo:
                raise ValueError(
                    f'{pathspec} is not in a Git repository in this presubmit'
                )

            # Make the path relative to the repo's root.
            pathspecs_by_repo[repo].append(os.path.relpath(pathspec, repo))
            repos_with_paths.add(repo)
        else:
            # Pathspecs that are not paths (e.g. '*.h') are used for all repos.
            for patterns in pathspecs_by_repo.values():
                patterns.append(pathspec)

    # If any paths were specified, only search for paths in those repos.
    if repos_with_paths:
        for repo in set(pathspecs_by_repo) - repos_with_paths:
            del pathspecs_by_repo[repo]

    return pathspecs_by_repo


def fetch_file_lists(
    root: Path,
    repo: Path,
    pathspecs: list[str],
    exclude: Sequence[Pattern] = (),
    base: str | None = None,
) -> tuple[list[Path], list[Path]]:
    """Returns lists of all files and modified files for the given repo.

    Args:
        root: root path of the project
        repo: path to the roots of Git repository to check
        base: optional base Git commit to list files against
        pathspecs: optional list of Git pathspecs to run the checks against
        exclude: regular expressions for Posix-style paths to exclude
    """

    all_files: list[Path] = []
    modified_files: list[Path] = []

    all_files_repo = tuple(
        exclude_paths(exclude, git_repo.list_files(None, pathspecs, repo), root)
    )
    all_files += all_files_repo

    if base is None:
        modified_files += all_files_repo
    else:
        modified_files += exclude_paths(
            exclude, git_repo.list_files(base, pathspecs, repo), root
        )

    _LOG.info(
        'Checking %s',
        git_repo.describe_files(repo, repo, base, pathspecs, exclude, root),
    )

    return all_files, modified_files


def run(  # pylint: disable=too-many-arguments,too-many-locals
    program: Sequence[Check],
    root: Path,
    repos: Collection[Path] = (),
    base: str | None = None,
    paths: Sequence[str] = (),
    exclude: Sequence[Pattern] = (),
    output_directory: Path | None = None,
    package_root: Path | None = None,
    only_list_steps: bool = False,
    override_gn_args: Sequence[tuple[str, str]] = (),
    keep_going: bool = False,
    continue_after_build_error: bool = False,
    rng_seed: int = 1,
    presubmit_class: type = Presubmit,
    list_steps_file: Path | None = None,
    substep: str | None = None,
    dry_run: bool = False,
) -> bool:
    """Lists files in the current Git repo and runs a Presubmit with them.

    This changes the directory to the root of the Git repository after listing
    paths, so all presubmit checks can assume they run from there.

    The paths argument contains Git pathspecs. If no pathspecs are provided, all
    paths in all repos are included. If paths to files or directories are
    provided, only files within those repositories are searched. Patterns are
    searched across all repositories. For example, if the pathspecs "my_module/"
    and "*.h", paths under "my_module/" in the containing repo and paths in all
    repos matching "*.h" will be included in the presubmit.

    Args:
        program: list of presubmit check functions to run
        root: root path of the project
        repos: paths to the roots of Git repositories to check
        name: name to use to refer to this presubmit check run
        base: optional base Git commit to list files against
        paths: optional list of Git pathspecs to run the checks against
        exclude: regular expressions for Posix-style paths to exclude
        output_directory: where to place output files
        package_root: where to place package files
        only_list_steps: print step names instead of running them
        override_gn_args: additional GN args to set on steps
        keep_going: continue running presubmit steps after a step fails
        continue_after_build_error: continue building if a build step fails
        rng_seed: seed for a random number generator, for the few steps that
            need one
        presubmit_class: class to use to run Presubmits, should inherit from
            Presubmit class above
        list_steps_file: File created by --only-list-steps, used to keep from
            recalculating affected files.
        substep: run only part of a single check

    Returns:
        True if all presubmit checks succeeded
    """
    repos = [repo.resolve() for repo in repos]

    non_empty_repos = []
    for repo in repos:
        if list(repo.iterdir()):
            non_empty_repos.append(repo)
            if git_repo.root(repo) != repo:
                raise ValueError(
                    f'{repo} is not the root of a Git repo; '
                    'presubmit checks must be run from a Git repo'
                )
    repos = non_empty_repos

    pathspecs_by_repo = _process_pathspecs(repos, paths)

    all_files: list[Path] = []
    modified_files: list[Path] = []
    list_steps_data: dict[str, Any] = {}

    if list_steps_file:
        with list_steps_file.open() as ins:
            list_steps_data = json.load(ins)
        all_files.extend(list_steps_data['all_files'])
        for step in list_steps_data['steps']:
            modified_files.extend(Path(x) for x in step.get("paths", ()))
        modified_files = sorted(set(modified_files))
        _LOG.info(
            'Loaded %d paths from file %s',
            len(modified_files),
            list_steps_file,
        )

    else:
        for repo, pathspecs in pathspecs_by_repo.items():
            new_all_files_items, new_modified_file_items = fetch_file_lists(
                root, repo, pathspecs, exclude, base
            )
            all_files.extend(new_all_files_items)
            modified_files.extend(new_modified_file_items)

    if output_directory is None:
        output_directory = root / '.presubmit'

    if package_root is None:
        package_root = output_directory / 'packages'

    presubmit = presubmit_class(
        root=root,
        repos=repos,
        output_directory=output_directory,
        paths=modified_files,
        all_paths=all_files,
        package_root=package_root,
        override_gn_args=dict(override_gn_args or {}),
        continue_after_build_error=continue_after_build_error,
        rng_seed=rng_seed,
        full=bool(base is None),
    )

    if not isinstance(program, Program):
        program = Program('', program)

    if only_list_steps:
        steps: list[dict] = []
        for filtered_check in program.filter(root, modified_files):
            step = {
                'name': filtered_check.name,
                'paths': [str(x) for x in filtered_check.paths],
            }
            substeps = filtered_check.check.substeps()
            if len(substeps) > 1:
                step['substeps'] = [x.name for x in substeps]
            steps.append(step)

        list_steps_data = {
            'steps': steps,
            'all_files': [str(x) for x in all_files],
        }
        json.dump(list_steps_data, sys.stdout, indent=2)
        sys.stdout.write('\n')
        return True

    return presubmit.run(program, keep_going, substep=substep, dry_run=dry_run)


def _make_str_tuple(value: Iterable[str] | str) -> tuple[str, ...]:
    return tuple([value] if isinstance(value, str) else value)


def check(*args, **kwargs):
    """Turn a function into a presubmit check.

    Args:
        *args: Passed through to function.
        *kwargs: Passed through to function.

    If only one argument is provided and it's a function, this function acts
    as a decorator and creates a Check from the function. Example of this kind
    of usage:

    @check
    def pragma_once(ctx: PresubmitContext):
        pass

    Otherwise, save the arguments, and return a decorator that turns a function
    into a Check, but with the arguments added onto the Check constructor.
    Example of this kind of usage:

    @check(name='pragma_twice')
    def pragma_once(ctx: PresubmitContext):
        pass
    """
    if (
        len(args) == 1
        and isinstance(args[0], types.FunctionType)
        and not kwargs
    ):
        # Called as a regular decorator.
        return Check(args[0])

    def decorator(check_function):
        return Check(check_function, *args, **kwargs)

    return decorator


def filter_paths(
    *,
    endswith: Iterable[str] = (),
    exclude: Iterable[Pattern[str] | str] = (),
    file_filter: FileFilter | None = None,
    always_run: bool = False,
) -> Callable[[Callable], Check]:
    """Decorator for filtering the paths list for a presubmit check function.

    Path filters only apply when the function is used as a presubmit check.
    Filters are ignored when the functions are called directly. This makes it
    possible to reuse functions wrapped in @filter_paths in other presubmit
    checks, potentially with different path filtering rules.

    Args:
        endswith: str or iterable of path endings to include
        exclude: regular expressions of paths to exclude
        file_filter: FileFilter used to select files
        always_run: Run check even when no files match
    Returns:
        a wrapped version of the presubmit function
    """

    if file_filter:
        real_file_filter = file_filter
        if endswith or exclude:
            raise ValueError(
                'Must specify either file_filter or '
                'endswith/exclude args, not both'
            )
    else:
        # TODO: b/238426363 - Remove these arguments and use FileFilter only.
        real_file_filter = FileFilter(
            endswith=_make_str_tuple(endswith), exclude=exclude
        )

    def filter_paths_for_function(function: Callable) -> Check:
        return Check(function, real_file_filter, always_run=always_run)

    return filter_paths_for_function


def call(
    *args, call_annotation: dict[Any, Any] | None = None, **kwargs
) -> None:
    """Optional subprocess wrapper that causes a PresubmitFailure on errors."""
    ctx = PRESUBMIT_CONTEXT.get()
    if ctx:
        # Save the subprocess command args for pw build presubmit runner.
        call_annotation = call_annotation if call_annotation else {}
        ctx.append_check_command(
            *args, call_annotation=call_annotation, **kwargs
        )
        # Return without running if dry-run mode is on.
        if ctx.dry_run:
            return

    attributes, command = tools.format_command(args, kwargs)
    _LOG.debug('[RUN] %s\n%s', attributes, command)

    tee = kwargs.pop('tee', None)
    propagate_sigterm = kwargs.pop('propagate_sigterm', False)

    kwargs.setdefault('stdout', subprocess.PIPE)
    kwargs.setdefault('stderr', subprocess.STDOUT)

    process = subprocess.Popen(args, **kwargs)
    assert process.stdout

    # Set up signal handler if requested.
    signaled = False
    if propagate_sigterm:

        def signal_handler(_signal_number: int, _stack_frame: Any) -> None:
            nonlocal signaled
            signaled = True
            process.terminate()

        previous_signal_handler = signal.signal(signal.SIGTERM, signal_handler)

    if pw_cli.env.pigweed_environment().PW_PRESUBMIT_DISABLE_SUBPROCESS_CAPTURE:
        while True:
            line = process.stdout.readline().decode(errors='backslashreplace')
            if not line:
                break
            _LOG.info(line.rstrip())
            if tee:
                tee.write(line)

    stdout, _ = process.communicate()
    if tee:
        tee.write(stdout.decode(errors='backslashreplace'))

    logfunc = _LOG.warning if process.returncode else _LOG.debug
    logfunc('[FINISHED]\n%s', command)
    logfunc(
        '[RESULT] %s with return code %d',
        'Failed' if process.returncode else 'Passed',
        process.returncode,
    )
    if stdout:
        logfunc('[OUTPUT]\n%s', stdout.decode(errors='backslashreplace'))

    if propagate_sigterm:
        signal.signal(signal.SIGTERM, previous_signal_handler)
        if signaled:
            _LOG.warning('Exiting due to SIGTERM.')
            sys.exit(1)

    if process.returncode:
        raise PresubmitFailure


def install_package(
    ctx: PresubmitContext,
    name: str,
    force: bool = False,
) -> None:
    """Install package with given name in given path."""
    root = ctx.package_root
    mgr = package_manager.PackageManager(root)

    ctx.append_check_command(
        'pw',
        'package',
        'install',
        name,
        call_annotation={'pw_package_install': name},
    )
    if ctx.dry_run:
        return

    if not mgr.list():
        raise PresubmitFailure(
            'no packages configured, please import your pw_package '
            'configuration module'
        )

    if not mgr.status(name) or force:
        mgr.install(name, force=force)
