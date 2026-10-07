# Polyphase Float

[![CI](https://github.com/KomorebiP/polyphase-float/actions/workflows/ci.yml/badge.svg)](https://github.com/KomorebiP/polyphase-float/actions/workflows/ci.yml)
[![ABI governance](https://github.com/KomorebiP/polyphase-float/actions/workflows/abi.yml/badge.svg)](https://github.com/KomorebiP/polyphase-float/actions/workflows/abi.yml)
[![CodeQL](https://github.com/KomorebiP/polyphase-float/actions/workflows/codeql.yml/badge.svg)](https://github.com/KomorebiP/polyphase-float/actions/workflows/codeql.yml)
[![License: LGPL-3.0-or-later](https://img.shields.io/badge/license-LGPL--3.0--or--later-blue.svg)](LICENSE.md)

Polyphase Float is an independent arbitrary-precision binary floating-point
research library built on GMP. It implements an MPFR-shaped numerical surface
under the `ppf_*` namespace while keeping MPFR outside the production library.
MPFR is optional and is used only by the interoperability adapter, differential
tests, and matched benchmarks.

The canonical source repository is
[`KomorebiP/polyphase-float`](https://github.com/KomorebiP/polyphase-float).

The current source is **version 0.18.0** and is a public pre-1.0 development
release. Version 0.18.0 closes project-controlled allocation atomicity for 56
public arithmetic, fused/planned-operation, and root endpoints. Common-
precision single-result calls use inline staging, larger results use
allocator-backed staging, and `ppf_div_with_plan_many` commits its complete
numerical and ternary batch only after every element succeeds. Supported
in-place calls and caller-managed external significands retain their existing
aliasing and ownership rules. See
`docs/ARITHMETIC_FUSED_ROOT_ALLOCATION_CONTRACT_0.18.0.md` and
`docs/ARITHMETIC_FUSED_ROOT_ALLOCATION_HARDENING_VALIDATION_0.18.0.md`.

Version 0.17.0 closed the transcendental and special-function allocation
transaction block. See
`docs/TRANSCENDENTAL_SPECIAL_ALLOCATION_CONTRACT_0.17.0.md` and
`docs/TRANSCENDENTAL_SPECIAL_ALLOCATION_HARDENING_VALIDATION_0.17.0.md`.

Version 0.16.0 closed the certified-constant allocation and cache transaction
block. See `docs/CONSTANT_ALLOCATION_CONTRACT_0.16.0.md` and
`docs/CONSTANT_ALLOCATION_HARDENING_VALIDATION_0.16.0.md`.

Version 0.15.0 closed the bounded text and interchange allocation block. See
`docs/IO_ALLOCATION_CONTRACT_0.15.0.md` and
`docs/IO_ALLOCATION_HARDENING_VALIDATION_0.15.0.md`.

Version 0.14.0 established the complete function-by-function aliasing contract.
All 437 public core functions have an explicit aliasing classification and the
reviewed ledger contains zero unverified aliasing entries. See
`docs/ALIASING_CONTRACT_0.18.0.md` and
`docs/ALIASING_CONTRACT_VALIDATION_0.18.0.md`.

Version 0.12.0 established **C ABI epoch 1** for the supported Linux ELF
LP64/GMP64 baseline while preserving the public 0.11.0 binary interface. It
adds a normalized source contract, runtime layout introspection,
ELF symbol versions, exact export governance, and a binary-consumer test that
runs an unchanged program linked against 0.11.0 with the 0.12.0 shared library.
The compatibility boundary is documented in `docs/ABI_POLICY.md` and validated
in `docs/ABI_STABILITY_VALIDATION_0.12.0.md`.

Version 0.11.0 completed the bounded POSIX-thread safety and cache-lifecycle
block. It defines the supported ownership model, adds automatic thread-exit
cleanup for default contexts and Toom workspaces, removes a special-constant
descriptor race, validates concurrent global-cache purge, and adds dedicated
ThreadSanitizer support. See `docs/THREAD_SAFETY.md` and
`docs/THREAD_SAFETY_VALIDATION_0.11.0.md`.

Version 0.10.9 completed the bounded allocation and portability hardening block.
It adds a dedicated allocation-failure flag, deterministic fail-at-N testing,
transactional destination preservation for covered context-owned paths,
optional OpenMP `AUTO/ON/OFF` builds, and Clang fuzzing targets for text and
PPFIF input. See `docs/ALLOCATION_FAILURE_CONTRACT.md`,
`docs/ALLOCATION_FAILURE_VALIDATION_0.10.9.md`, and
`docs/PORTABILITY_AND_FUZZING_0.10.9.md`.

Version 0.10.7 completed the first machine-profiled AUTO planner block. The
portable planner remains conservative, while an optional generated profile can
select an existing exact product graph only when independent close and spread
campaigns clear the configured evidence gates. See
`docs/PRODUCT_PLANNER_PROFILES.md` and
`docs/PRODUCT_AND_FUSED_PLANNER.md`.

Version 0.10.6 completed the counterbalanced benchmark-evidence layer.
Publication campaigns can counterbalance close and spread placement in
randomized paired blocks, estimate binding and run-order effects separately,
and compare candidates against a frozen baseline through an automated
regression gate. See `docs/BENCHMARK_COUNTERBALANCING.md`,
`docs/BENCHMARK_REGRESSION_GATES.md`, and
`docs/BENCHMARK_EVIDENCE_PIPELINE.md`.

Version 0.10.5 introduced complete timing-sample retention, process-indexed
resume, environment fingerprints, process-weighted summaries, and hierarchical
bootstrap intervals. See `docs/BENCHMARK_EVIDENCE_VALIDATION_0.10.5.md` and
`docs/CANONICAL_BASELINE.md`.

Versions 0.10.1 through 0.10.4 contain the strict-format, official-header,
precision-type, and optional-format compatibility corrections documented under
`docs/`.

## Current status

| Item | Status |
|---|---:|
| Lowercase reference inventory counterparts | 349 / 349 direct or conditional |
| Exported core `ppf_*` functions | 437 |
| Production MPFR imports | 0 by the supplied independence audit |
| Primary implementation language | C11 |
| Required arithmetic dependency | GMP |
| Parallel runtime | OpenMP optional through `AUTO`, `ON`, or `OFF` |
| Required threading baseline | POSIX threads |
| Optional validation/interoperability dependency | MPFR |
| Public ABI | Epoch 1 governed on Linux ELF LP64/GMP64 |
| Public semantic ledger | 437 / 437 functions indexed; gaps retained explicitly |
| Verified transactional public functions | 213 |
| Remaining allocation-atomicity gaps | 81 |
| Aliasing contract | 437 / 437 functions classified; 0 unverified aliasing entries |
| License | LGPL-3.0-or-later |

Repository initialization, protected-branch setup, tagging, and release-asset
steps are recorded in [`docs/GITHUB_RELEASE.md`](docs/GITHUB_RELEASE.md).

“349/349” describes literal inventory coverage. It does not assert equal
maturity, performance, resource behavior, portability history, or binary
compatibility with MPFR. See `docs/CLAIMS_AND_EVIDENCE.md`.

## Numerical model

A regular finite value is represented as

\[
x=(-1)^s M 2^{e-p}, \qquad 2^{p-1}\le M<2^p.
\]

Each `ppf_t` stores its precision, normalized significand, exponent, sign,
category, allocator, and storage mode. A `ppf_context_t` carries the active
exponent range, default rounding mode, exception flags, allocator, and scratch
arena.

The public surface includes:

- object initialization, precision, assignment, parsing, and conversion;
- arithmetic, fused operations, exact sum and dot-product paths;
- rounding control, exception flags, exponent ranges, and neighboring values;
- roots, reusable division plans, logarithms, exponentials, and powers;
- trigonometric, hyperbolic, and selected special functions;
- certified constants including `pi`, `log(2)`, Euler's constant, and Catalan's
  constant;
- stream I/O, formatted output, and the independent `PPFIF01` interchange
  format;
- random distributions, custom storage, cache management, and build queries;
- experimental rank-adaptive product and polyphase constant kernels.

## Supported baseline

The release-hardening work in this archive validates the following baseline:

- Linux or WSL on a 64-bit system;
- GCC or Clang with C11 support;
- POSIX threads;
- optional OpenMP support;
- GMP development files;
- CMake 3.23 or newer;
- MPFR development files for the optional adapter and full differential tests.

Other platforms may work, but they are not yet part of the supported ABI claim.
POSIX-thread use remains part of the current platform baseline. OpenMP is isolated behind the optional execution layer.

## Build the independent library

### CMake

```sh
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=AUTO \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_EXAMPLES=ON
cmake --build build/release -j
```

This builds the independent shared and static libraries without MPFR. OpenMP is used when available. Add `-DPPF_OPENMP=OFF` for a guaranteed serial package or `-DPPF_OPENMP=ON` to require it.

### Build the adapter and differential tests

```sh
cmake -S . -B build/validation \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_EXAMPLES=ON \
  -DPPF_BUILD_BENCHMARKS=OFF
cmake --build build/validation -j
ctest --test-dir build/validation -L fast --output-on-failure
```

`PPF_MPFR_SUPPORT=AUTO` is also accepted. In `AUTO`, the adapter and MPFR-based
tests are enabled only when an MPFR library is found.

### Make development harness

The existing Makefile remains available because the PowerShell/WSL research
launchers call named `bin/` targets directly:

```sh
make -j"$(nproc)" all
make test
make audit
make coverage
```

CMake is the canonical installation and downstream-consumer build path. The
Makefile is retained as a compatibility and experiment harness.

## Test tiers

Tests are labeled so routine validation does not silently launch the slowest
special-function sweeps.

```sh
ctest --test-dir build/validation -L fast --output-on-failure
ctest --test-dir build/validation -L hardening --output-on-failure
ctest --test-dir build/validation -L semantic --output-on-failure
ctest --test-dir build/validation -L threading --output-on-failure
ctest --test-dir build/validation -L extended --output-on-failure
ctest --test-dir build/validation -L smoke --output-on-failure
```

- `fast`: ordinary pull-request and local development checks;
- `hardening`: deterministic allocation-failure, thread-lifecycle, and
  resource-state checks;
- `semantic`: generated contract verification and direct public-surface checks;
- `threading`: focused context isolation, cache lifecycle, and cleanup checks;
- `extended`: randomized, exhaustive, or large-precision checks;
- `smoke`: optional benchmark executable sanity checks.

See `docs/TESTING.md` for the exact classification and limitations.

## Install and consume

```sh
cmake --install build/release --prefix "$HOME/.local"
```

The installation provides:

- shared and static library targets when enabled;
- public headers;
- `polyphase-float.pc` for `pkg-config`;
- a CMake package with `PolyphaseFloat::shared` and
  `PolyphaseFloat::static` targets;
- license and core project documentation.

A downstream CMake project can use:

```cmake
find_package(PolyphaseFloat CONFIG REQUIRED)
add_executable(example main.c)
target_link_libraries(example PRIVATE PolyphaseFloat::shared)
```

An install-tree consumer test is included in `tests/install_consumer`.

## Release and source audits

```sh
python3 tools/release_audit.py
python3 tools/create_source_release.py --output-dir dist
```

The release audit checks required public files, version consistency, SPDX
coverage for source-like files, temporary/build artifacts, stale top-level
completion claims, API coverage, and optionally the production shared-library
independence boundary.

## Benchmarks

Performance claims in this project are operation-specific. A valid comparison
must match:

- destination precision and rounding mode;
- input construction and operand classes;
- worker and affinity resources;
- cold or warm cache state;
- timed boundaries;
- correctness and ternary-direction checks.

The included benchmark reports contain promising operation-specific results,
including strong large-precision multiplication and constant-computation rows.
They remain machine-specific until independently reproduced. Do not describe
them as proof that every `ppf_*` operation is faster than MPFR.

## Repository map

| Path | Purpose |
|---|---|
| `include/` | Public C headers |
| `src/` | Independent production implementation |
| `adapter/` | Optional MPFR interoperability boundary |
| `tests/` | Core and differential validation |
| `benchmarks/` | Development and comparison benchmarks |
| `examples/` | Small API examples |
| `docs/` | Current technical documentation and historical records |
| `scripts/` | Benchmark and validation drivers |
| `tools/` | Independence, release, and packaging audits |
| `experiments/` | Explicitly non-production experimental work |

## Project policy

- Repository: [github.com/KomorebiP/polyphase-float](https://github.com/KomorebiP/polyphase-float)
- License: `LICENSE.md`
- Contributions: `CONTRIBUTING.md`
- Security: `SECURITY.md`
- Governance: `GOVERNANCE.md`
- Roadmap: `ROADMAP.md`
- Third-party dependencies: `THIRD_PARTY_NOTICES.md`
- Citation metadata: `CITATION.cff`
- GitHub publication procedure: `docs/GITHUB_RELEASE.md`
- Hardening evidence: `docs/PUBLIC_RELEASE_HARDENING_REPORT.md`

## Important boundary

This repository contains a serious numerical implementation and substantial
validation infrastructure. It is still pre-1.0 software. Correctness evidence
is strongest for the domains exercised by the supplied exact, differential,
randomized, special-value, and range tests. External review, broader platform
coverage, long-running fuzzing, allocation-failure testing, and an ABI decision
remain necessary before a production-maturity claim.
