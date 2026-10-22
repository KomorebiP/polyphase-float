# Release notes


## 0.20.0 allocation closeout and integrated resource repair

- Integrate PPFIF payload/significand admission limits and bounded streaming.
- Reject Airy inputs above the supported bound using exact comparison.
- Validate floating-setter rounding modes before special-value mutation.
- Resolve 79 allocation-evidence entries with explicit source contracts and
  focused lifecycle and transaction tests.
- Add reviewed numerical boundaries, deterministic retry-exhaustion tests and
  an offline qualification runner.
- Preserve the remote repository's private CodeQL and temporary-build fixes.

The candidate preserves ABI epoch 1 and the existing SONAME. External
correctness replication and independent mathematical review remain open.
See `docs/COMPLETION_SCOPE_0.20.0.md` and `docs/QUALIFICATION.md`.

## 0.19.0 deep audit and corrected contracts — 2026-10-09

- Correct directed rounding when sign changes and precision shrinks.
- Preserve destinations and report allocation failure in precision rounding,
  scalar remainder, random generation, and relative difference.
- Fix partial initialization cleanup and extreme signed-integer arguments.
- Reject empty numeric text, validate rounding, and correct extreme-exponent
  range classification.
- Correct Gamma/log-Gamma poles, infinities, integer identities, Bessel signed
  zeros and interval construction, and special-function remainder enclosures.
- Fix multiarch pkg-config relocation and installed CMake component dependencies.
- Replace token-reference inference with explicit allocation evidence lists.
- Add focused regressions to both CMake and Make. Preserve epoch-1 declarations,
  layouts, symbol versions, and SONAME on the governed baseline.

See `docs/DEEP_AUDIT_0.19.0.md` and `docs/COMPLETION_SCOPE_0.19.0.md`. This is a
pre-1.0 release; API inventory completion is not a claim of universal correct
rounding or unrestricted numerical domains.

## 0.18.0 arithmetic, fused-operation, and root allocation transactions — 2026-08-31

- added transactional public wrappers for 56 arithmetic, fused/planned-operation, and root endpoints
- used fixed inline staging through 16 GMP limbs so ordinary arithmetic does not acquire a new heap-allocation requirement
- retained allocator-backed staging at larger precision for deterministic project-allocation failure control
- gave `ppf_div_with_plan_many` one batch-level commit boundary for every numerical destination and optional ternary output
- preserved supported in-place operations and caller-managed external significand ownership
- kept private uncommitted implementation names hidden while preserving every public symbol and declaration
- added complete fail-at-every-project-allocation sweeps for all 56 public endpoints in serial and OpenMP builds
- made the concurrent test allocator bookkeeping race-free with a C11 atomic lock
- reduced the generated allocation-atomicity gap count from 137 to 81
- raised verified transactional public functions from 157 to 213
- preserved ABI epoch 1 with no public symbol, declaration, type, layout, enum, or calling-contract change
- corrected the GitHub ABI install-path check to follow the installed `abi/contract-v1.json` location
- synchronized repository metadata, roadmap counts, optional-OpenMP wording, citation links, and deterministic archive timestamps for public GitHub publication

The numerical formulas, rounding rules, exponent-range behavior, product
planner, and established aliasing classifications are unchanged. GMP-internal
allocation remains outside the deterministic project-allocation contract.

## 0.17.0 transcendental and special allocation transactions — 2026-08-18

- staged results for 79 public logarithmic, exponential, power, trigonometric, hyperbolic, and special-function endpoints
- preserved destination allocator provenance, limb pointers, and application-managed external significands during commit
- made `ppf_sin_cos` and `ppf_sinh_cosh` commit both numerical outputs atomically
- staged the auxiliary sign output of `ppf_lgamma`
- added a private allocation-failure epoch and active transaction context for nested project work arrays
- routed fast transcendental arrays and gamma, digamma, and zeta Bernoulli arrays through allocator-provenance buffers
- preallocated serial and OpenMP trigonometric binary-splitting workspaces before numerical execution
- avoided unsigned addition overflow in trigonometric packet workspace sizing
- added deterministic coverage for all 79 endpoints, with complete fail-at-every-allocation sweeps on 12 representative routes
- added explicit aliased, paired-output, sign-output, external-storage, high-precision, OpenMP, and sticky-allocation-flag cases
- reduced the generated allocation-atomicity gap count from 216 to 137
- preserved ABI epoch 1 with no public symbol, declaration, type, layout, enum, or calling-contract change

