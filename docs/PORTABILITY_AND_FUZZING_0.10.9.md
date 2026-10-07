# Optional OpenMP and fuzzing in Polyphase Float 0.10.9

## OpenMP policy

OpenMP is an execution capability rather than a numerical dependency.
CMake accepts:

```text
PPF_OPENMP=AUTO
PPF_OPENMP=ON
PPF_OPENMP=OFF
```

`AUTO` uses OpenMP when the active C toolchain can compile and link it. If the
probe fails, the build falls back to a one-worker serial implementation. `ON`
requires a usable implementation and stops configuration otherwise. `OFF`
never requests the OpenMP compiler or runtime.

The Makefile accepts the same values:

```sh
make PPF_OPENMP=AUTO
make PPF_OPENMP=ON
make PPF_OPENMP=OFF
```

## Serial compatibility layer

`src/ppf_openmp_compat.h` contains the only direct production inclusion of
`omp.h`. With OpenMP disabled, the small runtime calls reduce to deterministic
one-worker functions and the directive wrapper expands to ordinary structured
C. Exact product reconstruction and the common settlement layer are shared
between serial and parallel configurations.

## Installed metadata

An OpenMP build exports the OpenMP dependency through its CMake package and
static `pkg-config` metadata. A serial build omits that dependency. Downstream
serial consumers therefore do not need an OpenMP compiler or runtime.

## Fuzzing targets

Clang builds can enable:

```sh
cmake -S . -B build/fuzz \
  -DCMAKE_C_COMPILER=clang \
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

The available targets are:

```text
fuzz_text_parser
fuzz_fpif_import
```

Both targets use libFuzzer with AddressSanitizer and
UndefinedBehaviorSanitizer. Seed corpora are included under `fuzz/corpus`.

### Immutable seed corpora

The release seed directories are inputs, not campaign output directories. The
first corpus argument passed to libFuzzer must be a separate mutable working
directory. This prevents a campaign from adding generated files to the source
release.

Example bounded campaigns:

```sh
mkdir -p build/fuzz-corpus/text build/fuzz-corpus/fpif

ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
./build/fuzz/fuzz_text_parser \
  build/fuzz-corpus/text fuzz/corpus/text -runs=100000

ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
./build/fuzz/fuzz_fpif_import \
  build/fuzz-corpus/fpif fuzz/corpus/fpif -runs=100000
```

The release-validation campaigns completed 100,000 executions for each target
without a sanitizer diagnostic or crash. This is bounded evidence, not a claim
of sustained continuous fuzzing.

## Supported boundary

The validated local build matrix includes GCC OpenMP, GCC serial, Clang serial,
installed CMake consumers in both modes, and Clang sanitizer/fuzzer builds.
Native Windows, macOS, ARM64, big-endian, and 32-bit validation remain open.
