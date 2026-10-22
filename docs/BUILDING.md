# Building Polyphase Float

## Requirements

The validated source baseline is:

- CMake 3.23 or newer
- a C11 compiler
- GMP headers and library
- POSIX threads and a platform math library
- optional OpenMP C support
- optional MPFR headers and library for the adapter and differential tests
- Clang for the libFuzzer targets

The independent core does not require MPFR. OpenMP is optional in 0.12.0.

## CMake options

| Option | Default | Meaning |
|---|---:|---|
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

`PPF_OPENMP=AUTO` enables OpenMP when the active compiler passes the build
probe. It otherwise builds the serial path. `ON` fails configuration if OpenMP
is unavailable. `OFF` guarantees that no OpenMP dependency is exported.

`PPF_MPFR_SUPPORT=ON` fails configuration if MPFR cannot be found. `AUTO`
enables the adapter when available. `OFF` builds only the independent core.

## Portable OpenMP build

```sh
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=AUTO \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_ENABLE_NATIVE_OPTIMIZATIONS=OFF
cmake --build build/release -j
```

## Guaranteed serial build

```sh
cmake -S . -B build/serial \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF
cmake --build build/serial -j
```

The resulting shared library and installed package do not require an OpenMP
runtime.

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
ctest --test-dir build/validation --output-on-failure
```

Release validation should install and use the official MPFR development header.

## ThreadSanitizer build

```sh
cmake --preset tsan
cmake --build --preset tsan -j
ctest --preset tsan
```

The ThreadSanitizer configuration requires `PPF_OPENMP=OFF`. OpenMP-enabled
strict builds are validated separately so race reports describe Polyphase's
POSIX-thread and cache synchronization rather than an instrumented OpenMP
runtime.

## Fuzzer build

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

See `PORTABILITY_AND_FUZZING_0.10.9.md` for bounded campaign commands.

## Make modes

The research-harness Makefile accepts the same OpenMP policy:

```sh
make PPF_OPENMP=AUTO -j"$(nproc)" all
make PPF_OPENMP=OFF -j"$(nproc)" test
make PPF_OPENMP=ON -j"$(nproc)" test
```

CMake remains the canonical installation path.

## Installation

```sh
cmake --install build/release --prefix /desired/prefix
```

To stage a package:

```sh
DESTDIR="$PWD/stage" cmake --install build/release --prefix /usr
```

The generated `pkg-config` file computes its prefix relative to its installed
location, including nested library directories such as
`lib/x86_64-linux-gnu`. Relative install directories support relocation of the
complete prefix. Absolute `CMAKE_INSTALL_LIBDIR` or
`CMAKE_INSTALL_INCLUDEDIR` values retain those explicit absolute locations.
The exported CMake package is installed below
`${libdir}/cmake/PolyphaseFloat`.

## Downstream CMake use

```cmake
cmake_minimum_required(VERSION 3.23)
project(example C)
find_package(PolyphaseFloat CONFIG REQUIRED)
add_executable(example main.c)
target_link_libraries(example PRIVATE PolyphaseFloat::shared)
```

Use `PolyphaseFloat::static` for a static build. GMP and Threads are propagated.
OpenMP is propagated only when the installed library was built with it.

An unqualified `find_package` loads the core and any installed MPFR adapter,
preserving the full-package behavior. Applications that use only the
independent core can explicitly avoid the adapter dependency:

```cmake
find_package(PolyphaseFloat CONFIG REQUIRED COMPONENTS core)
target_link_libraries(example PRIVATE PolyphaseFloat::static)
```

Request `COMPONENTS mpfr` to require the installed adapter. The `mpfr`
component also loads the core and exports `PolyphaseFloat::mpfr_shared` and/or
`PolyphaseFloat::mpfr_static` according to the installed build. A required
adapter component fails configuration when the adapter was not installed.

Compatible 0.x releases in ABI epoch 1 accept CMake minimum-version requests
from 0.11.0 onward. For example, a newer compatible package satisfies
`find_package(PolyphaseFloat 0.11 CONFIG REQUIRED COMPONENTS core)`.
Requests for interfaces before 0.11.0 or a different major series are outside
that compatibility policy.

## ABI-governed Linux build

Release shared libraries on Linux should use the default
`PPF_ELF_SYMBOL_VERSIONING=AUTO` or require it explicitly:

```sh
cmake -S . -B build/abi \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_ELF_SYMBOL_VERSIONING=ON \
  -DPPF_MPFR_SUPPORT=OFF
cmake --build build/abi -j
cmake --build build/abi --target abi-contract-audit
```

`OFF` is available for non-ELF or unusual toolchains. A library built with it
is not the governed Linux release artifact.
