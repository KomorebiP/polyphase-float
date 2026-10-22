# Building Polyphase Float

Commands in this guide run from the source directory unless stated otherwise.
Use CMake for installation and downstream package consumption.

## Requirements

The supported baseline is Linux x86-64 ELF with the LP64 data model
(64-bit pointers and long integers) and 64-bit GMP limbs. It requires:

- CMake 3.23 or newer and a build tool such as Make or Ninja;
- a C11 compiler, with GCC and Clang covered by the qualification matrix;
- official GMP development headers and library;
- POSIX threads and the platform math library.

OpenMP is optional. Official MPFR development files are needed for the optional
adapter and full differential tests. Python 3 is needed for source audits and
Python-based tests. Clang and its libFuzzer runtime are needed for fuzzing.
The production core does not require MPFR.

## First build

Build a portable serial library and the examples:

```sh
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_EXAMPLES=ON
cmake --build build/release -j
./build/release/example_basic
```

Both shared and static libraries are enabled by default. The basic example
prints the same exact value stored at two different precisions and shows
checked initialization, operations, and cleanup. Its source is
[`examples/basic.c`](../examples/basic.c).

Use `-DPPF_BUILD_SHARED=OFF -DPPF_BUILD_STATIC=ON` for a static-only build. Keep
at least one library form enabled. Configure a new build directory when changing
compilers or instrumentation to avoid carrying incompatible cached settings.

## CMake options

| Option | Default | Meaning |
|---|---|---|
| `PPF_BUILD_SHARED` | `ON` | Build the shared core library |
| `PPF_BUILD_STATIC` | `ON` | Build the static core library |
| `PPF_OPENMP` | `AUTO` | `AUTO`, `ON`, or `OFF` for OpenMP |
| `PPF_MPFR_SUPPORT` | `AUTO` | `AUTO`, `ON`, or `OFF` for adapter and differential support |
| `PPF_BUILD_EXAMPLES` | top-level `ON` | Build examples |
| `PPF_BUILD_BENCHMARKS` | `OFF` | Build benchmark smoke targets |
| `PPF_BUILD_FUZZERS` | `OFF` | Build Clang libFuzzer targets |
| `PPF_ENABLE_NATIVE_OPTIMIZATIONS` | `OFF` | Add `-march=native` when supported |
| `PPF_WARNINGS_AS_ERRORS` | `OFF` | Treat project warnings as errors |
| `PPF_ENABLE_THREAD_SANITIZER` | `OFF` | Instrument a serial build with ThreadSanitizer |
| `PPF_ELF_SYMBOL_VERSIONING` | `AUTO` | `AUTO`, `ON`, or `OFF` for Linux ELF symbol versions |
| `PPF_PRODUCT_PROFILE_HEADER` | empty | Optional generated planner profile |
| `BUILD_TESTING` | CTest default | Build validation targets |

`PPF_OPENMP=AUTO` uses OpenMP when the active compiler passes the probe and
otherwise selects the serial path. `ON` requires OpenMP; `OFF` excludes its
runtime from exported dependencies. With OpenMP enabled, choose a worker count
appropriate for the application, for example through `OMP_NUM_THREADS`.

`PPF_MPFR_SUPPORT=ON` requires MPFR. `AUTO` enables it when found, and `OFF`
builds the independent core only. A compatibility header permits some local
validation builds with an MPFR shared library but no development header; that
configuration does not export an install-tree adapter. Use official headers
for release validation and adapter installation.

Native compiler tuning is disabled by default so the build does not require
the build machine's instruction set. Enable it only for machines compatible
with the selected compiler target.

## Strict full validation

```sh
cmake -S . -B build/validation \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPPF_OPENMP=AUTO \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_EXAMPLES=ON \
  -DPPF_BUILD_BENCHMARKS=ON \
  -DPPF_WARNINGS_AS_ERRORS=ON
cmake --build build/validation -j
ctest --test-dir build/validation --output-on-failure -j1
```

This runs the suite configured on this machine. See [Testing](TESTING.md) for
focused labels and [Qualification](QUALIFICATION.md) for the full release
matrix across compilers, OpenMP, sanitizers, and fuzzers.

## Installation

```sh
cmake --install build/release --prefix "$HOME/.local"
```

To stage a system package under a temporary directory:

```sh
DESTDIR="$PWD/stage" cmake --install build/release --prefix /usr
```

Relative install directories support relocation of the complete prefix,
including nested library directories such as `lib/x86_64-linux-gnu`.
Absolute `CMAKE_INSTALL_LIBDIR` or `CMAKE_INSTALL_INCLUDEDIR` values retain
those explicit locations. The CMake package is installed below
`${libdir}/cmake/PolyphaseFloat`; the `pkg-config` file computes its prefix
relative to its own installed location.

