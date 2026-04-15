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
"""A rule for generating the application linker script based on the system
configuration file.
"""

load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library")
load("//pw_kernel/tooling:app_linker_script.bzl", "app_linker_script")

def _app_codegen_src_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.name + ".rs")

    args = [
        "--template",
        "app=" + ctx.file.template.path,
        "--config",
        ctx.file.system_config.path,
        "--output",
        output.path,
        "render-app-template",
    ]

    if ctx.attr.app_name:
        args.extend(["--app-name", ctx.attr.app_name])

    if ctx.attr.process_name:
        args.extend(["--process-name", ctx.attr.process_name])

    ctx.actions.run(
        inputs = ctx.files.system_config + [ctx.file.template],
        outputs = [output],
        executable = ctx.executable.system_generator,
        mnemonic = "AppCodegenSrc",
        arguments = args,
    )

    return [
        DefaultInfo(files = depset([output])),
    ]

_app_codegen_src = rule(
    implementation = _app_codegen_src_impl,
    attrs = {
        "app_name": attr.string(
            doc = "Name of the application in the configuration file.",
            default = "",
        ),
        "process_name": attr.string(
            doc = "Name of the process in the configuration file.",
            default = "",
        ),
        "system_config": attr.label(
            doc = "System config file which defines the system.",
            allow_single_file = True,
            default = "//pw_kernel/target:system_config_file",
        ),
        "system_generator": attr.label(
            executable = True,
            cfg = "exec",
            default = "@pigweed//pw_kernel/tooling/system_generator:system_generator_bin",
        ),
        "template": attr.label(
            doc = "Application code generation template file.",
            allow_single_file = True,
            default = "@pigweed//pw_kernel/tooling/system_generator/templates:app.rs.jinja",
        ),
    },
    doc = "Generate the linker script for an app based on the system config.",
)

def rust_app_codegen(name, app_name = "", process_name = None, system_config = None, **kwargs):
    """Wrapper function to generate an app's codegen src and rust library.

    Args:
        name: The name of the target.
        app_name: The name of the app in the system manifest.
        process_name: The name of the process in the system manifest.
        system_config: System config file which defines the system.
        **kwargs: Other attributes (slice edition, tags, target_compatible_with) passed to both the `rust_library` and the internal codegen rule.
    """
    tags = kwargs.get("tags", [])

    _app_codegen_src(
        name = name + ".src",
        system_config = system_config,
        app_name = app_name,
        process_name = process_name if process_name else "",
        tags = tags,
    )

    codegen_kwargs = {}
    if "edition" in kwargs:
        codegen_kwargs["edition"] = kwargs["edition"]
    if "tags" in kwargs:
        codegen_kwargs["tags"] = kwargs["tags"]
    if "target_compatible_with" in kwargs:
        codegen_kwargs["target_compatible_with"] = kwargs["target_compatible_with"]

    rust_library(
        name = name,
        srcs = [":{}.src".format(name)],
        deps = [
            "@pigweed//pw_kernel/syscall:syscall_defs",
        ],
        **codegen_kwargs
    )

def rust_app(name, codegen_crate_name, srcs, deps = None, system_config = None, **kwargs):
    """Wrapper function to generate a userspace rust_binary for the kernel.

    Args:
        name: The name of the target.
        codegen_crate_name: Name to use for the generated codegen crate.
        system_config: System config file which defines the system.
        srcs: The list of source files for the app.
        deps: The list of dependencies for the app.
        **kwargs: Other attributes passed to the underlying rules.
    """
    if deps == None:
        deps = []

    rust_app_codegen(
        name = codegen_crate_name,
        app_name = name,
        system_config = system_config,
        **kwargs
    )

    linker_script_name = name + ".linker_script"

    tags = kwargs.get("tags", [])

    app_linker_script(
        name = linker_script_name,
        system_config = system_config,
        app_name = name,
        tags = tags,
    )

    rust_binary(
        name = name,
        srcs = srcs,
        deps = deps + [
            ":" + codegen_crate_name,
            ":" + linker_script_name,
        ],
        **kwargs
    )
