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

"""CIPD module extension implementation."""

load("//pw_env_setup/bazel/cipd/private/client:repo.bzl", "client_repo")
load("//pw_env_setup/bazel/cipd/private/packages:repo.bzl", "package_repo", "stub_package_repo")
load("//pw_env_setup/bazel/cipd/private/resolver:repo.bzl", "resolution_repo")
load("//pw_env_setup/bazel/cipd/private/resolver:resolver.bzl", "resolve_repo_configuration")
load("//pw_env_setup/bazel/cipd/private/versions:classify.bzl", "is_immutable_version")

def _resolve_cipd_bin(module_ctx):
    for mod in module_ctx.modules:
        if mod.is_root:
            for client in mod.tags.client:
                if client.cipd_bin:
                    stub_package_repo(name = "cipd_client")
                    return client.cipd_bin
            break

    client_repo(name = "cipd_client")
    return Label("@cipd_client//:cipd")

def _collect_requested_repositories(module_ctx):
    requested = {}
    for mod in module_ctx.modules:
        for tag in mod.tags.require:
            requested.setdefault(tag.name, []).append(struct(
                packages = tag.packages,
                patches = tag.patches,
                patch_args = tag.patch_args,
                build_file = tag.build_file,
                is_dev = tag.dev_dependency,
                from_name = mod.name,
                from_root = mod.is_root,
                from_source = tag,
            ))
    return requested

def _fail_with_context(err):
    """Fail with a message and (if available) the relevant source locations.

    With any module extension, the "module_ctx.modules.tags.*" objects will
    contain some hidden fields accessible to the fail() and print() functions
    that give the actual source code location where the tag was used.

    Unfortunately pre-formatting them into a message string does not include
    those details, and they instead print as an opaque "<unknown object>"
    string.

    This wrapper around fail() arranges for a nicer error message that includes
    those source code locations. This is important as the backtrace won't
    otherwise include them, as the call to the extension module is lazily made
    by Bazel.

    Example output:

        Error in fail: Version configuration conflict detected.
                  See: 'require' tag at path/to/MODULE.bazel:100:17
                 Also: 'require' tag at path/to/MODULE.bazel:110:17

    Args:
        err: An string, or an error object with msg and srcs fields.
    """
    if type(err) == "string":
        args = [err]
    else:
        args = [err.msg]
        if err.srcs:
            args.append("\n" + (" " * 10 + "See:"))
            args.append(err.srcs[0])
            for src in err.srcs[1:]:
                args.append("\n" + (" " * 9) + "Also:")
                args.append(src)
    fail(*args)

def _resolve_repo_configurations(os, requested):
    resolved = {}
    for name, configs in requested.items():
        resolved[name], err = resolve_repo_configuration(name, os = os, configs = configs)
        if err:
            _fail_with_context(err)

    return resolved

def _instantiate_repository_for_config(name, config, cipd_bin):
    if config.packages == None:
        stub_package_repo(name = name)
        return True

    package_repo(
        name = name,
        packages = config.packages,
        build_file = config.build_file,
        patches = config.patches,
        patch_args = config.patch_args,
        cipd_bin = cipd_bin,
    )

    return all([is_immutable_version(v) for v in config.packages.values()])

def _generate_resolution_md(requested_repos, resolved_repos):
    content = "# CIPD Repository Resolution\n\n"
    for name, configs in requested_repos.items():
        content += "## Repository: {}\n".format(name)
        content += "### Requests:\n"
        for c in configs:
            content += "*  From module: `{}`, source: `{}`\n".format(c.from_name, c.from_source)
            content += "    *  Packages: {}\n".format(c.packages)
            if c.patches:
                content += "    *  Patches: {}\n".format(c.patches)

        resolved = resolved_repos.get(name)
        content += "### Resolution:\n"
        if resolved:
            content += "*  Packages: {}\n".format(resolved.packages)
            if resolved.patches:
                content += "*  Patches: {}\n".format(resolved.patches)
            content += "*  Reason: {}\n".format(resolved.reason)
        else:
            content += "*  Failed to resolve.\n"
        content += "\n"
    return content

def _generate_resolution_bzl(requested_repos, resolved_repos):
    requested_data = {}
    for name, configs in requested_repos.items():
        reqs = []
        for c in configs:
            reqs.append({
                "from_module": c.from_name,
                "from_source": str(c.from_source),
                "packages": c.packages,
                "patch_args": c.patch_args,
                "patches": c.patches,
            })
        requested_data[name] = reqs

    resolved_data = {}
    for name, r in resolved_repos.items():
        resolved_data[name] = {
            "packages": r.packages,
            "patch_args": r.patch_args,
            "patches": r.patches,
            "reason": r.reason,
        }

    return repr({
        "requested": requested_data,
        "resolved": resolved_data,
    })

def _create_resolution_repo(requested_repos, resolved_repos):
    md_content = _generate_resolution_md(requested_repos, resolved_repos)
    bzl_content = _generate_resolution_bzl(requested_repos, resolved_repos)
    resolution_repo(
        name = "cipd_resolution",
        content = md_content,
        data_content = bzl_content,
    )

