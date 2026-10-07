# Trigonometric and Hyperbolic Speed Refinement

This release replaces the slowest interval-object loops in the trigonometric and hyperbolic layer with fixed-scale certified integer kernels. The public functions and their rounding contracts are unchanged.

## Main architectural change

The earlier implementation represented both ends of every enclosure as complete floating-point objects and repeatedly allocated, normalized, rounded, and cleared those objects inside each recurrence. The new hot path keeps a common binary scale:

\[
L2^{-S}\le x\le U2^{-S}.
\]

All series terms, phase products, and reconstruction steps operate on the integer endpoints `L` and `U`. The final enclosure is converted to `ppf_t` only after it determines both the correctly rounded value and its ternary direction. An undecided fast enclosure returns to the existing independent certified path. It never calls MPFR arithmetic.

## Retained improvements

### Shared fixed-point sine and cosine

The reduced sine and cosine series are evaluated together. Their powers and tail bounds are shared, and the double-angle reconstruction uses midpoint-radius enclosures so each level needs two large products instead of four independent endpoint products.

### Direct dyadic phase reduction

For `sinpi`, `cospi`, and `tanpi`, the exact binary input is reduced as an integer dyadic phase before multiplication by the cached interval for `pi`. This removes the former full rational canonicalization and most of the high-precision scaling work.

### Adaptive arctangent blocks

The reduced arctangent series uses blocks of 4 terms below 12,000 working bits, 16 terms through the middle range, and 32 terms at the largest tested working precisions. The half-angle planner remains precision-dependent. This reduced the high-precision inverse-angle cost without imposing the large-block overhead on small calls.

### Shared hyperbolic evaluation

Ordinary `sinh` and `cosh` calls share one fixed-point calculation. The paired `sinh_cosh` call now truly shares the exponential/series and reconstruction work instead of calling two independent public functions. Large arguments retain the proven range and saturation shortcuts.

### Transactional paired functions

`sin_cos` and `sinh_cosh` construct both certified outputs before committing either destination. This preserves input/output alias safety while sharing the dominant calculation.

## Same-host improvement over the original independent implementation

The following rows compare the original trig/hyperbolic package and this package, rebuilt and measured on the same development host at 5,000 bits. Both benchmarks used nine alternating-order repetitions.

| Operation | Original (s) | Refined (s) | Internal speedup | Current MPFR / Polyphase |
|---|---:|---:|---:|---:|
| `sin_0.3` | 0.028303248 | 0.000332714 | **85.07x** | 0.315x |
| `cos_0.3` | 0.029286442 | 0.000286883 | **102.08x** | 0.345x |
| `tan_0.3` | 0.028777562 | 0.000320748 | **89.72x** | 0.342x |
| `sinpi_0.3` | 0.035698438 | 0.000386735 | **92.31x** | 0.496x |
| `atan_0.3` | 0.088834187 | 0.000941575 | **94.35x** | 0.384x |
| `sinh_0.3` | 0.000765313 | 0.000299568 | **2.55x** | 0.695x |
| `cosh_0.3` | 0.000995717 | 0.000292280 | **3.41x** | 0.711x |
| `tanh_0.3` | 0.000596133 | 0.000330210 | **1.81x** | 0.641x |
| `atan2_0.75_-0.25` | 0.103682882 | 0.001278614 | **81.09x** | 0.292x |

The periodic and inverse-angle hot loops improved by roughly 81x to 102x in this same-host comparison. Direct hyperbolic functions improved by roughly 1.8x to 3.4x. The inverse hyperbolic functions did not gain materially in this pass.

## Current comparison with MPFR

Each row below uses one Polyphase call and one MPFR call at the same binary precision and nearest-even rounding. The benchmark checks the value and ternary direction before timing. A ratio above one favors Polyphase.

### 5,000 bits

| Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |
|---|---:|---:|---:|
| `sin_0.3` | 0.000332714 | 0.000104899 | 0.315x |
| `cos_0.3` | 0.000286883 | 0.000099088 | 0.345x |
| `tan_0.3` | 0.000320748 | 0.000109817 | 0.342x |
| `sinpi_0.3` | 0.000386735 | 0.000191899 | 0.496x |
| `atan_0.3` | 0.000941575 | 0.000361368 | 0.384x |
| `sinh_0.3` | 0.000299568 | 0.000208254 | 0.695x |
| `cosh_0.3` | 0.000292280 | 0.000207699 | 0.711x |
| `tanh_0.3` | 0.000330210 | 0.000211548 | 0.641x |
| `sin_cos_0.3` | 0.000294865 | 0.000104157 | 0.353x |
| `sinh_cosh_0.3` | 0.000292697 | 0.000203653 | 0.696x |

### 20,000 bits

| Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |
|---|---:|---:|---:|
| `sin_0.3` | 0.002499559 | 0.001767701 | 0.707x |
| `cos_0.3` | 0.002216221 | 0.001485544 | 0.670x |
| `tan_0.3` | 0.002271229 | 0.001534101 | 0.675x |
| `sinpi_0.3` | 0.004752222 | 0.002988922 | 0.629x |
| `atan_0.3` | 0.010163941 | 0.003168068 | 0.312x |
| `sinh_0.3` | 0.002125716 | 0.000766162 | 0.360x |
| `cosh_0.3` | 0.002119885 | 0.000766332 | 0.361x |
| `tanh_0.3` | 0.002309269 | 0.000839170 | 0.363x |
| `sin_cos_0.3` | 0.002361659 | 0.001512192 | 0.640x |
| `sinh_cosh_0.3` | 0.002141315 | 0.000786298 | 0.367x |

### 50,000 bits

| Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |
|---|---:|---:|---:|
| `sin_0.3` | 0.008796678 | 0.005031811 | 0.572x |
| `cos_0.3` | 0.008336414 | 0.005061780 | 0.607x |
| `tan_0.3` | 0.009053377 | 0.005207258 | 0.575x |
| `sinpi_0.3` | 0.028785820 | 0.014157133 | 0.492x |
| `atan_0.3` | 0.061262224 | 0.015802875 | 0.258x |
| `sinh_0.3` | 0.008411234 | 0.003206280 | 0.381x |
| `cosh_0.3` | 0.008164037 | 0.003096299 | 0.379x |
| `tanh_0.3` | 0.008785497 | 0.003276719 | 0.373x |
| `sin_cos_0.3` | 0.007599848 | 0.005037782 | 0.663x |
| `sinh_cosh_0.3` | 0.007597815 | 0.003051013 | 0.402x |

### 100,000 bits

| Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |
|---|---:|---:|---:|
| `sin_0.3` | 0.024478576 | 0.014270953 | 0.583x |
| `cos_0.3` | 0.027540958 | 0.016585300 | 0.602x |
| `tan_0.3` | 0.024505723 | 0.014468079 | 0.590x |
| `sinpi_0.3` | 0.114041051 | 0.039389385 | 0.345x |
| `atan_0.3` | 0.209435950 | 0.045155695 | 0.216x |
| `sinh_0.3` | 0.027556702 | 0.009002625 | 0.327x |
| `cosh_0.3` | 0.024573101 | 0.008955038 | 0.364x |
| `tanh_0.3` | 0.029905354 | 0.009318665 | 0.312x |
| `sin_cos_0.3` | 0.022879648 | 0.015061042 | 0.658x |
| `sinh_cosh_0.3` | 0.021712536 | 0.009113686 | 0.420x |

## Shared-pair savings

The paired functions remove almost one complete duplicate calculation.

| Bits | Separate `sin` + `cos` / paired | Separate `sinh` + `cosh` / paired |
|---:|---:|---:|
| 1,000 | **2.04x** | **1.98x** |
| 5,000 | **2.10x** | **2.02x** |
| 20,000 | **2.00x** | **1.98x** |
| 50,000 | **2.25x** | **2.18x** |
| 100,000 | **2.27x** | **2.40x** |

## Honest performance boundary

This is a large internal speedup, not yet a broad MPFR victory. At 20,000 to 100,000 bits, ordinary `sin`, `cos`, and the paired `sin_cos` path are generally within about 1.4x to 1.7x of the installed MPFR build. `sinpi` remains roughly 1.6x to 3x slower depending on precision. Direct hyperbolic functions remain roughly 2.6x to 3x slower. Arctangent, `atan2`, and inverse hyperbolic functions remain the clearest slow calls.

The next performance target is therefore not another Taylor-loop rewrite. It is a lower-level inverse-angle and argument-reduction layer using high products, cached reciprocal reduction constants, and a faster certified square-root/half-angle transform.

## Validation

- Complete Make differential suite passed.
- 31 of 31 CTest targets passed.
- 304,045 exhaustive normal-range rounding cases passed.
- 34,125 constrained-range cases passed.
- All five ordinary rounding modes and faithful-neighbor tests remain covered by the inherited and new suites.
- Special values, poles, exact grids, large arguments, and aliasing passed.
- Targeted AddressSanitizer and UndefinedBehaviorSanitizer runs passed for ordinary, special, large, alias, and randomized smoke cases.
- GCC static analysis reported no diagnostics in the two modified source files.
- The independence audit found no imported `mpfr_*` symbols in the production library.

## Files

- `src/trig_fast.c`: common-scale fixed-point kernels and adaptive arctangent blocks.
- `src/trig_hyperbolic.c`: certified dispatch, direct dyadic phase reduction, paired transactional evaluation, and fallback.
- `benchmarks/benchmark_trig_hyperbolic.c`: alternating-order correctness-gated benchmark.
- `scripts/run_trig_hyperbolic_speedups.py`: local sweep and report generator.
- `results/trig_hyperbolic_speedups/`: development measurements and validation logs.