The numerical formulas, interval constructions, rounding rules, and public
aliasing contract are unchanged. GMP-internal allocation remains outside the
deterministic project-allocation contract.

## 0.16.0 certified-constant allocation transactions — 2026-08-18

- staged all certified-constant destinations and committed only after successful interval rounding
- committed exact interval and rounded-result cache growth by swap
- added allocator-provenance work buffers for constant builders and the embedded pi engine
- preallocated log 2 worker scratch before entering OpenMP regions
- propagated nested Euler and Catalan allocation failures to the caller context
- added a thread-local project-allocation failure epoch to prevent nested contexts from swallowing failure evidence
- added deterministic fail-at-every-project-allocation coverage for all 24 public constant-family entries
- reduced the generated allocation-atomicity gap count from 228 to 216
- preserved ABI epoch 1 with no public symbol, declaration, type, layout, enum, or calling-contract change
- selected the exact non-adic Chudnovsky execution path for the public pi builder while leaving optional standalone p-adic paths outside this allocation contract
- synchronized publication and clearing of the embedded pi allocator context with every worker allocation callback
- kept `mpz_get_str` storage in the GMP allocator domain and released it through GMP's active deallocator
- corrected the errno-format sanitizer regression to release successful `ppf_asprintf` output with `free`, as required by its public ownership contract

The certified interval formulas and final endpoint-rounding rule are unchanged.
GMP-internal allocation remains outside the deterministic failure-injection
contract.

## 0.15.0 transactional text and interchange allocation — 2026-08-18

- made `ppf_set_str`, `ppf_strtofr`, `ppf_inp_str`, and `ppf_fpif_import`
  construct complete temporary values before committing initialized numerical
  destinations;
- made full-string parsing reject trailing junk without committing a valid
  numeric prefix, while preserving the documented prefix behavior of
  `ppf_strtofr`;
- made strings allocated by `ppf_get_str` retain allocator provenance and
  require release through `ppf_free_str`;
- moved formatting builders and digit copies to the resolved context allocator,
  preserving caller buffers and returned-pointer ownership on unrecovered
  allocation failure;
- staged `ppf_out_str`, formatted stream output, and PPFIF export before the
  first externally visible stream write;
- added deterministic fail-at-every-project-allocation coverage for parsing,
  returned strings, caller buffers, streams, and PPFIF interchange;
- retained ABI epoch 1 and the complete 437-function aliasing contract;
- changed no arithmetic, rounding, root, constant, transcendental, or special-
  function formula.

## 0.14.0 systematic aliasing contract — 2026-08-18

- generated a machine-readable aliasing contract and searchable CSV matrix for
  all 437 public core functions;
- reduced the semantic ledger's unverified aliasing count from 192 to zero;
- added an exhaustive compiled alias matrix covering all six rounding modes,
  ordinary in-place destinations, source-position overlap, all-sources-identical
  cases, paired outputs, auxiliary integer outputs, same-index batch overlap,
  initialization-only destinations, and required source/destination separation;
- made invalid same-output calls transactional for `ppf_modf`, `ppf_grandom`,
  and constant interval export;
- integrated alias-policy verification into CMake, Make, CI, installation, the
  release audit, and the generated semantic contract;
- preserved ABI epoch 1 with zero breaking changes relative to 0.13.0;
- changed no multiplication, rounding, root, constant, transcendental, or
  special-function formula.

## 0.13.0 generated public semantic contract — 2026-08-17

- generated one machine-readable semantic record for every one of the 437 core
  public functions;
- added a flat review matrix covering purpose, availability, return category,
  rounding, context, flags, aliasing, thread safety, ownership, failure
  atomicity, practical limits, evidence, and explicit gaps;
- added a curated function-purpose inventory and deterministic contract hash;
- added a direct core test for the 47 public names that previously had no
  direct test-source reference;