def _cipd_impl(module_ctx):
    cipd_bin = _resolve_cipd_bin(module_ctx)

    requested_repos = _collect_requested_repositories(module_ctx)
    resolved_repos = _resolve_repo_configurations(module_ctx.os, requested_repos)

    _create_resolution_repo(requested_repos, resolved_repos)

    reproducible = True
    for name, config in resolved_repos.items():
        reproducible = _instantiate_repository_for_config(name, config, cipd_bin) and reproducible

    return module_ctx.extension_metadata(reproducible = reproducible)

cipd = module_extension(
    implementation = _cipd_impl,
    tag_classes = {
        "client": tag_class(
            attrs = {
                "cipd_bin": attr.label(
                    default = "@cipd_client//:cipd",
                    doc = "The label for the binary to use.",
                ),
            },
            doc = """Allows a specific cipd client binary to be used.

Only the value set by the root module is used by the extension.

If it is not configured, the extension will set up its own `cipd_client`
repository, and load the cipd binary into it.

This allows a specific binary to be used without downloading an extra copy.
""",
        ),
        "require": tag_class(
            attrs = {
                "build_file": attr.label(
                    allow_single_file = True,
                    doc = """Specifies a BUILD file to inject into the repository.

Overrides any BUILD file in the created repository with a BUILD containing a
copy of the content from the file referenced by this label.
""",
                ),
                "dev_dependency": attr.bool(
                    default = False,
                    doc = """Indicates this package is a dev_dependency.

If true, the request will only be guaranteed to be satisfied if the requestor
is the root module. However another requestor may also cause the repository
to be configured. If all requests use dev_dependency = True, and none of the
requestors are the root module, an empty stub repository will be created.
""",
                ),
                "name": attr.string(
                    mandatory = True,
                    doc = """The name of the repository to create.

If the name includes a period ('.'), the prefix before the first period must
match the name of the module making the request. This allows the module to
define repos names that will never conflict with those from another module.

Other modules may still import those repo names, using the configuration the
defining module specified without having to define it themselves.

Updating the definition in the owning module (typically to use newer versions)
will then cause downstream modules to use that new configuration once the
changes to the owning module are published.
                    """,
                ),
                "packages": attr.string_dict(
                    mandatory = True,
                    allow_empty = False,
                    doc = """A mapping of package paths to versions to download from CIPD.

The package names can include placeholders. See the docstring for the extension
module for more information.
""",
                ),
                "patch_args": attr.string_list(
                    doc = "Arguments to pass to the patch tool. List of strings.",
                ),
                "patches": attr.label_list(
                    doc = "A list of patches to apply to the package after downloading it",
                ),
            },
            doc = """Requests a repository containing a set of CIPD packages.

The extension will evaluate all requests for a given name, and attempt to
resolve them.

If all requests are for the same configuration, the repository will be created
with that configuration (assuming it is otherwise valid).

If requests differ, they must

  - Request versions that are parseable as (relaxed) semantic versions.
  - Not include any patches or patch_args. There is no guarantee such patches
    will apply to the resolved version.
  - Use the same build_file (if any)

If any of those conditions are not met, the extension will raise an error.
""",
        ),
    },
    doc = """This is the Pigweed CIPD module extension.

The extension will evaluate all the requests for CIPD packages, from all
modules that use it. For each repository name it will try to resolve a single
version of the CIPD package (or for composite_repositories, packages) to
install.

If multiple requests are made for the same repository name, the same package
name, but with different versions, the implementation will attempt to pick the
version with the following heuristic:

- If the version can be treated as a semantic version, it will pick the newest
  version from all the requests.
- Otherwise Bazel's root module gets to pick the version used.

If you experience a version conflict, you can resolve it by adding a request to
your root module with the version you want to use, and the `dev_dependency`
attribute set to `True`, which cause your request to be ignored if you are not
the root module.

Manual resolution is needed to since versions can be git commit hashes, with no
fast for this tool to determine what to use.

To prevent hidden problems, the extension will validate that all request
versions are valid, even if they aren't ultimately used. This is done when
setting up the repository, and so it is effectively cached.

It is otherwise an error to try and configure a repository name with
conflicting package information.

As a bonus, if all the versions of all the packages ultimately used refer to
immutable packages, the extension will mark itself as reproducible, which
reduces the state Bazel needs to cache.

## Package names and placeholders

This extension supports the same placeholders syntax documented for CIPD's
ensure file format (See the section on
[Package Definitions](https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/cipd/client/cipd/ensure/doc.go,

This allows for the package to be requested for the current host OS and
architecture, and also allows a package names to be ignored if it does not match
a list of valid values, using the syntax `${var=possible,values}`.

The three placeholders are `${arch}`, `${os}`, and `${platform}` (the last of
which is equivalent to `${os}-${arch}`)

For example, if you want the buildifier package for the current host, you would
just specify:

    `package="infra/3pp/tools/buildifier/${platform}"`

## CIPD Versions

As defined by CIPD, a
[version](https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/cipd/README.md#versions)
can be one of:

- A ref tag (e.g. 'latest')
- a SHA256 hash digest
- A key-value tag (e.g. 'version:1.0.0', or 'git_revision:<hash>')

The last two options are stable identifiers for a package.
""",
)
