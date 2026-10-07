# Transcendental speed refinement

## Scope

This branch changes the implementation of the independent logarithm,
exponential, and power family without changing its public numerical contract.
The production library still computes every implemented result inside `ppf_t`
and GMP arithmetic. MPFR is used only by the optional adapter and by the
correctness gates in tests and benchmarks.

The prior transcendental implementation represented every enclosure endpoint
as a full floating object. A series iteration therefore performed repeated
object initialization, normalization, general floating division, and
allocation. Its operation count also grew roughly as the square root of the
destination precision.

The new implementation adds a certified fixed-point backend:

\[
[L,U]2^{-S}
\]

is represented by two GMP integers with one common binary scale. Inner series
and reconstruction loops now use integer multiplication, exact shifts,
small-divisor operations, and outward endpoint rounding. The existing floating
interval evaluator remains a native fallback when a fixed-point certificate is
not decisive.

## Retained algorithms

### Logarithms

For ordinary precision ranges, the input is normalized and repeatedly square
rooted. The reduced logarithm is evaluated through

\[
\log m = 2\operatorname{atanh}\left(\frac{m-1}{m+1}\right).
\]

The root count is selected from measured precision bands rather than fixed at
\(\lceil\sqrt p\rceil\). The atanh polynomial uses a fixed-point Horner scheme
above the small-precision crossover. At very high precision, a bit-burst path
is available and selected after its measured crossover.

The implementation uses read-only GMP views of the normalized `ppf_t`
significand. It does not copy the complete source into a new integer before
evaluation.

### Exponentials

The argument is reduced by \(\log 2\), then reconstructed from a fixed-point
series. The planner distinguishes two shapes:

- sparse dyadic reduced arguments use a bit-burst reconstruction;
- dense reduced arguments use blocked Taylor evaluation below the measured
  crossover, then bit-burst at larger precision.

This distinction matters because natural `exp(0.125)` is unusually sparse,
while `exp2`, `exp10`, and general powers contain dense constant products.

### Related calls

The fixed-point backend now also serves:

```text
log1p, log2, log10, log2p1, log10p1
expm1, exp2, exp10, exp2m1, exp10m1
general pow
```

`log1p` forms the exact dyadic value \(1+x\) before evaluation. The `m1`
functions subtract one inside the certified fixed-point enclosure rather than
rounding the exponential first. General power composes the fixed-point log and
exp enclosures before the final rounding decision.

## Before-and-after improvement

The previous and optimized independent archives were rebuilt on the same
machine. Each row below is the median of nine randomized paired runs. A ratio
above one favors the optimized implementation.

| Bits | Operation | Previous | Optimized | Internal speedup |
|---:|---|---:|---:|---:|
| 1,000 | log | 96.17 us | 16.35 us | **5.90x** |
| 1,000 | log1p | 191.86 us | 17.34 us | **11.12x** |
| 1,000 | exp | 117.44 us | 12.53 us | **9.39x** |
| 1,000 | expm1 | 125.50 us | 13.18 us | **9.41x** |
| 1,000 | pow | 330.28 us | 37.13 us | **8.88x** |
| 5,000 | log | 1.793 ms | 0.181 ms | **10.12x** |
| 5,000 | log1p | 3.864 ms | 0.181 ms | **20.49x** |
| 5,000 | exp | 2.686 ms | 0.058 ms | **45.11x** |
| 5,000 | expm1 | 8.573 ms | 0.059 ms | **149.45x** |
| 5,000 | pow | 7.048 ms | 0.375 ms | **19.13x** |
| 10,000 | log | 7.383 ms | 0.599 ms | **12.51x** |
| 10,000 | log1p | 14.892 ms | 0.605 ms | **25.09x** |
| 10,000 | exp | 32.608 ms | 0.127 ms | **255.94x** |
| 10,000 | expm1 | 32.882 ms | 0.129 ms | **262.57x** |
| 10,000 | pow | 89.922 ms | 1.169 ms | **73.93x** |
| 20,000 | log | 96.282 ms | 2.123 ms | **45.19x** |
| 20,000 | log1p | 190.343 ms | 2.189 ms | **86.53x** |
| 20,000 | exp | 131.420 ms | 0.283 ms | **459.20x** |
| 20,000 | expm1 | 128.562 ms | 0.288 ms | **442.17x** |
| 20,000 | pow | 355.254 ms | 3.580 ms | **99.22x** |

