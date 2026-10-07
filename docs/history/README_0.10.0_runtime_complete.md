# Polyphase Independent Floating-Point Library

## Runtime-complete independent surface

The supported baseline now provides a direct or conditional `ppf_*` counterpart for every one of the 349 lowercase symbols in the project inventory. The production shared library exports 423 independent functions and imports no MPFR symbols.

The final runtime block adds:

```text
can_round, nearest-away rounding, and rounding-mode reporting
uniform, normal, paired-normal, and exponential random generators
version and build-option queries
cache, scratch-pool, and memory cleanup
MPF compatibility aliases
application-owned custom significand storage
conditional binary128, decimal64, and decimal128 conversion
```

Run the complete block with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_RUNTIME_COMPLETION.ps1
```

See `RUNTIME_COMPLETION_REPORT.md`, `RUNTIME_COMPLETION_VALIDATION.txt`, and `API_COVERAGE.md`.

This package is an independent arbitrary-precision binary floating-point
library built on GMP limb and integer arithmetic. The production library does
not link MPFR. A separate optional adapter and the tests use MPFR only for
interoperability and differential verification.

A regular finite number is represented as

\[
x=(-1)^sM2^{e-p},
\qquad 2^{p-1}\le M<2^p.
\]

Each `ppf_t` owns its precision, normalized significand, exponent, sign, value
category, and allocation. A `ppf_context_t` owns the active exponent range,
rounding mode, exception flags, allocator, and scratch arena.


## Complete independent arithmetic family

All 54 arithmetic entries in the target inventory now have direct `ppf_*`
counterparts. The completed surface includes scalar integer, C-double, GMP
integer, and GMP rational variants, unsigned power-of-two scaling, factorial,
and exact two-product fused operations:

```text
add_ui add_si add_d add_z add_q
ui_sub sub_ui si_sub sub_si d_sub sub_d z_sub sub_z sub_q
mul_ui mul_si mul_d mul_z mul_q
ui_div div_ui si_div div_si d_div div_d div_z div_q
mul_2ui div_2ui fac_ui fmma fmms
```

Rational variants keep the complete expression exact in `mpq_t` until one
final binary rounding. `fmma` and `fmms` form two exact dyadic products,
combine them, and round once. No implemented arithmetic operation calls MPFR.

Run the completed block with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_ARITHMETIC_COMPLETION.ps1
```

See `docs/ARITHMETIC_COMPLETION.md`, `ARITHMETIC_COMPLETION_REPORT.md`, and
`PROGRESS_AFTER_ARITHMETIC_COMPLETION.md`.

## Independent stream, interchange, and formatted output

The stream/raw I/O and formatted-output families are now complete:

```text
out_str, inp_str, fpif_export, fpif_import, dump
fprintf, vfprintf, printf, vprintf
sprintf, vsprintf, snprintf, vsnprintf, asprintf, vasprintf
```

The formatter accepts ordinary C values, GMP values, precision values, and
`ppf_t` values in one argument list. Independent floating conversions use the
`%R` type with binary, hexadecimal, scientific, fixed, and general output,
including explicit rounding selectors. `PPFIF01` is an endian-independent,
checksum-protected interchange representation that preserves precision,
category, sign, exponent, and significand.

Run the full block with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_IO_FORMATTING.ps1
```

See `docs/IO_AND_FORMATTING.md`, `IO_FORMATTING_REPORT.md`, and
`PROGRESS_AFTER_IO_FORMATTING.md`.

## Independently certified special functions and constants

The complete 24-function special family is now implemented directly on `ppf_t`:

```text
eint, li2
gamma, gamma_inc, lngamma, lgamma, digamma, beta
zeta, zeta_ui
erf, erfc
j0, j1, jn, y0, y1, yn
agm, ai
const_log2, const_pi, const_euler, const_catalan
```

The functions share a directed interval and precision-escalation system. Gamma and digamma use recurrence, asymptotic Bernoulli expansions, and reflection. Zeta selects among Euler--Maclaurin, Hasse, exact Bernoulli values, and the functional equation. Bessel and Airy series include explicit ratio and tail certificates. Euler and Catalan constants use the same interval caches as `pi` and `log(2)`.

Run the full special-function validation and development benchmark with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_SPECIAL_FUNCTIONS.ps1
```

The present special-function kernels are correctness-first and remain slower than MPFR. The explicit plan to return to optimization—including the previously deferred inverse-angle and dense-trigonometric work—is recorded in `SPEED_RETURN_BACKLOG.md`.

