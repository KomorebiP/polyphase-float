# Runtime-complete release

## Added

- all 38 remaining lowercase inventory counterparts;
- complete rounding-control and certification family;
- complete random and build-information surface;
- cache and memory cleanup controls;
- MPF compatibility and external custom storage;
- conditional binary128 and decimal64/decimal128 conversion;
- three new differential suites and a correctness-gated runtime benchmark;
- coverage refresh to 349/349.

## Validation

- complete Make differential suite passed;
- independent CMake Release build passed 48/48 CTest targets;
- new runtime subsystems passed ASan, UBSan, leak detection, and GCC static analysis;
- production independence audit passed with zero imported `mpfr_*` symbols.

## Boundary

Literal API completion is not claimed as equivalent to MPFR's production maturity. Licensing, ABI stability, wider portability, external replication, long-running fuzzing, and performance hardening remain.

---

# Inverse-angle and dense-phase speed refinement

The latest branch adds:

- a cubically convergent certified arctangent correction;
- a shared exact-ratio `atan2` path with exact quadrant reconstruction;
- direct tangent enclosures from the fixed-point sine/cosine kernel;
- density-aware sine/cosine Taylor blocks;
- a dedicated correctness-gated launcher and local benchmark report.

Same-host comparisons against the preceding independent package reduced `atan2(0.75,-0.25)` by about 1.47x at 20,000 bits, 2.32x at 50,000 bits, and 2.03x at 100,000 bits. The corresponding `atan(0.3)` gains were about 1.30x, 1.92x, and 1.67x. MPFR remains faster for these calls in the current one-thread benchmark.

The stable inverse-hyperbolic rewrite, dense binary splitting, two-phase dense-angle decomposition, and universal large Taylor blocks were measured and rejected.

# Release notes — certified log, exponential, and power family

## Added

- all 24 logarithm, exponential, and power inventory functions;
- a shared directed-interval and precision-escalation evaluator;
- exact recognizers for powers of two, powers of ten, exact dyadic powers, and
  exact compound results;
- complete structured, randomized, and narrow-range differential tests;
- a correctness-gated representative benchmark;
- `RUN_TRANSCENDENTALS.ps1`;
- coverage refresh to 208/349 symbols, or 59.60 percent.

## Current boundary

The new family is correctly rounded over the tested domain and remains free of
MPFR arithmetic imports.  Its first conservative series kernels are slower
than MPFR and are not presented as a performance result.

---

# Release notes

## Rank-adaptive product and fused-operation planner

- Added direct-limb five-product Toom-3 multiplication and square.
- Added explicit full, rank-3, rank-5, automatic, and certified-window product
  choices.
- Added exact unrounded product builders shared by multiplication, square,
  FMA, FMS, and reciprocal-square-root correction.
- Added threaded and algorithm-selected FMA/FMS entry points.
- Added exact phase-aligned FMA/FMS with one final rounding.
- Added rank selection inside reciprocal-square-root Newton correction.
- Kept the portable automatic policy conservative after nonmonotone crossover
  measurements.
- Fixed forced rank 5 so it remains rank 5 with fewer than five workers rather
  than silently changing algorithms.
- Added correctness-gated product-planner and matched-worker benchmarks.
- Added paired medians, bootstrap intervals, W-L-T counts, sign tests, and
  checkpointed resume.
- Removed unrelated large dot-product setup from multiplication/FMA benchmark
  processes.
- Retained failed or conditional product ideas in a separate boundary report.
- Expanded CMake/CTest coverage to 20 tests.

The independent production library continues to import no MPFR symbols.

## Hot-path speed refinement

- Added direct exact integer assignment and extraction paths.
- Removed redundant full-significand copies from remainder calculations.
- Added certified reduced-precision short radix output.
- Added exact direct `ppf_t` to GMP mantissa conversion for the reduced radix
  endpoints.
- Reused one GMP radix-conversion object across both certified endpoints.
- Replaced integer root-with-remainder calculations with floor-root calls.
- Increased the ordinary-root first certificate margin to prevent expensive
  full retries on hard rounding cases.
- Replaced reciprocal-root divide-first construction with a faster root-first
  exact enclosure.
- Added correctness-gated paired benchmarks for the slowest calls.
- Added raw-row checkpointing and automatic benchmark resume.
- Added the hot-path benchmark to CMake and CTest.
- Fixed the minimal-MPFR-ABI CMake condition for the new benchmark.
- Retained unsuccessful text and reciprocal-root experiments as documented
  boundaries.

The independent production library continues to import no MPFR symbols.

## Deep hot-path refinement

- Added a recursive fixed-point inverse-square-root kernel.
- Added certified residual truncation with a sub-ulp omission bound.
- Added `ppf_rec_sqrt_threads` with three exact product channels.
- Added exact ternary resolution through `result^2 * input` relative to one.
- Fixed an odd-exponent directed-rounding corner case exposed by the matched-worker benchmark.
- Added a one-pass certified short-radix output path with a cheap exactness rejection.
- Added a correctness-gated equal-worker reciprocal-square-root benchmark.
- Added checkpointed benchmark output and automatic resume.

## Certified transcendental speed backend

- Added a common-scale fixed-point interval representation for log, exp, and
  general power evaluation.
- Removed repeated `ppf_t` allocation, normalization, and general floating
  division from the inner series loops.
- Added exact read-only GMP views of independent significands.
- Added measured root/Horner and high-precision bit-burst policies for
  logarithms.
