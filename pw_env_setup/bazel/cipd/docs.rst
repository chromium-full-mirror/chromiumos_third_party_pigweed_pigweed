.. _module-pw_env_setup-bazel-cipd:

===========================
Bazel CIPD Module Extension
===========================

.. contents:: Table of Contents
   :depth: 2

--------
Overview
--------
For Bazel projects, Pigweed defines a CIPD extension to allow projects to
configure a named Bazel repository whose contents are CIPD packages.

As Bazel sets up repositories lazily, only the repositories that are ultimately
needed by the build are set up, including downloading and unpacking the CIPD
packages requested.

As these repositories are managed by a Bazel module extension, they are also
sharable between all Bazel modules that use the extension.

---------------
Getting Started
---------------
The extension is loaded from ``//pw_env_setup/bazel/cipd:defs.bzl`` with a
standard Bazel ``use_extension()`` function in your MODULE.bazel.

.. code-block:: starlark

   cipd_ext = use_extension("//pw_env_setup/bazel/cipd:defs.bzl", "cipd")

Once loaded, you can "require" as many Bazel repo names as you'd like, and
specify the combination of CIPD packages and versions that should go in that
repo. In typical use you would typically configure the repo to have only one
package unless the packages were very closely related.

.. code-block:: starlark

   # Configure a repository with a single CIPD package:
   cipd_ext.require(
       name = "my_repo_with_package1_and_package2",
       packages = {
           "some/cipd/package": "cipd:version",
       },
   )

The version can be any version type that `CIPD supports`_: A ref
tag like ``latest``, a key-value tag like ``version:1.0.0``, or a instance ID
hash.

.. _`CIPD supports`: https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/cipd#versions

Once you've configured what you want, you (or downstream users of your Bazel
Module) can ``use_repo`` the repo to import the name into your MODULE's
namespace, and even give it a shorter name in the import:

.. code-block:: starlark

   # Make the repository available to the module:
   use_repo(cipd_ext, my_repo="my_repo_with_package1_and_package2")

--------
Features
--------

Namespacing
===========
As a way of ensuring that a package configuration is only defined once, the
extension enforces a namespace convention on the repository names.

If the name begins with the name followed by a period (e.g.,
"pigweed.toolchain"), only the module whose name matches the prefix can define
that configuration. It's an immediate hard error at Bazel load time if this is
violated.

Doing this allows a module to define CIPD package configurations which will
NEVER conflict with any other module's configuration.

Other modules can however use those repos freely, gaining the benefit of using
a repo with a known configuration shared across repos.

Doing this simplifies version upgrades, as the version is defined in one place,
and not scattered across many Bazel modules. The tradeoff is that everyone is
upgraded at the same time.

Placeholder Expansion
=====================

The package path can contain placeholder strings that will be expanded by the
CIPD command line tool for the current host platform.

It uses the same placeholders supported by the `CIPD ensure tool`_:

- ``"${platform}"``
- ``"${os=val1, val2}"``
- ``"${arch}"``

.. _`CIPD ensure tool`: https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/cipd/client/cipd/ensure/doc.go

Semantic Versioning
===================
The CIPD extension does allow multiple Bazel modules to set up a repo with the
same CIPD package but with different semantic versions.

The extension will configure the Bazel repo with the latest version of the
package, based on all the requests.

Mixing semantic versions with non-semantic versions will result in an error,
since the extension will not be able to determine what is newer.

---------------------
Examples Module Usage
---------------------

Single-Package Repository
=========================
A simple repository containing a single CIPD package.

.. code-block:: starlark

   cipd = use_extension("//pw_env_setup/bazel/cipd:defs.bzl", "cipd")

   cipd.require(
       name = "pigweed.linux_sysroot",
       build_file = "//pw_toolchain/host_clang:linux_sysroot.BUILD",
       packages = {
           "fuchsia/third_party/sysroot/bionic": "git_revision:702eb9654703a7cec1cadf93a7e3aa269d053943",
       },
   )

   use_repo(cipd, linux_sysroot = "pigweed.linux_sysroot")

Version selection
=================
You can use placeholder restrictions to specify specific versions for each OS.

