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

"""CIPD repository configuration resolver."""

load("//pw_env_setup/bazel/cipd/private/packages:expand_vars.bzl", "expand_vars")
load("//pw_env_setup/bazel/cipd/private/versions:relaxed_semver_parser.bzl", semver_parse = "parse")

def _create_stub(reason):
    return struct(
        packages = {},
        patches = [],
        patch_args = [],
        build_file = None,
        from_source = None,
        from_name = None,
        from_root = False,
        is_dev = False,
        reason = reason,
    )

def _err(msg, srcs = None):
    return struct(msg = msg, srcs = srcs or [])

def _enforce_module_namespace(name, configs):
    """Reserve <module>.* for repos created by <module>."""
    scope, sep, base = name.rpartition(".")
    if not sep:
        return None

    for config in configs:
        module_scope, _, _ = scope.partition(".")

        if config.from_name != module_scope:
            return _err(
                ("The name \"{0}\" is reserved for CIPD repos created from " +
                 "the {1} module. You may request \"{2}.{3}\" if you wish, or " +
                 "just use the base name \"{3}\".").format(
                    name,
                    module_scope,
                    config.from_name,
                    base,
                ),
                [config.from_source],
            )
    return None

def _enforce_one_request_per_module(name, configs):
    """More than one may be confusing."""
    prev_for_module = {}
    for config in configs:
        # The unit tests use .from_names=None for simplicity for most tests.
        # That value does not happen with normal extension use.
        if not config.from_name:
            continue

        prev = prev_for_module.get(config.from_name)
        prev_for_module[config.from_name] = config
        if prev:
            return _err(
                ("The CIPD repo \"{}\" is being requested from multiple " +
                 "locations in the {} module. Please make only one " +
                 "request.").format(name, config.from_name),
                [prev.from_source, config.from_source],
            )

    return None

def _enforce_patched_repos_are_namespaced(name, configs):
    if "." in name:
        return None

    for c in configs:
        if c.patches or c.patch_args:
            return _err(
                ("Requests for patched repos like \"{0}\" must use use your " +
                 "modules namespace prefix to keep them distinct. E.g., " +
                 "\"{1}.{0}\".").format(name, c.from_name),
                [configs[0].from_source, c.from_source],
            )
    return None

def _check_packages_versions_not_blank(name, configs):
    for c in configs:
        if not c.packages:
            return _err("Request for \"{}\" includes no packages".format(name), [c.from_source])
        for pkg, ver in c.packages.items():
            if not pkg:
                return _err("Request for \"{}\" contains invalid packages".format(name), [c.from_source])
            if not ver:
                return _err("Request for \"{}\" contains invalid versions".format(name), [c.from_source])
    return None

def _check_package_strings_match(name, configs):
    first_config = configs[0]
    first_packages = set(first_config.packages.keys())
    for config in configs[1:]:
        if first_packages != set(config.packages.keys()):
            return _err(
                ("The set of package strings requested for \"{}\" must be " +
                 "the same across all requests, including any placeholders.").format(name),
                [first_config.from_source, config.from_source],
            )
    return None

def _check_build_files_match(name, configs):
    build_file = configs[0].build_file
    for c in configs[1:]:
        if c.build_file != build_file:
            return _err(
                ("All requests for \"{}\" must use the same value for " +
                 "\"build_file\".").format(name),
                [configs[0].from_source, c.from_source],
            )
    return None

def _check_preconditions(name, configs):
    err = _enforce_module_namespace(name, configs)
    if err:
        return err

    err = _enforce_one_request_per_module(name, configs)
    if err:
        return err

    err = _enforce_patched_repos_are_namespaced(name, configs)
    if err:
        return err

    err = _check_packages_versions_not_blank(name, configs)
    if err:
        return err

    err = _check_package_strings_match(name, configs)
    if err:
        return err

    err = _check_build_files_match(name, configs)
    if err:
        return err

    return None

def _select_newest_config(configs, all_identical):
    newest = configs[0]
    if all_identical:
        return newest, None

    first_orig_keys = sorted(newest.packages.keys())
    selected_semvers = {pkg: semver_parse(newest.packages[pkg]) for pkg in first_orig_keys}

    for candidate in configs[1:]:
        current_is_newer = False
        selected_is_newer = False
        c_parsed = {}

        for pkg in first_orig_keys:
            current_version = candidate.packages[pkg]
            selected_version = newest.packages[pkg]

            if current_version == selected_version:
                c_parsed[pkg] = selected_semvers[pkg]
                continue

            current_semver = semver_parse(current_version)
            selected_semver = selected_semvers[pkg]

            if current_semver == None or selected_semver == None:
                return None, _err(
                    ("Can't compare non-semantic versions for package " +
                     "\"{}\". All requests must use them, or else they must " +
                     "be the same tag.").format(pkg),
                    [candidate.from_source, newest.from_source],
                )

            c_parsed[pkg] = current_semver

            if current_semver > selected_semver:
                current_is_newer = True
            elif selected_semver > current_semver:
                selected_is_newer = True

        if current_is_newer and selected_is_newer:
            return None, _err(
                ("Multi-package repos must resolve to a semantic version " +
                 "combination that was requested by at least one module. " +
                 "These modules introduce an irreconcilable version conflict."),
                [candidate.from_source, newest.from_source],
            )

        if current_is_newer:
            newest = candidate
            selected_semvers = c_parsed

    return newest, None

def _are_all_configs_identical(configs):
    first_pkgs = configs[0].packages
    for c in configs[1:]:
        if c.packages != first_pkgs:
            return False
    return True

def _is_dev_only_stub(configs):
    for c in configs:
        if c.from_root or not c.is_dev:
            return False
    return True

def _expand_package_vars(packages, os):
    res_pkgs = {}
    for pkg, ver in packages.items():
        exp_pkg, err = expand_vars(pkg, os)
        if err:
            return None, err
        if exp_pkg:
            res_pkgs[exp_pkg] = ver
    return res_pkgs, None

def resolve_repo_configuration(name, os, configs):
    """Returns the resolved repository configuration or None and an error struct.

    Args:
        name: The name of the repository.
        os: The operating system configuration.
        configs: The repository configurations to resolve.

    Returns:
        A tuple of (resolved_config, error_struct), where error_struct has 'msg' and 'srcs' fields.
     """
    if not configs:
        return None, _err("no configs")

    err = _check_preconditions(name, configs)
    if err:
        return None, err

    all_identical = _are_all_configs_identical(configs)

    selected, err = _select_newest_config(configs, all_identical)
    if err:
        return None, err

    if _is_dev_only_stub(configs):
        return _create_stub("dev-only"), None

    # Now expand variables for the WINNER!
    res_pkgs, err = _expand_package_vars(selected.packages, os)
    if err:
        return None, err

    if not res_pkgs:
        return _create_stub("empty-stub"), None

    reason = "unique" if all_identical else "resolved"

    resolved_config = struct(
        packages = res_pkgs,
        patches = selected.patches,
        patch_args = selected.patch_args,
        build_file = selected.build_file,
        from_source = selected.from_source,
        from_name = selected.from_name,
        from_root = False,
        is_dev = False,
        reason = reason,
    )

    return resolved_config, None