- added semantic-contract verification and policy tests to CMake and Make;
- installed the semantic JSON, CSV, and human-readable contract with the
  library documentation under a dedicated `semantics/` namespace;
- installed the ABI JSON under a separate `abi/` namespace, preventing the two
  independently governed `contract-v1.json` files from overwriting one another;
- recorded rather than concealed the remaining 192 aliasing, 232
  allocation-atomicity, 112 bounded-path, and six conditional-build gaps;
- preserved ABI epoch 1 without adding a shared-library symbol or changing a
  governed public structure.

No production numerical source file or numerical formula changed.

## 0.12.0 ABI epoch 1 and compatibility governance — 2026-08-17

- preserved the public 0.11.0 Linux ELF ABI and SONAME;
- froze a normalized source-contract baseline covering functions, public types,
  structures, enum values, and constants;
- added exact source-contract, runtime-layout, export, and symbol-version audits;
- added ABI epoch, revision, tag, contract-hash, and size-tagged runtime
  introspection functions;
- versioned inherited core symbols as `PPF_0.11` and additive introspection
  symbols as `PPF_0.12`;
- versioned MPFR adapter symbols independently as `PPF_MPFR_0.11`;
- added a binary compatibility test that compiles against the actual 0.11.0
  library and runs the unchanged executable with the 0.12.0 library;
- added CMake, Make, CI, installation, and release-audit integration for the ABI
  contract;
- documented the epoch-1 compatibility rules and breaking-change procedure.

The only production source added is `src/abi.c`. No existing production source
file and no numerical formula changed.

## 0.11.0 thread safety and cache lifecycle — 2026-08-17

- added `ppf_thread_cleanup()` and automatic POSIX thread-exit cleanup for the
  default context and direct-limb Toom workspaces;
- added `ppf_buildopt_thread_cleanup_p()` for cleanup-key capability queries;
- documented the ownership rules for contexts, objects, divisor plans, random
  states, global caches, OpenMP, and library unload;
- made Euler and Catalan constant descriptors immutable after one-time
  initialization, removing concurrent static writes;
- made local cache release avoid creating a default context solely to free an
  empty pool;
- made direct clearing of the current default context recreate documented
  defaults on later use;
- added concurrent cache purge, context isolation, shared read-only operand,
  shared divisor-plan, thread-exit, and post-cleanup reconstruction tests;
- added `PPF_ENABLE_THREAD_SANITIZER`, a CMake preset, a Make target, and a
  dedicated CI job;
- corrected arena-block metadata alignment while preserving exact custom
  allocator deallocation through the original pointer and allocation size;
- added explicit arena alignment tests through 128-byte boundaries.

The numerical formulas, exact product identities, final rounding algorithms,
certified constant formulas, elementary-function reductions, and
special-function formulas are unchanged.

## 0.10.9 allocation transaction and optional-execution hardening — 2026-08-17

- added `PPF_FLAG_ALLOC` and allocation-flag convenience functions
- routed covered core object, arena, arithmetic, divisor-plan, root, and PPFIF
  import allocations through context-owned helpers
- defined and tested byte-for-byte destination preservation after covered
  unrecovered allocation failures
- added deterministic fail-at-N allocator injection, leak accounting, ownership
  validation, alias tests, later-allocation failures, and successful recovery
- made `ppf_init2_ctx` accept a mutable context so initialization can report an
  allocation failure through context flags
- added `PPF_OPENMP=AUTO`, `ON`, and `OFF` to CMake and Make
- added a genuine serial library and installed package with no OpenMP runtime
  dependency
- added Clang libFuzzer targets for the public text parser and PPFIF importer
- validated 54 of 54 CTest entries in both OpenMP and serial configurations
- validated the allocation suite with AddressSanitizer,
  UndefinedBehaviorSanitizer, and leak detection

The numerical formulas, exact product identities, final rounding algorithms,
certified constant formulas, and special-function formulas are unchanged.

## 0.10.7 machine-profiled AUTO product planner — 2026-08-14

- centralized AUTO product selection for multiplication, square, FMA/FMS, and
  reciprocal-square-root refinement;