### Verify the installation

From the source directory, build the supplied consumer against your prefix:

```sh
cmake -S tests/install_consumer -B build/install-consumer \
  -DCMAKE_PREFIX_PATH="$HOME/.local" \
  -DPPF_CONSUMER_COMPONENTS=core
cmake --build build/install-consumer -j
ctest --test-dir build/install-consumer --output-on-failure
```

This uses the installed header and library, not an in-tree library target.

## Downstream CMake use

Create a `CMakeLists.txt` in your application directory:

```cmake
cmake_minimum_required(VERSION 3.23)
project(my_app LANGUAGES C)
find_package(PolyphaseFloat CONFIG REQUIRED COMPONENTS core)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE PolyphaseFloat::shared)
```

For a first application, copy `examples/basic.c` from the source tree to your
application directory as `main.c`. It includes `<polyphase_float.h>`. Then,
from your application directory:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"
cmake --build build
./build/my_app
```

Use `PolyphaseFloat::static` for the static library. The chosen target must
have been enabled when Polyphase Float was built. GMP and Threads propagate
through both targets; OpenMP propagates only when enabled in the library.

`COMPONENTS core` avoids loading the optional MPFR adapter. An unqualified
`find_package` loads the core and any installed adapter. Request
`COMPONENTS mpfr` to require the adapter; this also loads the core and exports
`PolyphaseFloat::mpfr_shared` and/or `PolyphaseFloat::mpfr_static` according to
the installed build. A required adapter component fails configuration if the
adapter was not installed.

Compatible 0.x releases in ABI epoch 1 accept CMake minimum-version requests
from 0.11.0 onward. For example, 0.20.0 satisfies
`find_package(PolyphaseFloat 0.11 CONFIG REQUIRED COMPONENTS core)`.
Requests for an interface before 0.11.0 or a different major series are outside
this package-version policy. Exact-version requests still require the named
version.

## ABI-governed Linux build

Shared release libraries on Linux use ELF symbol versioning. The default
`PPF_ELF_SYMBOL_VERSIONING=AUTO` enables it on Linux; require it explicitly with:

```sh
cmake -S . -B build/abi \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_ELF_SYMBOL_VERSIONING=ON \
  -DPPF_MPFR_SUPPORT=OFF
cmake --build build/abi -j
cmake --build build/abi --target abi-contract-audit
```

`OFF` supports unusual toolchains and static-only environments, but such a
shared library is outside the governed Linux release artifact. See
[ABI policy](ABI_POLICY.md) for epoch 1 and the retained `.so.0` SONAME.

## ThreadSanitizer and fuzz builds

A serial ThreadSanitizer build is available through presets:

```sh
cmake --preset tsan
cmake --build --preset tsan -j
ctest --preset tsan
```

It requires `PPF_OPENMP=OFF`. Qualify OpenMP separately so the focused race
checks exercise Polyphase's POSIX-thread and cache ownership model.

To build the parser and PPFIF fuzzers:

```sh
cmake -S . -B build/fuzz \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_SHARED=OFF \
  -DPPF_BUILD_STATIC=ON \
  -DBUILD_TESTING=OFF \
  -DPPF_BUILD_EXAMPLES=OFF \
  -DPPF_BUILD_BENCHMARKS=OFF \
  -DPPF_BUILD_FUZZERS=ON
cmake --build build/fuzz -j
```

[Qualification](QUALIFICATION.md) provides timed seeded runs with writable
corpora and crash output outside the source tree.

## Troubleshooting

| Symptom | Action |
|---|---|
| GMP cannot be found | Install its development package, including headers, for the active compiler and architecture. |
| MPFR is unavailable | Use `PPF_MPFR_SUPPORT=OFF` for the core, or install official MPFR development files for validation and adapter installation. |
| OpenMP configuration fails | Use `PPF_OPENMP=OFF` for a serial build, or install the compiler's matching OpenMP runtime. |
| A downstream `find_package` fails | Set `CMAKE_PREFIX_PATH` to the installation prefix, not the source tree. Confirm that installation completed. |
| A core consumer unexpectedly needs MPFR | Request `COMPONENTS core` explicitly. |
| The program cannot load a shared library | Make the installed library directory available to the platform loader; CMake's build-tree runtime path does not replace an application's deployment policy. |
| Changing compiler or sanitizer produces inconsistent results | Configure a fresh build directory. |
| Fewer tests appear than expected | Check the CMake summary, MPFR support, and optional target settings; list tests with `ctest --show-only`. |

The Makefile remains available for development and research harnesses, including
`make test`, `make audit`, and named `bin/` targets. CMake is the canonical
installation route. [Historical reports](HISTORY_INDEX.md) describe earlier
harness configurations.
