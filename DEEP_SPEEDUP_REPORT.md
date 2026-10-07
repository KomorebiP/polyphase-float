# Deep hot-path speed refinement

This branch focuses on the two calls that remained clearly behind after the first hot-path pass:

- reciprocal square root
- short correctly rounded radix output

No implemented operation calls MPFR to complete its mathematical work. MPFR is linked only by the optional adapter and by tests and benchmarks.

## Reciprocal square root

The previous route computed an integer root and then divided a large power of two by that root. At one million bits, those two stages consumed roughly comparable time, so allocation changes could not close the gap.

The new route computes a fixed-point inverse square root directly. After normalizing the input to

\[
1\le A<4,
\qquad
\frac12\le X=A^{-1/2}<1,
\]

it recursively obtains an \(h\)-bit approximation and applies the Newton correction

\[
X\leftarrow X+\frac{X(1-AX^2)}2.
\]

The recursive precision satisfies

\[
2h\ge p+10,
\]

so one correction supplies the requested \(p\)-bit work result.

### Truncated correction

Let \(x_h\) be the integer representing the \(h\)-bit approximation. The exact residual is

\[
R=2^{d+2h}-Mx_h^2,
\]

and the scaled correction is

\[
\Delta=\frac{x_hR}{2^{d+3h+1-p}}.
\]

The implementation does not multiply by all of \(R\). It keeps the signed high quotient after a power-of-two cut. If \(c\) low residual bits are omitted, their contribution to the output correction is bounded by

\[
\frac{x_h2^c}{2^{d+3h+1-p}}<2^{-g}
\]

work ulp, where the retained implementation uses \(g=8\). The returned dyadic enclosure includes that truncation error and the recursive approximation error.

This is the difference between the retained kernel and the first failed Newton prototype. The failed version formed full residual and correction products and was slower than the root-plus-quotient route.

### Ternary direction

A narrow enclosure can settle the rounded value while still straddling that value, leaving the ordinary interval routine unable to prove whether the result is above or below the exact reciprocal root.

The new resolver proves the missing relation exactly. For a positive candidate \(r\) and positive input \(x\),

\[
r\gtrless \frac1{\sqrt{x}}
\quad\Longleftrightarrow\quad
r^2x\gtrless1.
\]

This exact comparison also resolves directed rounding when the two interval endpoints select adjacent floating-point values. A targeted odd-exponent test was added because that case exposed the issue.

### Parallel product channels

The fixed-point Newton stage contains large exact products. Balanced products can use the existing three-channel decomposition. The public entry point is

```c
int ppf_rec_sqrt_threads(
    ppf_t *result,
    const ppf_t *input,
    ppf_rnd_t rounding,
    unsigned long workers,
    ppf_context_t *context
);
```

The present kernel exposes three useful product channels. Requesting more workers does not yet create more than three active multiplication channels inside one reciprocal-square-root call.

## Short radix output

The former exact path scaled with the complete source precision even when only 128 digits were requested. The retained path now:

1. rounds one reduced-precision endpoint in the safe direction
2. requests a small number of additional radix digits
3. rounds those digits to the requested length
4. rejects exactness cheaply from exponent and odd-part bounds in the usual large-source case
5. invokes the second endpoint only when the first certificate is genuinely ambiguous

This keeps a 128-digit request nearly independent of whether the source has one hundred thousand or five million bits.

A direct exact conversion of the reduced endpoint was tested and rejected. A large radix exponent still forced construction of a large base power. A custom `mpf_t` scratch representation and a one-conversion boundary certificate were also slower than the retained implementation.

## Development before-and-after results

These rows compare this branch with the immediately preceding hot-path package on the same development host. Five paired runs were used. They are implementation diagnostics rather than portable claims.

| Bits | Operation | Median internal speedup | W-L |
|---:|---|---:|---:|
| 100,000 | 128-digit output | 1.641x | 5-0 |
| 1,000,000 | 128-digit output | 1.705x | 5-0 |
| 5,000,000 | 128-digit output | 1.667x | 5-0 |
| 100,000 | reciprocal square root | 1.490x | 5-0 |
| 1,000,000 | reciprocal square root | 1.363x | 5-0 |
| 5,000,000 | reciprocal square root | 1.430x | 5-0 |

The ordinary cube-root and seventh-root changes were near parity with the prior implementation. They are retained mainly because the input is now exposed to GMP as a read-only significand rather than copied into a second large integer.

## Development comparison with MPFR

The package includes a correctness-gated randomized-order benchmark. The table below is from five development runs; the supplied launcher uses nine by default.

| Bits | Comparison | Paired median |
|---:|---|---:|
| 100,000 | one-thread reciprocal square root vs MPFR 1T | 0.936x |
| 1,000,000 | one-thread reciprocal square root vs MPFR 1T | 0.913x, noisy |
| 5,000,000 | one-thread reciprocal square root vs MPFR 1T | 0.956x |
| 5,000,000 | three-channel single-call latency vs MPFR 1T | 1.573x |
| 100,000 | equal three-worker batch throughput | 1.558x |
| 1,000,000 | equal three-worker batch throughput | 1.985x |
| 5,000,000 | equal three-worker batch throughput | 1.982x |

The three-channel row is a latency comparison against one ordinary MPFR call. The matched batch row gives both implementations three outer workers and three independent jobs, so it is the equal-worker throughput comparison.

Short output is now close to MPFR rather than ahead of it. In the development run it was approximately 3% to 8% slower, depending on precision. The remaining difference is fixed conversion and certification overhead, not work proportional to the source precision.

## Boundaries retained

The following variants were tested and rejected:

- full-product Newton reciprocal square root
- high-level `ppf_t` Newton iteration
- five-product Toom refinement inside reciprocal square root
- direct exact reduced-endpoint radix conversion
- custom GMP floating scratch layout
- one-conversion radix boundary arithmetic

The failures are preserved under `experiments/` and summarized in `FAILED_DEEP_SPEED_EXPERIMENTS.md`.

## Benchmark contracts

The release separates three questions:

- one-thread operation: Polyphase 1T versus MPFR 1T
- single-call latency: three-channel Polyphase versus one MPFR call
- equal-worker throughput: Polyphase outer workers versus MPFR outer workers

Every benchmark process checks the returned value and ternary direction before its timing row is accepted. The benchmark writes each row immediately, so interrupted long sweeps can be resumed.
