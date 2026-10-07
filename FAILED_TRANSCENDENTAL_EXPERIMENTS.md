# Failed and corrected transcendental approaches

The first broad transcendental family produced several useful failures.  They
are retained because they define what a correct implementation cannot assume.

## Fixed guard precision for composed functions

The first composition calculated `log`, multiplied it, and evaluated `exp`
with one fixed extra precision.  Ordinary tests passed, but cancellation and
hard-to-round cases failed.

**Boundary:** agreement at one larger precision is not a correct-rounding
certificate.

**Retained solution:** directed interval composition and a Ziv-style retry
loop.

## Rounded `1+x` for `log1p`

Forming `1+x` at the working precision can erase a tiny `x` before the
logarithm begins.

**Boundary:** `log1p` cannot be reduced to ordinary `log` through one rounded
addition.

**Retained solution:** construct a directed interval for the exact sum.

## Numerical detection of exact powers

The interval for `9^(1/2)` correctly enclosed 3, but did not prove that the
exact result equalled 3.  The value was right while the ternary result remained
undecidable.

**Boundary:** an interval can determine the rounded value without proving
exactness.

**Retained solution:** algebraic exact-power detection for dyadic exponents.

## Independent intervals for correlated exact identities

`log10(10)`, `log2(1-3/4)`, and `(1+1/2)^2` exposed the same issue.  Separately
rounded intervals destroyed the exact algebraic correlation.

**Boundary:** narrowing independent intervals is not always enough to recover
an exact identity.

**Retained solution:** exact recognizers for powers of two, powers of ten,
exact `1+x`, and exact integer powers.

## Unbounded exact integer exponentiation

An early integer-power route attempted to materialize the complete exact
significand for every machine-size exponent.  That is unsafe when the exact
integer contains vastly more bits than the destination needs.

**Boundary:** exact-operation semantics do not require materializing every bit
of a gigantic intermediate.

**Retained solution:** strip powers of two first, estimate the useful exact
width, and switch to the certified log/exp evaluator when the exact odd part is
not economical.

## Treating `powr` as `pow`

The ordinary cases agree, but signed zero, zero exponent, infinity, negative
inputs, and `1^Inf` do not.

**Boundary:** shared numerical kernels do not eliminate function-specific
special-value contracts.

**Retained solution:** a separate `powr` domain and special-case layer followed
by the common power evaluator.