.. code-block:: starlark

   cipd.require(
       name = "buildifier",
       packages = {
           "infra/3pp/tools/buildifier/${platform=linux-amd64,mac-arm64}": "version:2@6.4.0",
           "infra/3pp/tools/buildifier/${platform=windows-amd64}": "version:2@6.3.0",
       },
   )

   use_repo(cipd, "buildifier")

- On ``linux-amd64`` or ``mac-arm64``, this will result in the ``2@6.4.0``
  version of the package being used.
- On ``windows-amd64``, it will result in the ``2@6.3.0`` version of the
  package being used.
- On other platforms, the repo will be stubbed out, allowing for a fallback in
  case there is no need for the package on those platforms.

Multi-Package Repositories
==========================
A repository can also contain multiple CIPD packages.

.. code-block:: starlark

   cipd.require(
       name = "pigweed.tools",
       packages = {
           "infra/3pp/tools/buildifier/${platform}": "version:2@6.4.0",
           "infra/3pp/tools/git/${platform}": "version:2@2.42.0.chromium.11",
       },
   )

   use_repo(cipd, cipd_tools = "pigweed.tools")

Patched Repository
==================
A repository that applies patches after downloading the packages. Requests for
patched repositories **must** use a namespaced name to ensure uniqueness.

.. code-block:: starlark

   cipd.require(
       name = "pigweed.patched_repo",
       packages = {
           "some/package": "version:1.0.0",
       },
       patches = ["//third_party/some:fix.patch"],
       patch_args = ["-p1"],
   )

Root Override
=============

While a module is limited in its ability to modify an existing
``cipd.require`` call made elsewhere, as the root module it can still
create a new requirement and use ``override_repo`` to override it, even if the
repo being replaced uses a namespaced name. Note the extension won't be
involved in checking that the replacement makes sense.

This example will override the package configuration used by everyone using
"pigweed.tools" from the extension.

.. code-block:: starlark

   cipd.require(
       name = "my_override_for_pigweed_tools",
       packages = {
           "infra/3pp/tools/buildifier/${platform}": "version:2@6.0.0",
           "infra/3pp/tools/git/${platform}": "version:2@2.40.0.chromium.11",
       },
   )

   # Override "pigweed.tools" for everyone (if this is the root module)
   override_repo(cipd, {"pigweed.tools" : "my_override_for_pigweed_tools"})

If the module is not the root module, the ``override_repo`` call is ignored by
Bazel.

-----------------
Resolution Report
-----------------
To help understand the version resolution process and see all the requests the
extension received, the extension creates a repository named
``@cipd_resolution`` containing a ``resolution.md`` file, if you import the repo
from the extension.

You can view this file by running:

.. code-block:: bash

   bazel run @cipd_resolution//:show

The extension also generates a ``resolution.bzl`` file in the same repository,
which exposes the resolution data as a Starlark dictionary named
``CIPD_RESOLUTION``. This can be used to inspect the resolution results
programmatically.

.. code-block:: starlark

   load("@cipd_resolution//:resolution.bzl", "CIPD_RESOLUTION")

-----------------------------
Migrating from cipd_rules.bzl
-----------------------------
Previously, projects used ``cipd_repository`` via ``use_repo_rule`` from
``//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl``.

Old way:

.. code-block:: starlark

   cipd_repository = use_repo_rule("//pw_env_setup/bazel/cipd_setup:cipd_rules.bzl", "cipd_repository")

   cipd_repository(
       name = "linux_sysroot",
       build_file = "//pw_toolchain/host_clang:linux_sysroot.BUILD",
       path = "fuchsia/third_party/sysroot/bionic",
       tag = "git_revision:702eb9654703a7cec1cadf93a7e3aa269d053943",
   )

New way:

.. code-block:: starlark

   cipd = use_extension("//pw_env_setup/bazel/cipd:defs.bzl", "cipd")

   cipd.require(
       name = "linux_sysroot",
       build_file = "//pw_toolchain/host_clang:linux_sysroot.BUILD",
       packages = {
           "fuchsia/third_party/sysroot/bionic": "git_revision:702eb9654703a7cec1cadf93a7e3aa269d053943",
       },
   )

   use_repo(cipd, "linux_sysroot")

Key changes:
- Use ``use_extension`` instead of ``use_repo_rule``.
- Use ``cipd.require`` instead of ``cipd_repository``.
- Specify ``packages`` as a dictionary mapping paths to versions, instead of
separate ``path`` and ``tag`` attributes.
