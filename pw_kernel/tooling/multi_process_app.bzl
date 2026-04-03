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
"""A rule for generating multi-process app packages based on the system configuration file."""

load("@rules_rust//rust:defs.bzl", "rust_binary")
load(
    "//pw_kernel/tooling:app_linker_script.bzl",
    "app_linker_script",
)

def _multi_process_app_src_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.name + ".rs")

    args = [
        "--template",
        "app=" + ctx.file.template.path,
        "--config",
        ctx.file.system_config.path,
        "--output",
        output.path,
        "render-app-template",
        "--app-name",
        ctx.attr.app_name,
    ]

    ctx.actions.run(
        inputs = ctx.files.system_config + [ctx.file.template],
        outputs = [output],
        executable = ctx.executable.system_generator,
        mnemonic = "MultiProcessAppSrc",
        arguments = args,
    )

    return [
        DefaultInfo(files = depset([output])),
    ]

multi_process_app_src = rule(
    implementation = _multi_process_app_src_impl,
    attrs = {
        "app_name": attr.string(
            doc = "The name of the app.",
            mandatory = True,
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
            doc = "Wrapper application template file.",
            allow_single_file = True,
            default = "@pigweed//pw_kernel/tooling/system_generator/templates:multi_process_app.rs.jinja",
        ),
    },
    doc = "Generate the wrapper application source based on the system config.",
)

def multi_process_app(name, processes, system_config = None, **kwargs):
    """Builds a userspace multi-process Rust binary.

    Args:
        name: The name of the target.
        system_config: System config file which defines the system.
        processes: A list of process targets to include in the app.
        **kwargs: Other attributes passed to the underlying `rust_binary` and internal rules.
    """
    tags = kwargs.get("tags", [])
    deps = kwargs.pop("deps", [])
    deps = deps + processes

    multi_process_app_src(
        name = name + ".src",
        system_config = system_config,
        app_name = name,
        tags = tags,
    )

    app_linker_script(
        name = name + ".linker_script",
        system_config = system_config,
        app_name = name,
        tags = tags,
    )

    rust_binary(
        name = name,
        srcs = [":" + name + ".src"],
        edition = "2024",
        deps = deps + [
            ":" + name + ".linker_script",
            "@pigweed//pw_kernel/userspace",
            "@pigweed//pw_kernel/syscall:syscall_user",
            "@pigweed//pw_status/rust:pw_status",
        ],
        **kwargs
    )