- Added sparse/dense exponential routing between bit-burst reconstruction and
  blocked Taylor evaluation.
- Added fixed-point fast paths for log1p, log2, log10, log2p1, log10p1,
  expm1, exp2, exp10, exp2m1, exp10m1, and general power.
- Added a 1,000/5,000-bit all-rounding-mode differential test for the complete
  family.
- Added a randomized paired one-thread benchmark with checkpointed raw output,
  bootstrap intervals, W-L-T counts, and high-precision extensions.
- Retained the old native interval evaluator as a correctness fallback.
- Documented rejected AGM, Newton, binary-splitting, and universal-schedule
  experiments.
- Expanded CMake/CTest coverage to 25 tests.

On the development machine, the optimized independent kernels reduced
5,000-bit log, log1p, exp, expm1, and general power by approximately 10x, 20x,
45x, 149x, and 19x relative to the preceding independent implementation.
Against the installed MPFR build, exp and expm1 showed repeatable one-thread
wins at high precision; logarithm and general noninteger power remain slower.

---

# Release notes — trigonometric and hyperbolic families

## Added

- all 25 trigonometric inventory functions;
- all 10 hyperbolic inventory functions;
- certified reduction by `pi/2` for ordinary periodic functions;
- exact rational reduction for `pi`-scaled and unit-scaled functions;
- reduced sine, cosine, and arctangent enclosures;
- complete quadrant, signed-zero, pole, infinity, and NaN handling;
- exact grid recognition for integer, half-integer, quarter-turn, and selected `m*pi/6` values;
- `atan2`, `atan2u`, and `atan2pi` quadrant semantics;
- hyperbolic saturation and guaranteed range shortcuts;
- ordinary, special, randomized, large-precision, faithful-rounding, and alias tests;
- correctness-gated development benchmark;
- `RUN_TRIG_HYPERBOLIC.ps1`;
- API coverage refresh to 243/349 symbols, or 69.63 percent.

## Current boundary

The 35-function block is an independent correct-rounding baseline rather than a performance result. The ordinary trigonometric and inverse-angle paths remain much slower than MPFR because they use complete floating interval objects throughout reduction and series evaluation. A fixed-point paired-kernel rewrite is the next speed target.

## Trigonometric and hyperbolic speed refinement

- Added common-scale fixed-point sine/cosine, arctangent, and paired hyperbolic kernels.
- Added direct dyadic phase reduction for pi-scaled angles.
- Added true shared `sin_cos` and `sinh_cosh` calculations.
- Added adaptive 4/16/32 arctangent blocks.
- Preserved the independent certified fallback and zero-MPFR-import production boundary.
- Added correctness-gated sweeps through 100,000 bits and same-host before/after measurements.

## Special functions and remaining constants

- added all 22 previously missing special-function/constant counterparts
- completed the 24-entry special-function family
- added Euler and Catalan certified interval caches
- added shared special-function interval and precision-escalation infrastructure
- added Gamma, incomplete Gamma, log-Gamma, digamma, Beta, Ei, Li2, zeta, erf/erfc, Bessel J/Y, AGM, and Airy Ai
- added seven dedicated special-function test suites
- added a correctness-gated benchmark and WSL launcher
- recorded current performance boundaries and the explicit return-to-speed-work backlog
- raised direct inventory coverage to 265/349 (75.93%)


---

# Release notes — stream, interchange, and formatted output

## Added

- completed all five stream/raw I/O inventory functions;
- completed all ten formatted-output inventory functions;
- added correctly rounded stream text input and output;
- added the canonical endian-independent `PPFIF01` representation;
- added CRC-32 corruption detection and transactional import;
- added standard C, GMP, precision, and `%R` formatting in one parser;
- added all ten stream, stdout, buffer, bounded-buffer, and allocated-string wrappers;
- added zero-precision hexadecimal normalization compatible with the target formatter;
- added randomized differential formatting and stream-output tests;
- added direct limb serialization and a table-driven CRC hot path;
- added `RUN_IO_FORMATTING.ps1`, a correctness-gated benchmark, and a new example;
- raised direct inventory coverage to 279/349, or 79.94 percent.

## Boundary

`PPFIF01` is an independent portable format. It is not asserted to be byte-compatible with GNU MPFR's experimental FPIF encoding. Positional `$` format arguments remain outside the baseline sequential variadic interface.


---

# Release notes — complete arithmetic family

## Added

- completed all 54 arithmetic inventory functions;
- added scalar add variants for unsigned integer, signed integer, double, GMP integer, and GMP rational inputs;
- added all left- and right-sided subtraction scalar variants;
- added scalar multiplication and division variants;
- added unsigned power-of-two scaling over the complete `unsigned long` domain;
- added exact factorial followed by one independent final rounding;
- added exact two-product `fmma` and `fmms` with one final rounding;
- added randomized differential testing across all ordinary rounding modes;
- added signed-zero, NaN, infinity, narrow-range, enormous-shift, and alias testing;
- added a correctness-gated arithmetic-completion benchmark and PowerShell launcher;
- raised direct inventory coverage to 311/349, or 89.11 percent;
- raised public header/shared-export agreement to 385/385.

## Boundary

The scalar wrappers complete the public numerical surface but are not uniformly faster than MPFR. GMP-integer multiplication and factorial are strong in the measured high-precision rows, while temporary-based scalar and full-rational variants remain optimization targets. No operation delegates to MPFR.
