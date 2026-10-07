# Independent arithmetic design

## Representation

A regular value is

\[
x=(-1)^sM2^{e-p},
\qquad 2^{p-1}\le M<2^p.
\]

The significand is stored as an owned little-endian GMP limb array. The top precision bit is set and every bit above the declared precision is zero. This gives one normalized representation at a fixed precision.

## One central rounding operation

Arithmetic produces one of two internal forms:

1. an exact signed limb integer multiplied by a power of two;
2. a certified dyadic interval containing the exact result.

The common rounding layer maps either form to the destination precision. It determines the retained bits, guard bit, round bit, sticky information, increment decision, carry renormalization, exponent event, ternary direction, and exception flags.

No implemented operation performs a sequence of approximate floating-point intermediate steps.

## Addition and subtraction

For exact dyadic inputs

\[
x=a2^e,\qquad y=b2^f,
\]

the general path aligns them at

\[
g=\min(e,f)
\]

and forms

\[
x\pm y=(a2^{e-g}\pm b2^{f-g})2^g.
\]

A same-phase fast path operates directly on normalized limb arrays with `mpn_add_n`, `mpn_sub_n`, and `mpn_cmp`. The general path remains exact and handles cancellation before one final rounding.

## Multiplication and square

The exact product is

\[
xy=(ab)2^{e+f}.
\]

The one-thread path uses `mpn_mul` or `mpn_sqr` and sends the exact result directly to the limb-rounding layer.

For sufficiently large balanced inputs, the threaded path splits

\[
a=a_0+2^ha_1,\qquad b=b_0+2^hb_1
\]

and evaluates three independent products:

\[
p_0=a_0b_0,
\qquad p_2=a_1b_1,
\qquad p_1=(a_0+a_1)(b_0+b_1).
\]

It reconstructs

\[
ab=p_0+2^h(p_1-p_0-p_2)+2^{2h}p_2.
\]

## Division

For

\[
x=a2^e,
\qquad y=b2^f,
\]

the implementation computes an extended exact quotient and remainder. The remainder supplies exact sticky information. A dyadic interval around the quotient is then rounded once to the destination.

The present quotient path emphasizes correctness and independence. Reciprocal/preinverse reuse and direct limb quotient planning remain future performance work.

## Square root

The positive finite input is converted to an exact integer radicand at a selected binary scale. Integer square root with exact remainder gives

\[
a2^{-S}\le\sqrt{x}<(a+1)2^{-S}
\]

when the root is inexact. The common interval layer decides the final rounded result. Exact squares collapse to a point interval.

## Fused operations

Fused multiply-add and multiply-subtract form

\[
ab\pm c
\]

as one exact dyadic expression and round once. This avoids the double rounding of a separate multiply followed by add or subtract.

## Sum and dot product

The sum path aligns all exact dyadic terms and rounds once. The dot-product path computes each exact product independently, then performs one exact reduction and one final rounding. The threaded dot path parallelizes the product leaves.

## MPFR boundary

The independent library imports no MPFR symbols. The optional adapter can import and export values for existing applications and for differential tests. It is not part of the arithmetic implementation.