- added operation-specific effective-worker caps so rank 3, rank 5, full, and
  window graphs do not request teams larger than their usable task depth;
- retained the conservative portable policy as the distributable default;
- added an opt-in generated build-time profile keyed by operation, exact
  precision, limb count, and requested worker count;
- added public plan, profile-name, and profile-hash introspection;
- added a deterministic profile generator requiring independent process
  evidence, close/spread agreement, a minimum effect size, a bootstrap interval
  beyond parity, and a process-win threshold;
- added a resumable counterbalanced calibration runner and tuned-build profile
  validator, with stable campaign IDs and process-independent CPU-affinity
  fingerprints;
- added portable and custom-profile execution tests plus an inspection CLI.

This release changes planner and product call-site code but does not introduce a
new multiplication formula or change final rounding semantics. Explicit
algorithm requests continue to bypass AUTO and generated profiles.

## 0.10.6 counterbalanced campaigns and regression gates — 2026-08-14

- added a resumable publication-campaign launcher that executes close and
  spread placement in randomized paired blocks with stable extension semantics;
- added deterministic per-process condition-order randomization and a persisted
  execution schedule, so thermal and cache trends are not confounded with one
  fixed operation order;
- added an order-adjusted paired analyzer that separates binding effects from
  first-versus-second execution effects and retains deterministic bootstrap
  intervals;
- added a machine-aware benchmark regression comparator with confirmed,
  preliminary, stable, and improvement classifications;
- added an explicit regression policy, regression reports, and synthetic tests
  covering schedule balance, paired-effect recovery, and regression decisions;
- preserved the complete 0.10.5 canonical evidence and all production numerical
  kernels unchanged.

The 0.10.6 release is an evidence, orchestration, and regression-control update.
It does not alter arithmetic, rounding, root, constant, transcendental,
special-function, conversion, formatting, or parallel numerical kernels.

## 0.10.5 benchmark evidence and canonical baseline — 2026-08-14

- retained every in-process timing sample emitted by the core-scaling benchmark
  executables instead of discarding the sample vector after computing a summary;
- standardized benchmark JSON on schema version 2 with explicit warmup count,
  sample count, summary statistic, and correctness status;
- added process repetition and idempotent condition-level resume support;
- attached campaign identifiers, stable condition identifiers, executable
  hashes, binding policy, and process-repeat indices to retained records;
- added privacy-conscious Linux and WSL environment capture covering CPU
  topology, affinity, compiler and linker versions, GMP and MPFR versions and
  build options, OpenMP settings, and frequency-policy fields;
- added process-weighted medians and deterministic hierarchical bootstrap
  intervals over independent processes and retained in-process samples;
- added process-level and condition-level statistics, raw-sample CSV export,
  executable and environment fingerprints, and hashed analysis manifests;
- added a strict executable-level schema regression to the smoke suite;
- added a canonical baseline specification and claim boundary for public
  benchmark artifacts.

The strict local 0.10.5 validation passed 31 fast, 7 extended, and 12 smoke
tests. A byte-level comparison found 35 of 35 production `src/` files unchanged
from 0.10.4. No production arithmetic, rounding, root, constant, transcendental,
special-function, formatting, conversion, or parallel kernel changed in this
patch release.

## 0.10.4 MPFR optional-format compatibility fix — 2026-08-13

- made binary128 MPFR conversion declarations opt-in through
  `MPFR_WANT_FLOAT128`, as required by the official MPFR header;
- made decimal64 and decimal128 MPFR conversion declarations opt-in through
  `MPFR_WANT_DECIMAL_FLOATS`;
- removed nonstandard `mpfr_decimal64` and `mpfr_decimal128` aliases from the
  test-only fallback header and used the Polyphase conditional-format types at
  the interoperability boundary;
- added CMake compile-and-link feature probes so conditional tests are built
  only when both the compiler types and installed MPFR conversion APIs exist;
- added Make compile-link-run probes so the non-CMake validation path also
  respects the optional capabilities of the installed MPFR build;
- aligned the fallback `mpfr_fpif_export` declaration with the official
  mutable-source signature and corrected the formatting benchmark call site;
