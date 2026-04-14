Create the following bazel targets in the ``pw_ide/bazel/compile_commands/test`` directory. Do not change the existing targets, but reuse the newly created targets where appropriate.

# No deps
Create a cc_library target with C source+header and no deps.
Create a cc_library target with C++ source+header and no deps.
Create a cc_library target with multiple C++sources+headers and no deps.
Create a cc_library target with both C and C++ sources+headers and no deps.
Create a cc_library target with Header-only and no deps.

Now create the following additional targets, using the previously created targets as deps where appropriate. Do not change the existing targets. Place any additional headers in the same folder as the BUILD.bazel and do not use strip_include_prefix.

# Direct deps
Create a cc_library target shim library (no sources or headers) which depends on a C++ source+header library.
Create a cc_library target shim library (no sources or headers) which depends on a Header-only library.
Create a cc_library target with C source+header which depends on a C source+header library.
Create a cc_library target with C++ source+header which depends on a C++ source+header library.
Create a cc_library target with C++ source+header which depends on a C source+header library.
Create a cc_library target with C++ source+header which depends on a Multiple sources+headers library.
Create a cc_library target with C++ source+header which depends on a Combined language sources+headers library.
Create a cc_library target with C++ source+header which depends on a Header-only library.
Create a cc_library target with Header-only which depends on a C++ source+header library.
Create a cc_library target with Header-only which depends on a Header-only library.

Now create the following additional targets, using the previously created targets as deps where appropriate. Do not change the existing targets. Place any additional headers in the same folder as the BUILD.bazel and do not use strip_include_prefix.

# Transitive deps
Create a cc_library target shim library (no sources or headers) which depends on a C++ source+header library which depends on a C++ source+header library.
Create a cc_library target C source+header which depends on a C source+header library which depends on a C source+header library.
Create a cc_library target C++ source+header which depends on a Shim library which depends on a C++ source+header library.
Create a cc_library target C++ source+header which depends on a Shim library which depends on a Header only library.
Create a cc_library target C++ source+header which depends on a C source+header library which depends on a Asm source+header library.
Create a cc_library target C++ source+header which depends on a C++ source+header library which depends on a C source+header library.
Create a cc_library target C++ source+header which depends on a C++ source+header library which depends on a C++ source+header library.
Create a cc_library target C++ source+header which depends on a C++ source+header library which depends on a Multiple sources+headers library.
Create a cc_library target C++ source+header which depends on a C++ source+header library which depends on a Combined language sources+headers library.
Create a cc_library target C++ source+header which depends on a C++ source+header library which depends on a Header only library.
Create a cc_library target C++ source+header which depends on a C++ source+header library which depends on a Import (pre-compiled) library.
Create a cc_library target C++ source+header which depends on a Header-only library which depends on a Header only library.
Create a cc_library target Header-only which depends on a Shim library which depends on a Header only library.
Create a cc_library target Header-only which depends on a Header-only library which depends on a C++ source+header library.
Create a cc_library target Header-only which depends on a Header-only library which depends on a Header only library.