The integer-power route was already highly specialized and did not improve in
this pass. Its small regression remains visible in the raw data rather than
being hidden.

## Equal-resource comparison with MPFR

Every row is one worker versus one worker at the same destination precision and
nearest-even rounding. Each process verifies the value and ternary direction
against the other implementation before timing. The table shows paired median
speedup, where a value above one favors Polyphase.

| Bits | Operation | Speedup vs MPFR | Result |
|---:|---|---:|---|
| 5,000 | exp | **2.404x** | 9-0 |
| 5,000 | expm1 | **2.223x** | 9-0 |
| 5,000 | exp10 | **1.278x** | 9-0 |
| 5,000 | exp10m1 | **1.329x** | 9-0 |
| 5,000 | pow_ui | **1.150x** | 9-0 |
| 10,000 | exp | **1.085x** | 7-2 |
| 10,000 | exp10 | **1.106x** | 8-1 |
| 10,000 | exp10m1 | **1.107x** | 8-1 |
| 10,000 | pow_ui | **1.237x** | 8-1 |
| 20,000 | exp | **1.426x** | 9-0 |
| 20,000 | expm1 | **1.362x** | 8-1 |
| 20,000 | exp10m1 | **1.431x** | 9-0 |
| 20,000 | pow_ui | **1.262x** | 8-1 |
| 50,000 | exp | **1.650x** | 5-0 |
| 50,000 | expm1 | **1.743x** | 5-0 |
| 50,000 | exp10 | **1.492x** | 5-0 |
| 50,000 | exp10m1 | **1.566x** | 5-0 |
| 100,000 | exp | **1.889x** | 5-0 |
| 100,000 | expm1 | **1.943x** | 5-0 |
| 100,000 | exp2 | **1.074x** | 5-0 |
| 100,000 | exp2m1 | **1.063x** | 5-0 |
| 100,000 | exp10 | **1.664x** | 5-0 |
| 100,000 | exp10m1 | **1.631x** | 5-0 |
| 100,000 | pow_ui | **2.563x** | 5-0 |

The natural exponential input is the exact dyadic value 0.125 and therefore a
favorable sparse workload. The exp2 and exp10 rows provide denser reduction
checks. At 100,000 bits, both exp2 and exp2m1 crossed the installed MPFR build
in all five paired runs.

## Remaining boundaries

The optimized logarithm is dramatically faster than the previous independent
implementation, but it remains slower than MPFR in the tested general case:
roughly 2.0x slower at 5,000 bits and 2.1x slower at 100,000 bits. General
`pow(1.25,3.25)` is also slower because its runtime is dominated by the
logarithm stage.

The next one-thread speed target is therefore not another exponential
reconstruction. It is a faster certified logarithm kernel, most likely a
low-level binary-splitting or bit-burst implementation that avoids full
fixed-point square roots and repeated complete products.

## Benchmark artifacts

```text
results/transcendental_speedups/REPORT.md
results/transcendental_speedups/raw.csv
results/transcendental_speedups/summary.csv
results/transcendental_speedups/paired_comparisons.csv
results/transcendental_speedups/BEFORE_AFTER.md
results/transcendental_speedups/before_after_raw.csv
results/transcendental_speedups/before_after_summary.csv
results/transcendental_speedups_large/REPORT.md
results/transcendental_speedups_exp2_large/REPORT.md
```