- added release-audit checks for the opt-in declarations, feature probes, and
  FPIF signature.

No production arithmetic, rounding, constant, transcendental, special-function,
formatting, conditional-format, or parallel kernel was changed in this patch
release.

## 0.10.3 MPFR precision-type compatibility fix — 2026-08-13

- corrected the test-only minimal MPFR ABI model so `mpfr_prec_t` and
  `mpfr_exp_t` use the signed GMP-sized types selected by MPFR's default ABI;
- replaced implicit MPFR-to-Polyphase precision conversions in
  `tests/test_divisor_plan.c` with explicit range-checked conversions;
- added `test_mpfr_type_model`, which fails at compile time if either MPFR
  precision or exponent type becomes unsigned;
- added release-audit checks for the fallback type model and regression test;
- changed the complete Windows/WSL runner to repeat a failed parallel CMake
  build serially with verbose output, keeping the decisive diagnostic at the
  end of the stage log.

No production arithmetic, rounding, constant, transcendental, special-function,
formatting, or parallel kernel was changed in this patch release.

## 0.10.2 official-MPFR-header validation fix — 2026-08-12

- replaced the direct deprecated `mpfr_root` reference call in
  `tests/test_roots.c` with a warning-free oracle that preserves the legacy
  signed-zero behavior of `mpfr_root`;
- mirrored the official MPFR deprecation annotations for `mpfr_root` and
  `mpfr_grandom` in the test-only minimal ABI header;
- made the CMake `validation` preset treat project warnings as errors;
- added a release audit that rejects direct calls to known deprecated MPFR
  interfaces in validation and benchmark C sources;
- improved the complete Windows/WSL runner so compiler error context is printed
  even when parallel build output pushes the first diagnostic out of the log
  tail.

No production arithmetic, rounding, constant, transcendental, special-function,
or parallel kernel was changed in this patch release.

## 0.10.1 format-security and validation fix — 2026-08-12

- replaced the dynamic zero-argument `snprintf` path for native `%m` formatting
  with explicit errno-message extraction and bounded padding;
- preserved GNU `%#m` symbolic-name behavior on glibc 2.35 and newer;
- added a native `%m` differential regression test that does not require MPFR;
- added a CI hardening build using `-Wformat -Werror=format-security`;
- updated the complete Windows/WSL campaign runner to exercise the same
  hardening flags and to report the effective build environment.

No arithmetic, rounding, constant, transcendental, or special-function kernel
was changed in this patch release.

## 0.10.0 public release-hardening edition — 2026-08-12

### Public-source foundation

- selected `LGPL-3.0-or-later` for the project;
- added complete license texts, third-party notices, contribution terms,
  governance, security policy, support policy, authorship, and citation
  metadata;
- added SPDX metadata to source-like files;
- removed temporary source-header copies from the release tree;
- retained the original chronological README and release notes under
  `docs/history/`.

### Build and packaging

- made CMake the canonical install and downstream-consumer path;
- added project version metadata and versioned shared-library properties;
- made the actual OpenMP requirement explicit;
- added robust GMP and optional MPFR discovery modules;
- added shared/static build switches and an `AUTO/ON/OFF` MPFR policy;
- added relocatable `pkg-config` metadata;
- added exported CMake package targets and an install-tree consumer test;
- added CMake presets and public CI workflows for GCC, Clang, source-only,
  installed-consumer, and extended validation paths.

### Validation organization

- separated ordinary `fast`, long-running `extended`, and benchmark `smoke`
  CTest labels;
- added a release audit for licensing, version consistency, source hygiene,
  API coverage, and optional binary independence;
- added a deterministic source-release packager and checksum generation;
- added explicit claims/evidence, testing, building, ABI, and release-process
  documentation.

### Numerical implementation

No numerical algorithm or public structure layout was intentionally changed in
this hardening block. The runtime remains pre-1.0 and its ABI is not frozen.
The public version string is normalized to `0.10.0`.

## Historical development record

The earlier cumulative release notes, including intermediate inventory counts
and performance experiments, are preserved in
`docs/history/RELEASE_NOTES_legacy_0.10.0.md`. They are historical records, not
the current project status.
