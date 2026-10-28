<!-- SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors -->
<!-- SPDX-License-Identifier: LGPL-3.0-or-later -->

# Reproducing release qualification

`tools/qualify_release.py` runs the supported Linux qualification matrix and
records its actual execution. It uses only the Python standard library and
does not install packages or access the network. Install the supported GCC,
Clang, CMake, CTest, Make, pkg-config, GMP and MPFR development packages before
running it. Clang's libFuzzer, AddressSanitizer and UndefinedBehaviorSanitizer
runtimes are needed for fuzzing. Use official GMP and MPFR headers.

From a clean source extraction, run:

```sh
python3 tools/qualify_release.py --output ../polyphase-qualification --with-sanitizers
```

The three required configurations are strict GCC serial, strict Clang serial,
and strict GCC/OpenMP with four workers. Each enables MPFR and runs its complete
configured CTest suite, including installed-consumer checks. The sanitizer
configuration runs the `hardening` CTest label under GCC ASan and UBSan,
except the Python `ctypes` export check, which would load the instrumented
library into an unsanitized Python process. All three ordinary compiler
suites run that export check. The sanitizer configuration retains the
compiled C ABI/runtime tests.

The separate fuzz configuration instruments the static production core with
Clang. It does not build the MPFR adapter, shared library, ordinary tests or
examples. Text-parser and PPFIF-import campaigns execute concurrently for
600 seconds per target by default. Their fixed seeds are distinct. The first
corpus directory is mutable and lives under the output directory. The source
corpora are additional input directories and are never the writable outputs.
Crash artifacts also remain outside the source tree.

Inspect the available scopes or print a command plan without building:

```sh
python3 tools/qualify_release.py --list
python3 tools/qualify_release.py --output ../polyphase-qualification --with-sanitizers --plan
```

Select a bounded subset or a longer campaign explicitly:

```sh
python3 tools/qualify_release.py --output ../polyphase-qualification --only gcc-serial
python3 tools/qualify_release.py --output ../polyphase-qualification --only asan
python3 tools/qualify_release.py --output ../polyphase-qualification --only fuzz --fuzz-seconds 3600
python3 tools/qualify_release.py --output ../polyphase-qualification --skip-fuzz
```

Every invocation creates a new run directory. It preserves commands, exit
codes, elapsed times, tool and dependency details, environment overrides and
logs. `qualification.json` summarizes only the scopes completed by that
invocation. Unselected scopes and unavailable optional checks remain
`NOT_RUN`. An unavailable required compiler or a failed selected build/test
produces a failing exit code. `--skip-fuzz` never records a fuzz pass.
The shorter `--only gcc`, `--only clang` and `--only openmp` names are aliases
for the corresponding strict configurations. Multiple scopes can follow one
`--only` option.

`source_snapshot.json` binds the included source files to SHA-256 hashes.
The runner compares the source before and after execution and fails if it
changes. Keep the source frozen while qualification runs. The output directory
must be outside the source tree, which keeps evidence, generated corpora and
build files out of source archives.

LeakSanitizer is disabled by default with `detect_leaks=0` because some
containers cannot support its process introspection. This limit is recorded
explicitly. Use `--detect-leaks 1` on a host where it works. ASan and UBSan
remain enabled in either case. Project allocator checks do not establish
recoverable allocation failure inside GMP.

The receipt describes local bounded execution. It does not establish
independent external replication, universal correct rounding, performance
improvements, coverage of omitted scopes, or correctness beyond the documented
contracts and exercised inputs.

[Documentation](README.md) · [Building](BUILDING.md) · [Testing](TESTING.md) · [Release process](RELEASE_PROCESS.md)