See `docs/SPECIAL_FUNCTIONS.md`, `SPECIAL_FUNCTIONS_REPORT.md`, `SPECIAL_FUNCTIONS_VALIDATION.txt`, and `FAILED_SPECIAL_FUNCTION_EXPERIMENTS.md`.

## Correctly rounded trigonometric and hyperbolic functions

The complete 35-function trigonometric and hyperbolic surface is now implemented directly on `ppf_t`:

```text
cos, sin, tan, cosu, sinu, tanu, cospi, sinpi, tanpi, sin_cos
sec, csc, cot, acos, asin, atan, acosu, asinu, atanu
acospi, asinpi, atanpi, atan2, atan2u, atan2pi
cosh, sinh, tanh, sinh_cosh, sech, csch, coth, acosh, asinh, atanh
```

Ordinary angles use certified reduction by `pi/2`, reduced sine/cosine and arctangent enclosures, exact quadrant reconstruction, and the common precision-escalation loop. Scaled-angle functions reduce the exact dyadic input as a rational before multiplying by the independent `pi` interval. Hyperbolic functions reuse the certified exponential and logarithm layers, with explicit saturation and range proofs for large inputs.

Run the latest inverse-angle and dense-phase speed pass with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_INVERSE_ANGLE_SPEEDUPS.ps1
```

This pass adds a cubically convergent certified arctangent correction, shared exact-ratio `atan2`, direct tangent intervals inside the correction loop, and density-aware sine/cosine blocks. See `INVERSE_ANGLE_SPEEDUP_REPORT.md` and `FAILED_INVERSE_ANGLE_SPEEDUP_EXPERIMENTS.md`.

The broader trigonometric/hyperbolic speed sweep remains available with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_TRIG_HYPERBOLIC_SPEEDUPS.ps1
```

The hot paths now use common-scale integer enclosures, shared sine/cosine and sinh/cosh kernels, direct dyadic phase reduction for the pi-scaled functions, midpoint-radius reconstruction, and adaptive arctangent blocks. The previous independent certified implementation remains the native fallback when a fast enclosure is undecided. See `TRIG_HYPERBOLIC_SPEEDUP_REPORT.md` and `FAILED_TRIG_HYPERBOLIC_SPEEDUP_EXPERIMENTS.md`.

## Correctly rounded logarithms, exponentials, and powers

The full 24-function family is now implemented directly on `ppf_t`:

```text
log, log_ui, log2, log10, log1p, log2p1, log10p1
exp, exp2, exp10, expm1, exp2m1, exp10m1
pow, powr, pow_ui, pow_si, pow_uj, pow_sj, pown, pow_z
ui_pow_ui, ui_pow, compound_si
```

Every nonexact path uses directed interval arithmetic and the shared
precision-escalation loop.  A result is accepted only when the interval proves
both the final floating-point value and its ternary direction.  Algebraic exact
paths preserve zero ternary values for identities such as powers of two,
powers of ten, exact dyadic roots, and exact compound results.

Run the complete differential tests and representative benchmark with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_TRANSCENDENTALS.ps1
```

See `docs/LOG_EXP_POWER.md`, `PROGRESS_AFTER_TRANSCENDENTALS.md`, and
`FAILED_TRANSCENDENTAL_EXPERIMENTS.md`.

The transcendental family now has a second, certified fixed-point backend.
It removes repeated floating-object allocation from the series loops and
selects separate sparse and dense exponential schedules. The original
directed-floating implementation remains a native correctness fallback.

Run the dedicated paired speed benchmark with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_TRANSCENDENTAL_SPEEDUPS.ps1
```

On the development machine, natural `exp` and `expm1` were faster than the
installed MPFR build from 5,000 through 100,000 bits in the tested sparse
dyadic workload. The denser `exp10` family also showed repeatable gains, while
`exp2` crossed at 100,000 bits. Logarithm and general noninteger power remain
slower than MPFR, despite large internal reductions in runtime. See
`TRANSCENDENTAL_SPEEDUP_REPORT.md` and
`FAILED_TRANSCENDENTAL_SPEEDUP_EXPERIMENTS.md`.

## Rank-adaptive products and fused arithmetic

The current branch moves exact product selection underneath multiplication,
square, fused multiply-add, fused multiply-subtract, and reciprocal-square-root
refinement.  The available exact product graphs are:

```text
full GMP product
three-product Karatsuba
five-product direct-limb Toom-3
certified convolution-window experiment
```

