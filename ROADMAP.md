# Roadmap

Version 0.20.0 has a documented Linux x86-64 ELF LP64/GMP64 contract, governed ABI,
canonical CMake installation, and reproducible local qualification procedure.
See [Release scope](docs/COMPLETION_SCOPE_0.20.0.md) for the current supported
behavior and [History](docs/HISTORY_INDEX.md) for earlier development reports.

## Available foundations

- Shared and static C11 libraries, with optional MPFR adapter and OpenMP.
- Installed CMake targets and `pkg-config` metadata, including relocated consumers.
- ABI epoch 1 with source-contract, layout, export, symbol-version, and old-binary checks.
- A 437-function semantic ledger and complete indexed aliasing classifications.
- Allocation, initialization, and prefix-cleanup classifications with no remaining indexed allocation-evidence gaps.
- POSIX-thread ownership rules, thread-exit cleanup, and cache-lifecycle tests.
- Exact, differential, randomized, resource-failure, sanitizer, and seeded fuzz test paths.
- Deterministic source packaging and a qualification runner that binds evidence to source hashes.
- Retained-sample benchmark tooling and evidence-gated product-planner profiles.

## Version 1.0 promotion requirements

The following remain promotion requirements rather than claims made by the
0.20.0 version number:

- [ ] Make the final SONAME, package-version compatibility, and support-window decision while preserving or explicitly migrating the epoch-1 commitment.
- [ ] Establish sustained fuzzing and permanent regression-corpus governance.
- [ ] Document or prove every fixed iteration and argument-domain limit, with boundary evidence.
- [ ] Connect each certified numerical path to its written enclosure or remainder argument.
- [ ] Obtain independent review of core rounding and constant proofs and of the intended release claims.
- [ ] Obtain at least two independent correctness reproductions of the exact candidate and declared support matrix.

Local compiler and sanitizer runs contribute evidence but do not replace
independent review or external reproduction. A 1.0 release must also have a
frozen public contract, published limits, and successful qualification for its
declared platforms.

## Platform and safety expansion

- Extend governed ABI evidence beyond Linux x86-64 ELF LP64/GMP64.
- Add ARM64, native Windows, macOS, big-endian, and 32-bit evidence before making corresponding support claims.
- Expand hard-to-round, narrow-range, high-thread-count, and library-unload stress coverage.
- Continue longer sanitizer and fuzz campaigns and retain regressions in the corpus.

These platforms are outside the current contract. The scope of a future release
should state explicitly which platform-expansion goals it adopts.

## Performance work

- Improve serial logarithm, reciprocal square root, inverse-angle, hyperbolic, and special-function kernels.
- Complete higher-core product graphs and worker-aware scheduling.
- Resolve high-worker regressions before enabling aggressive defaults.
- Benchmark each numerical family over representative input regions.
- Obtain independent reproduction before promoting principal performance results as externally replicated.

Performance claims remain operation- and environment-specific under the
[claims policy](docs/CLAIMS_AND_EVIDENCE.md).
