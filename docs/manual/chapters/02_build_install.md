# Building, installing, and linking

The goal of this chapter is a working C program linked to Polyphase Float. Start with the serial core, verify it, and add optional facilities when your application needs them. Keep the source, build output, and installation prefix separate: that makes it easier to repeat a build or diagnose which library an application is using.

A **source directory** contains `CMakeLists.txt`, `include/`, and `src/`. A **build directory** contains generated files and compiled binaries. An **installation prefix** is the destination tree from which another project consumes the headers, libraries, and CMake package. Building does not automatically install the library.

## Prepare the system

The supported baseline is Linux x86-64 ELF, using LP64 and 64-bit GMP limbs. You need:

| Component | Purpose |
|---|---|
| C11 compiler, such as GCC or Clang | Compile the library and application |
| CMake 3.23 or newer | Configure, build, test, and install |
| Make or Ninja | Execute the generated build rules |
| Official GMP development headers and library | Build the core arithmetic library |
| POSIX threads and the platform math library | Provide system facilities used by the core |
| Python 3 | Run source audits and Python-based tests |

MPFR development files are optional for ordinary core use and required for the full differential-test and adapter route described below. A development package includes headers as well as a linkable library. Having only a runtime shared library is insufficient for a normal development installation.

On an Ubuntu system with package access, the following installs the basic development tools:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake libgmp-dev pkg-config python3
```

Check the installed tools before configuring:

```sh
cc --version
cmake --version
python3 --version
```

Use your distribution's supported package sources. Package versions differ by distribution release; the CMake version must meet the minimum above. The [Ubuntu package index](https://packages.ubuntu.com/) documents the packages available for each release.

### Windows: enter the Linux environment first

On a supported Windows installation, open PowerShell as an administrator and run:

```powershell
wsl --install
```

Follow the restart and Ubuntu first-run prompts, including creation of a Linux user account. Microsoft maintains the [WSL installation instructions](https://learn.microsoft.com/en-us/windows/wsl/install), including routes for systems that need additional setup. This bootstrap normally downloads components and is separate from building Polyphase Float.

After Ubuntu opens, run the Linux commands in this chapter **inside Ubuntu**, not in PowerShell. Use an x86-64 Linux environment and keep the project in its Linux home directory. This is a Linux build on a Windows host; it does not provide a native Windows DLL or establish support for an ARM machine. Verify the build and tests in your own WSL environment as you would on any other Linux installation.

### Preparing an offline build

The core build does not need to fetch a dependency from the network once the source archive, compiler, CMake, build tool, and GMP development files are present. Arrange those prerequisites before disconnecting. On a managed offline system, obtain the matching distribution packages and their dependencies through your administrator or an approved package cache. Copying only `libgmp.so` from another machine does not supply the headers or ensure an ABI-compatible development environment.

The package-install commands above require access to configured package repositories unless the necessary packages are already cached. They are preparation commands, not a promise of an offline package installation. After preparation, unpack the complete source archive into a writable directory and run the following build commands there.

## Build and run the core

From the directory containing `CMakeLists.txt`:

```sh
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_EXAMPLES=ON \
  -DBUILD_TESTING=ON
cmake --build build/release -j2
./build/release/example_basic
ctest --test-dir build/release --output-on-failure -j1
```

The trailing backslash continues a shell command on the next line; do not put characters after it. `-S .` selects the current source directory. `-B build/release` selects a separate build directory. `-j2` permits two build jobs; reduce it to `-j1` on a memory-constrained machine. The test command uses one test job to make failures easier to follow.

The basic example prints:

```text
x (256 bits) = 3.10000e1
y (80 bits)  = 3.10000e1
```

Both objects contain the integer 31 exactly. Their different precisions show that storage precision can differ without changing an exactly representable value. The file [`examples/basic.c`](../../../examples/basic.c) includes checked initialization, calculation, output, and cleanup.

This configuration builds the core without MPFR or an OpenMP runtime requirement. Both shared and static core libraries are enabled by default. The tests available in a configured build depend on its capabilities; a core-only test run is useful local validation, but it is not the full MPFR-enabled differential matrix.

If configuration fails, fix that error before running the build command. If compilation fails, read the first compiler error before later messages caused by it. A successful `cmake` configuration alone does not mean the library has compiled or its tests have run.

## Install to your user account

A user-local installation avoids requiring administrator privileges:

```sh
cmake --install build/release --prefix "$HOME/.local"
```

The prefix contains headers, libraries, package metadata, and documentation. Depending on the platform's installation conventions, the library directory can be `lib` or a nested directory such as `lib/x86_64-linux-gnu`. Use the exported CMake package instead of assuming that directory name in application build scripts.

Verify consumption from the installation, rather than accidentally linking back to the source build:

```sh
cmake -S tests/install_consumer -B build/install-consumer \
  -DCMAKE_PREFIX_PATH="$HOME/.local" \
  -DPPF_CONSUMER_COMPONENTS=core