FMA and FMS now form an exact unrounded rank-selected product, align the third
operand in the same binary phase, and round only once.  The portable automatic
policy is conservative because the rank-3/rank-5 crossover is nonmonotone;
the supplied tuner records machine-specific choices by operation, precision,
worker count, and binding policy.

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_PRODUCT_PLANNER.ps1
```

See `PRODUCT_PLANNER_REPORT.md`, `FAILED_PRODUCT_EXPERIMENTS.md`, and
`docs/PRODUCT_AND_FUSED_PLANNER.md`.

## Independently certified constants

The library now computes

\[
\pi,\qquad \log 2,\qquad \gamma,\qquad G
\]

directly into `ppf_t`, where `gamma` is Euler's constant and `G` is Catalan's constant.

The path is

\[
\text{constant-specific exact calculation}
\longrightarrow
[L,U]
\longrightarrow
\text{independent binary rounding}
\longrightarrow
\texttt{ppf\_t}.
\]

The public interface includes:

```c
ppf_const_pi
ppf_const_log2
ppf_const_euler
ppf_const_catalan
ppf_precompute_pi
ppf_precompute_log2
ppf_precompute_euler
ppf_precompute_catalan
ppf_get_constant_interval
ppf_clear_constant_cache
ppf_free_constant_caches
```

The constant layer also provides a reusable precision-escalation loop,
thread-safe exact interval caches, per-rounding-mode result caches, explicit
worker control, and diagnostics.

The \(\pi\) calculation uses the exact Polyphase-Chudnovsky packet calculus,
its cancellation and packet scheduling rules, a formally bounded omitted
tail, and a certified dyadic enclosure. The \(\log 2\) calculation uses an
exact hypergeometric packet recurrence and a rigorous geometric tail bound.

## Foundation available

- arbitrary per-object binary precision
- normalized owned significands
- signed zero, infinity, and NaN
- explicit and thread-local contexts
- all six rounding modes
- ternary exact/above/below results
- exact dyadic and certified interval rounding
- overflow, underflow, range checking, and subnormal emulation
- initialization, assignment, parsing, conversion, and fit tests
- addition, subtraction, multiplication, square, division, and scaling
- FMA, FMS, exact sum, and exact dot product
- complete comparison and classification
- complete integer rounding and remainder operations
- neighboring values, min/max, positive difference, and exponent access
- square root, reciprocal square root, cube root, general roots, and hypot
- reusable-denominator division plans and matched-worker batches
- independently certified \(\pi\), \(\log 2\), Euler, and Catalan constants
- correctly rounded logarithms, exponentials, and powers
- complete special functions and Euler/Catalan constants

## Build and test

```bash
make clean
make -j$(nproc) all test audit examples
```

The independent library is:

```text
lib/libpolyphase_float.so
```

The optional MPFR adapter is:

```text
lib/libpolyphase_float_mpfr.so
```

The binary audit rejects the independent build if it imports an `mpfr_*`
symbol.

## Example

```c
#include <polyphase_float.h>

ppf_context_t context;
ppf_t pi, log2_value;

ppf_context_init(&context);
ppf_init2_ctx(&pi, 1000000, &context);
ppf_init2_ctx(&log2_value, 1000000, &context);

ppf_set_constant_threads(4);
ppf_const_pi(&pi, PPF_RNDN, &context);
ppf_const_log2(&log2_value, PPF_RNDN, &context);

ppf_clear(&log2_value);
ppf_clear(&pi);
ppf_context_clear(&context);
ppf_free_constant_caches();
```

A complete executable version is in `examples/constants.c`.

## One-command Windows/WSL validation

For the complete library:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_INDEPENDENT_LIBRARY.ps1
```

For the focused constant tests and comparisons:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_CONSTANTS.ps1
```

## Current boundary

The independent binary foundation and the complete logarithm, exponential, and
power family are in place. Literal coverage is 208 of 349 inventory symbols,
or 59.60 percent. Product-heavy arithmetic has selected speed advantages. The
new fixed-point exponential paths now add several one-thread transcendental
speed wins, while logarithm and general power remain the main performance
boundary.

The next broad mathematical layer is trigonometric and hyperbolic functions,
beginning with certified argument reduction and one shared `sin_cos` kernel.

## Hot-path speed refinement

The current branch removes avoidable full-width copies from exact integer
assignment and extraction, remainder operations, and radix output. General
roots use floor-only integer roots and a larger first certificate margin.
Reciprocal roots use a root-first exact enclosure.

Run the correctness-gated slow-call benchmark with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\RUN_HOTPATH_SPEEDUPS.ps1
```

The benchmark checkpoints every completed row and resumes after interruption.
See `HOTPATH_SPEEDUP_REPORT.md` and `FAILED_HOTPATH_EXPERIMENTS.md`.
