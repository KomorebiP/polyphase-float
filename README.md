# Polyphase Float

[![CI](https://github.com/KomorebiP/polyphase-float/actions/workflows/ci.yml/badge.svg)](https://github.com/KomorebiP/polyphase-float/actions/workflows/ci.yml)
[![ABI governance](https://github.com/KomorebiP/polyphase-float/actions/workflows/abi.yml/badge.svg)](https://github.com/KomorebiP/polyphase-float/actions/workflows/abi.yml)
[![CodeQL](https://github.com/KomorebiP/polyphase-float/actions/workflows/codeql.yml/badge.svg)](https://github.com/KomorebiP/polyphase-float/actions/workflows/codeql.yml)
[![License: LGPL-3.0-or-later](https://img.shields.io/badge/license-LGPL--3.0--or--later-blue.svg)](LICENSE.md)

Polyphase Float is a C11 library for arbitrary-precision binary floating-point
arithmetic, built on GMP. It provides arithmetic, roots, elementary and selected
special functions, constants, conversion, formatting, and binary interchange
through the `ppf_*` API. Precision and rounding are controlled explicitly.

**Version 0.20.0** supports Linux x86-64 ELF with the LP64 data model and
64-bit GMP limbs. It retains ABI epoch 1 and `libpolyphase_float.so.0`. The production core
requires GMP and POSIX threads. OpenMP is optional; MPFR is used only by the
optional interoperability adapter, differential tests, and matched benchmarks.
This is a pre-1.0 release with a documented numerical and resource contract.

## Build and run

Install a C11 compiler (GCC or Clang), CMake 3.23 or newer, and the official GMP
development headers and library. Run these commands from the source directory:

```sh
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_EXAMPLES=ON
cmake --build build/release -j
./build/release/example_basic
```

This builds shared and static libraries and runs a small example without an
MPFR or OpenMP runtime dependency. Read `examples/basic.c` to follow object
initialization, checked operations, output, and cleanup. The other examples
cover arithmetic, constants, transcendental functions, and I/O.

Set `PPF_OPENMP=AUTO` to use OpenMP when available, or `ON` to require it.
See [Building](docs/BUILDING.md) for all options, static-only builds, package
consumption, and troubleshooting.

## Install and use in your project

```sh
cmake --install build/release --prefix "$HOME/.local"
```

In your application's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.23)
project(my_app LANGUAGES C)
find_package(PolyphaseFloat CONFIG REQUIRED COMPONENTS core)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE PolyphaseFloat::shared)
```

For a first application, copy `examples/basic.c` to your application directory
as `main.c`; it includes `<polyphase_float.h>`. Configure that directory with `cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"`,
then run `cmake --build build`. Use `PolyphaseFloat::static` if you prefer the
static library. The targets propagate required dependencies.

The installation also supplies `polyphase-float.pc` for `pkg-config`. Request
CMake component `mpfr` only when your application needs the installed MPFR
adapter. [Building](docs/BUILDING.md) includes an executable installed-consumer
check and details of shared-library discovery.

## Working with the API

Initialize an explicit context and each floating-point object before use.
Precision is measured in **bits**. The destination object's precision controls
the precision of a numerical result; passing a context does not resize it.
Clear objects and other owned resources before clearing their context.

Many numerical operations return a ternary value: zero for an exact result,
negative when the stored result is below the exact value, and positive when it
is above. **Check for `PPF_TERNARY_ERROR` before interpreting the sign.** Other
functions return status codes, predicates, counts, or pointers; their return
contracts are listed in the [semantic reference](docs/PUBLIC_SEMANTIC_CONTRACT_0.20.0.md).
Context exception flags are sticky diagnostics and can remain set after a
successful operation.

Use the [aliasing contract](docs/ALIASING_CONTRACT_0.20.0.md) before reusing an
input as an output. Threads may share read-only inputs, but mutable objects and
explicit contexts need exclusive ownership or application locking; see
[Thread safety](docs/THREAD_SAFETY.md).

## Numerical and resource contract

The [release scope](docs/COMPLETION_SCOPE_0.20.0.md) states the supported input
and resource limits, including finite Airy inputs, integer Bessel orders,
trigonometric reduction, certification retries, and PPFIF import size.
Check operation results when inputs approach these limits. Numerical rejection
and allocation failure have different output contracts.

The current reference indexes 437 core functions and classifies their return
values, ownership, aliasing, and failure behavior. It records 192 transactional
allocation contracts and no remaining indexed allocation-evidence gaps.
GMP-managed storage and I/O side effects remain outside project-allocation
rollback. See the [allocation reference](docs/ALLOCATION_COMPLETION_0.20.0.md)
and [PPFIF resource contract](docs/FPIF_RESOURCE_CONTRACT.md).

## Validate a build

Install official MPFR development files to enable the adapter and differential
checks, then build a separate validation tree:

```sh
cmake -S . -B build/validation \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_EXAMPLES=ON \
  -DPPF_BUILD_BENCHMARKS=ON \
  -DPPF_WARNINGS_AS_ERRORS=ON
cmake --build build/validation -j
ctest --test-dir build/validation -L fast --output-on-failure
```

[Testing](docs/TESTING.md) explains the available labels and complete test run.
For release qualification, use [the reproducible qualification procedure](docs/QUALIFICATION.md),
which records compiler, installed-consumer, sanitizer, and timed fuzz results
against an exact source snapshot. Its receipt distinguishes passes, failures,
and checks that were not run.

## Documentation and project information

| Need | Reference |
|---|---|
| Build, install, or resolve a configuration problem | [Building](docs/BUILDING.md) |
| Understand supported inputs and failure behavior | [Release scope](docs/COMPLETION_SCOPE_0.20.0.md) |
| Look up API and aliasing classifications | [Semantic contract](docs/PUBLIC_SEMANTIC_CONTRACT_0.20.0.md), [aliasing](docs/ALIASING_CONTRACT_0.20.0.md) |
| Check binary compatibility | [ABI policy](docs/ABI_POLICY.md) |
| Reproduce validation | [Testing](docs/TESTING.md), [qualification](docs/QUALIFICATION.md) |
| Assess a numerical or performance claim | [Claims and evidence](docs/CLAIMS_AND_EVIDENCE.md) |
| Prepare a release | [Release process](docs/RELEASE_PROCESS.md) |
| Read changes and earlier experiments | [Release notes](RELEASE_NOTES.md), [history index](docs/HISTORY_INDEX.md) |

The source is maintained at [KomorebiP/polyphase-float](https://github.com/KomorebiP/polyphase-float)
and distributed under [LGPL-3.0-or-later](LICENSE.md). See
[Contributing](CONTRIBUTING.md), [Support](SUPPORT.md), [Security](SECURITY.md),
and [third-party notices](THIRD_PARTY_NOTICES.md). The [roadmap](ROADMAP.md)
records future work and the independent review and reproduction requirements
for a later 1.0 promotion.
