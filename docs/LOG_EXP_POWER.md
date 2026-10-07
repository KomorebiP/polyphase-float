# Correctly rounded logarithms, exponentials, and powers

The independent library now implements the complete 24-function logarithm,
exponential, and power family on `ppf_t`:

```text
log, log_ui, log2, log10, log1p, log2p1, log10p1
exp, exp2, exp10, expm1, exp2m1, exp10m1
pow, powr, pow_ui, pow_si, pow_uj, pow_sj, pown, pow_z
ui_pow_ui, ui_pow, compound_si
```

No function in this file calls MPFR.  MPFR is used only by the separate test
and benchmark executables.

## Shared certified evaluator

Every nonexact path is reduced to one operation:

```text
construct a directed interval at working precision q
round the interval to the requested destination
accept only when value and ternary direction are both proved
increase q and repeat when the decision is ambiguous
```

The evaluator begins with the destination precision plus 80 guard bits.  Each
retry increases the working precision by at least ten percent and 64 bits.
A result is never accepted merely because two ordinary approximations agree.

## Interval representation

A temporary interval consists of two independent `ppf_t` values:

```c
struct interval {
    ppf_t lower;
    ppf_t upper;
};
```

The lower endpoint is calculated with downward rounding and the upper endpoint
with upward rounding.  Addition, subtraction, multiplication, division,
reciprocal, square root, powers of two, and sign changes preserve enclosure.
At the end, both endpoints are exported as exact dyadic integers and sent to
the common independent interval-rounding engine.

## Natural logarithm

For a positive regular input, write

\[
x=m2^n,
\qquad 1\le m<2.
\]

The mantissa is repeatedly square-rooted until it is near one.  Then use

\[
z=\frac{m-1}{m+1},
\]

and

\[
\log m
=2\left(z+\frac{z^3}{3}+\frac{z^5}{5}+\cdots\right).
\]

A directed next-term geometric estimate encloses the omitted tail.  The
square-root reduction is undone by a power of two, and the exact exponent
contribution

\[
n\log 2
\]

is added using the certified `log(2)` interval.

`log2` and `log10` divide the natural-log interval by independently certified
intervals for `log(2)` and `log(10)`.  Exact powers of two and positive integer
powers of ten are recognized before the general path so exact results retain a
zero ternary value.

## Logarithms near one

`log1p`, `log2p1`, and `log10p1` form an exact directed interval for `1+x`
instead of first rounding the sum to the destination precision.  This avoids
losing tiny `x` when its exponent is far below one.

Exact cases such as

\[
\log_2(1-3/4)=-2
\]

and

\[
\log_{10}(1+9)=1
\]

are recognized algebraically.  This is necessary to prove the exact ternary
result; an interval produced by separately rounding correlated quantities can
enclose the exact answer without collapsing to it.

## Exponential

For an exact input `x`, choose an integer `k` such that

\[
r=x-k\log 2
\]

is small.  The reduced exponential is evaluated with a directed Taylor series:

\[
e^r=1+r+\frac{r^2}{2!}+\frac{r^3}{3!}+\cdots.
\]

The reduced argument is halved before the series and restored by repeated
interval squaring.  The final interval is scaled by `2^k`.

`exp2` and `exp10` first multiply the input interval by certified intervals for
`log(2)` or `log(10)`, then call the same exponential interval engine.

The `m1` functions subtract one inside the interval calculation.  They do not
calculate a rounded exponential and then subtract one, which would lose
relative accuracy for small arguments.

Integer inputs to `exp2` and `exp10` use exact dyadic or rational construction.
This preserves exactness and avoids a log/exp round trip.

## Integer powers

Integer powers use exact exponentiation when that is an economical route.  The
base is first written as

\[
a2^b
\]

with all removable powers of two transferred into `b`.  This means powers of
two do not create enormous intermediate significands.

When the exact odd-significand power would be much wider than the destination,
the exact route is abandoned before allocating it.  The operation then uses
the certified general power evaluator.

Negative integer exponents are represented as exact rationals and rounded once.

## General powers

For the general positive-magnitude case,

\[
x^y=\exp(y\log x).
\]

The logarithm, product, and exponential are all interval operations.  The
implementation never substitutes a single rounded `log(x)` into a single
rounded multiplication and assumes a fixed number of guard bits is enough.

Negative bases are permitted only for integer exponents.  The sign is set from
the parity of the exponent.

The special cases for NaN, signed zero, infinity, unit magnitude, and infinite
exponents are handled before the general interval calculation.

## Exact noninteger dyadic powers

Some noninteger powers are exact, including

\[
9^{1/2}=3,
\qquad
81^{1/4}=3.
\]

A numerical interval around `exp(y log x)` may prove the returned value without
proving that it equals the exact answer.  The library therefore detects dyadic
exponents of the form

\[
y=c2^{-m}
\]

with odd positive `c`.  It takes `m` exact integer square roots of the dyadic
base when possible and then performs an exact integer power.  This supplies the
correct zero ternary value.

## `powr`

`powr` follows its stricter domain rules rather than being a synonym for
`pow`.  In particular, negative inputs are invalid, zero to zero and infinity
to zero are invalid, and `powr(1, ±Inf)` is invalid.  Negative zero with a
finite negative exponent returns positive infinity; with a finite positive
exponent it returns positive zero.

## Compound interest function

`compound_si(x,n)` calculates

\[
(1+x)^n.
\]

Special cases are resolved first.  When `1+x` is exactly representable as a
dyadic number, the function uses the exact integer-power engine.  The remaining
cases evaluate

\[
\exp\!\left(n\log(1+x)\right)
\]

through interval composition.

## Present performance boundary

This block establishes correct rounding and a common proof architecture.  It
is not yet a speed result.  The initial logarithm uses repeated square roots
and an atanh series, while the exponential uses conservative Taylor intervals
and repeated squaring.  These are robust reference kernels but they perform
more full-precision arithmetic than MPFR's mature algorithms.

The next optimization pass should focus on:

- binary splitting or rectangular splitting for logarithmic series;
- faster reduced exponential kernels;
- shared tables and cached reduction constants;
- high-product and middle-product primitives;
- fused interval multiplication to reduce endpoint duplication;
- hard-to-round test generation and adaptive guard policies.

The current implementation should remain as a correctness oracle while faster
kernels are introduced underneath the same evaluator.

## Fixed-point speed backend

The original evaluator keeps lower and upper endpoints as complete `ppf_t`
objects. That path remains useful as an independent reference and fallback,
but it is not used first on normal finite inputs.

The speed backend represents every enclosure at one binary scale:

\[
L2^{-S}\le x\le U2^{-S}.
\]

The series loops then use GMP integer operations directly. Conversion back to
a floating object occurs only when the endpoint pair is tested for a final
rounding decision.

### Logarithm policy

The normal path combines square-root reduction with the atanh identity. Root
counts and series organization are selected from measured precision bands. A
bit-burst variant is used only after its measured high-precision crossover.
AGM, Newton, and exact binary-splitting variants were implemented and retained
as failed or conditional experiments; none displaced the fixed-point
root/Horner path over the tested normal range.

### Exponential policy

The reduced fixed-point argument is classified by density. Sparse dyadic
arguments use bit-burst reconstruction. Dense arguments use blocked Taylor
below the crossover and switch later. This avoids optimizing only for the
especially favorable `exp(0.125)` shape while regressing exp2, exp10, or
composed powers.

### Correctness fallback

A fast path returns only after the fixed-point interval proves both the final
value and the ternary direction. An unresolved interval returns control to the
original native directed-floating evaluator. There is no MPFR arithmetic
fallback.