cmake --build build/install-consumer -j2
ctest --test-dir build/install-consumer --output-on-failure
```

`CMAKE_PREFIX_PATH` tells CMake where to search for installed packages. It does not modify the system package manager. If you maintain several Polyphase Float installations, choose one prefix deliberately and use a fresh consumer build directory when switching between them.

## Create your own CMake application

Make an application directory alongside the library source directory. Place a copy of [`intro_first.c`](../../../examples/manual/intro_first.c) in it as `main.c`, and add this `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.23)
project(my_calculation LANGUAGES C)

find_package(PolyphaseFloat CONFIG REQUIRED COMPONENTS core)
add_executable(my_calculation main.c)
target_compile_features(my_calculation PRIVATE c_std_11)
set_target_properties(my_calculation PROPERTIES C_EXTENSIONS OFF)
target_link_libraries(my_calculation PRIVATE PolyphaseFloat::shared)
```

From that application directory:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"
cmake --build build -j2
./build/my_calculation
```

Expected output is `2 * 3 = 6.00000e0`. The imported target supplies the library's include paths and transitive link requirements. You do not need to copy headers into the application or hand-maintain GMP and thread link flags in this CMake project.

To select the installed static core, replace `PolyphaseFloat::shared` with `PolyphaseFloat::static`, provided that form was built and installed. Static linking still needs the relevant dependency libraries; the imported target expresses those requirements. The word `static` does not imply that every dependency in the final executable is statically linked.

`COMPONENTS core` makes the dependency choice explicit. An application using the optional MPFR adapter should request the `mpfr` component and use its documented adapter target. The independent core has no need for that component.

## Compile the introductory programs directly

For a quick experiment with the serial build above, run this from the library source directory:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic \
  -Iinclude examples/manual/intro_first.c \
  build/release/libpolyphase_float.a \
  -lgmp -lm -pthread -o build/release/intro_first
./build/release/intro_first
```

The source comes before the static library, and the dependency libraries follow it. That order matters to ordinary static linking. `-Iinclude` locates the public header; it does not locate the compiled library. For the other introductory programs, replace `intro_first` in both the source path and executable name.

This command is intentionally tied to the serial build in this chapter. An OpenMP-enabled static build can require additional compiler and runtime flags. For a maintained application, prefer the CMake imported target so that the selected build's dependencies travel with it.

## Add optional capabilities deliberately

Use a new build directory for a different compiler, sanitizer, or major configuration. CMake caches detected paths and compiler properties; reusing a directory after changing toolchains can produce confusing combinations.

For full differential testing on Ubuntu, first install official MPFR development files:

```sh
sudo apt-get install libmpfr-dev
```

Then configure a separate validation build:

```sh
cmake -S . -B build/validation \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPPF_OPENMP=AUTO \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_EXAMPLES=ON \
  -DPPF_WARNINGS_AS_ERRORS=ON \
  -DBUILD_TESTING=ON
cmake --build build/validation -j2
ctest --test-dir build/validation --output-on-failure -j1
```

`AUTO` enables OpenMP when the compiler probe succeeds; `ON` requires it; `OFF` excludes it. For MPFR, `ON` requires support and `OFF` selects the independent core. Use official MPFR headers when installing the adapter. A compatibility-header validation route does not provide the same installable adapter configuration.

Native CPU tuning is off by default. Keep it off when distributing a binary to machines with potentially different instruction sets. The complete option list, packaging staging commands, and tooling configurations are in [Building Polyphase Float](../../BUILDING.md); the test labels and qualification boundaries are in [Testing](../../TESTING.md).

## Diagnose the first failure

| Symptom | First action |
|---|---|
| `cmake` is not found or is too old | Install a suitable CMake package and check `cmake --version`. |
| GMP headers or library cannot be found | Install the development package; check that headers and library target the same architecture. |
| `PolyphaseFloatConfig.cmake` cannot be found | Install the library, then pass its prefix through `CMAKE_PREFIX_PATH`. |
| The compiler cannot find `polyphase_float.h` | Use the imported CMake target, or check `-Iinclude` and your current directory. |
| The linker reports undefined `ppf_` symbols | Confirm that the library is linked and matches the header; for direct static linking, check argument order. |
| The executable cannot load a shared library | Check the installation prefix and runtime library search configuration; do not substitute an unrelated shared object. |
| A test fails | Keep the `--output-on-failure` output and record compiler, configuration, and platform before changing settings. |

A useful problem report includes the first failing command, its full error message, the configured options, compiler version, and whether the failure occurs during configuration, compilation, linking, execution, or testing. Those stages have different remedies.

**Exercise.** Why can `#include "polyphase_float.h"` compile successfully while the final link fails?

**Answer.** The header provides declarations to the compiler. The linker still needs the compiled implementations and their dependencies. Linking `PolyphaseFloat::shared` or `PolyphaseFloat::static` through CMake supplies that second part.
