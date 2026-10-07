# Rank-adaptive products and fused arithmetic

This branch moves product selection below multiplication, square, fused
multiply-add, fused multiply-subtract, and reciprocal-square-root refinement.
The arithmetic result is still produced by one exact operation followed by one
rounding into `ppf_t`.

The public floating-point contract is unchanged. Precision belongs to the
destination object, special values retain their signs, every supported rounding
mode is handled, the return value gives the direction relative to the exact
answer, and range and exception behavior remain part of the calculation.

## Product choices

The planner can select:

```c
PPF_PRODUCT_AUTO
PPF_PRODUCT_FULL
PPF_PRODUCT_KARATSUBA3
PPF_PRODUCT_TOOM3
PPF_PRODUCT_WINDOW
```

The explicit entry points are:

```c
ppf_mul_algorithm_threads(...)
ppf_sqr_algorithm_threads(...)
ppf_fma_algorithm_threads(...)
ppf_fms_algorithm_threads(...)
ppf_rec_sqrt_algorithm_threads(...)
```

Convenience threaded calls are also available:

```c
ppf_mul_threads(...)
ppf_sqr_threads(...)
ppf_fma_threads(...)
ppf_fms_threads(...)
ppf_rec_sqrt_threads(...)
```

## Rank 3

For

\[
A=A_0+2^hA_1,
\qquad
B=B_0+2^hB_1,
\]

the three exact products are

\[
z_0=A_0B_0,
\qquad
z_2=A_1B_1,
\qquad
z_1=(A_0+A_1)(B_0+B_1)-z_0-z_2.
\]

The exact product is reconstructed as

\[
AB=z_0+2^hz_1+2^{2h}z_2.
\]

The new implementation can place this exact unrounded product directly inside
a later fused expression. It no longer has to create a rounded floating-point
product before adding the third operand.

## Rank 5

The direct-limb Toom-3 path writes

\[
A(X)=A_0+A_1X+A_2X^2,
\qquad
B(X)=B_0+B_1X+B_2X^2.
\]

It evaluates at

\[
0,\quad 1,\quad -1,\quad 2,\quad \infty,
\]

computes five independent exact products, interpolates with exact divisions by
2 and 3, and reconstructs directly into a limb array.

The rank-5 path is now valid even when fewer than five workers are requested.
The five tasks are scheduled over the available worker pool rather than being
silently replaced by another algorithm.

## Certified window candidate

The window experiment splits

\[
A=A_H2^h+A_L,
\qquad
B=B_H2^h+B_L
\]

and computes every term except the low-low product:

\[
A_HB_H2^{2h}+(A_HB_L+A_LB_H)2^h.
\]

The omitted term is bounded exactly:

\[
0\leq A_LB_L<2^{u+v}.
\]

Both endpoints are rounded. The candidate is accepted only when the value and
ternary direction are both proved. Ambiguous cases are completed by a native
full product, never by MPFR arithmetic.

The method is correct but has not established a stable portable speed advantage,
so it remains an explicit experimental choice.

## Fused arithmetic

For

\[
ab+c
\]

or

\[
ab-c,
\]

the normal finite path now performs:

1. one exact rank-selected product,
2. one exact phase alignment with the addend,
3. one signed addition or subtraction,
4. one final rounding.

The old path constructed two general-purpose GMP integer terms and realigned
them afterward. The new path avoids that second representation layer.

Very large sparse phase gaps retain the exact interval-capable path. No
mathematical operation is delegated to MPFR.

## Reciprocal square root

The truncated Newton reciprocal-root calculation can now force rank 3, rank 5,
or automatic product selection inside its large correction products. Rank 5
was not universally better, but it produced a stable improvement at ten million
bits with five workers in the development sweep.

## Benchmark contracts

The tuner reports three distinct questions.

### Equal-core arithmetic

One Polyphase worker is compared with one MPFR worker.

### Single-call latency

One internally threaded Polyphase operation is compared with one ordinary
single-threaded MPFR call.

### Equal-worker batch throughput

Both libraries receive the same number of outer workers and the same number of
independent jobs.

Every timed result is checked against MPFR before it is retained.

## Development results

The following are paired medians from seven randomized-order runs on the
development host. They are target-machine evidence, not universal thresholds.

### Rank 5 relative to rank 3

| Precision | Operation | Workers | Rank-5 speedup | Result |
|---:|---|---:|---:|---|
| 1,000,000 bits | multiply | 5 | 1.550x | retained by tuner |
| 1,000,000 bits | square | 5 | 1.262x | retained by tuner |
| 5,000,000 bits | FMA | 5 | 1.184x | retained by tuner |
| 5,000,000 bits | square | 5 | 1.682x | retained by tuner |
| 10,000,000 bits | multiply | 5 | 1.436x | retained by tuner |
| 10,000,000 bits | FMA | 5 | 1.401x | retained by tuner |
| 10,000,000 bits | square | 5 | 1.623x | retained by tuner |
| 10,000,000 bits | reciprocal square root | 5 | 1.129x | retained by tuner |

Rank 3 remained better in several neighboring rows. The crossover is not
monotone because each child product crosses GMP's own internal multiplication
thresholds at different sizes.

### Internally threaded latency relative to MPFR 1T

| Precision | Operation | Workers | Selected speedup |
|---:|---|---:|---:|
| 1,000,000 bits | FMA | 5 | 1.677x |
| 1,000,000 bits | multiply | 5 | 1.610x |
| 1,000,000 bits | square | 5 | 1.527x |
| 5,000,000 bits | FMA | 5 | 2.377x |
| 5,000,000 bits | square | 5 | 2.411x |
| 10,000,000 bits | FMA | 5 | 2.391x |
| 10,000,000 bits | reciprocal square root | 5 | 1.939x |

These are latency results, not equal-resource throughput claims.

### Equal-worker throughput

The strongest repeatable equal-worker result remained reciprocal square root:

| Precision | Workers and jobs | Polyphase speedup |
|---:|---:|---:|
| 1,000,000 bits | 3 | 2.009x |
| 1,000,000 bits | 5 | 2.418x |
| 5,000,000 bits | 3 | 1.764x |
| 5,000,000 bits | 5 | 1.980x |
| 10,000,000 bits | 3 | 1.989x |
| 10,000,000 bits | 5 | 1.881x |

Equal-worker multiplication and FMA were mostly near parity. That boundary is
important: internal task decomposition improves the latency of one large call,
while MPFR can use the same cores efficiently across independent outer jobs.

## Portable policy

The portable automatic policy is deliberately conservative:

- full multiplication for small or unbalanced inputs,
- rank 3 for large general multiplication and FMA,
- rank 5 for sufficiently large squares,
- rank 3 for reciprocal square root unless a machine profile says otherwise.

The supplied tuner records better choices by operation, precision, worker count,
and binding policy. Machine-specific results remain outside the source rather
than being presented as universal thresholds.
