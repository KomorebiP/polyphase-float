# Independent floating-point speed refinement

This branch reduces the amount of temporary conversion and full-width copying in the independent `ppf_t` arithmetic layer. Correctness is unchanged: every successful operation still computes the exact mathematical operation and rounds once at the destination precision. MPFR is used only by the benchmark and differential tests as an external oracle.

## Retained changes

### Direct exponent alignment for addition and subtraction

Finite values are aligned directly from normalized limb arrays whenever the phase span is practical. The result is normalized and rounded once. Extreme phase spans continue through the existing exact native GMP-integer path rather than delegating to MPFR.

Compared with the previous export-align-import implementation, the development run measured approximately 3.2x less time at one million bits and 5.7x less time at five million bits. Addition is still slower than MPFR because it remains memory-bound and performs more general bookkeeping.

### Direct product channels

Large balanced multiplication uses three exact limb products:

\[
p_0=a_0b_0,\qquad p_2=a_1b_1,\qquad
p_1=(a_0+a_1)(b_0+b_1),
\]

followed by

\[
ab=p_0+2^h(p_1-p_0-p_2)+2^{2h}p_2.
\]

The previous threaded path converted each channel through `mpz_t`. The new path writes and reconstructs the limb products directly. Square uses low square, cross product, and high square channels.

The development run measured about 1.12x improvement over the previous threaded channel path at one million bits and 1.29x at five million bits. With internal product threads, single-call multiplication latency was about 1.60x and 2.10x the one-thread MPFR throughput at those two sizes. These are latency comparisons, not equal-worker batch comparisons.

### Direct quotient and remainder

Regular finite division now constructs the scaled numerator from raw limbs, calls the low-level quotient-and-remainder operation, and sends the quotient plus exact remainder sticky information directly to the limb-rounding engine. It avoids the old `ppf_t -> mpz_t -> ppf_t` cycle.

The measured crossover is not universal. The development run was slightly slower than MPFR at one million bits and about 1.11x faster at five million bits.

### Direct power-of-two scaling

Multiplication or division by a power of two is treated as an exponent change plus one exact significand copy whenever the result remains in range. This removed roughly 20x to 28x of the framework's former internal overhead and brought the operation to MPFR parity in the tested range.

### Direct hypotenuse construction

`ppf_hypot` now forms the two exact squares from limbs, aligns and adds them once, and applies the direct square-root path. The development measurements were close to parity, with small local gains around one and five million bits.

### Adaptive divisor plans

Reusable-denominator plans now build a reciprocal only in the precision range where the development measurements justify the setup cost. Outside that range, the plan remains lightweight and uses native direct division. Per-call execution uses raw limbs and exact one-step correction.

In the provisional equal-worker batch test, planned division was faster at the large tested sizes, but the run used only three outer repetitions. The packaged benchmark defaults to nine.

## Benchmark contracts

The benchmark reports three distinct comparisons.

1. One-thread Polyphase versus one-thread MPFR is the equal-core single-operation comparison.
2. Internally threaded Polyphase versus ordinary one-thread MPFR measures single-call latency.
3. Polyphase outer jobs versus MPFR outer jobs gives both methods the same worker count and measures equal-worker batch throughput.

Every timed result is compared with MPFR before it is recorded. A value mismatch aborts the run.

## Development result boundary

The bundled development report uses three randomized outer repetitions and is provisional. It is retained to show direction and internal before/after changes. The PowerShell launcher defaults to nine randomized outer repetitions and five inner samples per row.

The clearest retained conclusions are:

- direct product channels materially reduce threaded multiplication overhead;
- power-of-two scaling should never use the generic exact-integer path;
- direct division can cross MPFR only at sufficiently large precision;
- `hypot` is competitive after removing unnecessary conversions;
- elementary addition remains memory-bound;
- internally threaded latency gains do not automatically imply equal-worker batch gains.
