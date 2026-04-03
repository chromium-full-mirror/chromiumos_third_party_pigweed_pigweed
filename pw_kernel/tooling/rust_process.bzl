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
"""A rule for generating app process libraries based on the system configuration file."""

load("@rules_rust//rust:defs.bzl", "rust_library")
load(
    "//pw_kernel/tooling:rust_app.bzl",
    "rust_app_codegen",
)

def rust_process(name, codegen_crate_name, multi_process_app, srcs, deps = None, system_config = None, **kwargs):
    """Target to generate a userspace rust process.

    Processes must be listed in a multi_process_app to be included in the final image.

    Args:
        name: The name of the target.
        codegen_crate_name: Name to use for the generated codegen crate.
        multi_process_app: Label string to the multi_process_app this process belongs to.
        system_config: System config file which defines the system.
        srcs: The list of source files for the process.
        deps: The list of dependencies for the process.
        **kwargs: Other attributes passed to the underlying `rust_library` and internal rules.
    """
    if deps == None:
        deps = []

    app_name = Label(multi_process_app).name

    rust_app_codegen(
        name = codegen_crate_name,
        app_name = app_name,
        process_name = name,
        system_config = system_config,
        **kwargs
    )

    rust_library(
        name = name,
        srcs = srcs,
        deps = deps + [
            ":" + codegen_crate_name,
        ],
        **kwargs
    )
