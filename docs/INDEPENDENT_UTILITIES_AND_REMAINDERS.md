# Independent comparison, integer utilities, and remainders

This layer operates directly on `ppf_t`. MPFR is used only by the differential
tests and optional adapter, not by the production calculations.

## Comparison

A regular value is represented as

\[
x=(-1)^s M 2^{e-p},
\qquad 2^{p-1}\leq M<2^p.
\]

Two regular values are compared from their signs, exponents, precisions, and
normalized significands. Neither input is rounded. Scalar comparisons first
form the corresponding exact dyadic or rational value.

The implementation includes the ordinary comparison family, absolute-value
comparisons, IEEE-style predicates, and total order. NaN is handled separately:
ordinary comparisons raise the range-error flag and return zero, predicates
become unordered, and total order includes the NaN sign bit.

## Integer rounding

For a finite exact dyadic value, write

\[
x=sM2^e,
\qquad M=Q2^k+R,
\qquad 0\leq R<2^k.
\]

`Q` is the truncated integer magnitude and `R` contains all discarded
fractional information. The rounding decision uses the sign, `R`, the halfway
bit, lower sticky bits, and the parity of `Q`.

`ppf_rint` follows the special MPFR-style return convention:

- zero for an exact result;
- sign times one when the input was already an integer but could not be stored
  exactly in the destination precision;
- sign times two when the original input was nonintegral.

The two-stage `rint_*` functions first select the required exact integer and
then round that integer to the destination precision.

## Fractional decomposition

With the same split,

\[
\operatorname{trunc}(x)=sQ,
\qquad
\operatorname{frac}(x)=sR2^e.
\]

The same-precision fast paths write the integer and fractional significands
directly into their destinations. `ppf_modf` computes both exact components
before either output is committed, so the input may alias either output.

## Exact remainders

For finite inputs, the implementation first represents both values on a common
binary scale:

\[
x=X2^g,
\qquad
y=Y2^g.
\]

For `fmod`, the integer quotient is truncated toward zero:

\[
n=\operatorname{trunc}(x/y),
\qquad
r=x-ny.
\]

For IEEE remainder, the quotient is rounded to the nearest integer with ties
to even:

\[
n=\operatorname{roundEven}(x/y),
\qquad
r=x-ny.
\]

The remainder is formed exactly before the one final destination rounding.
`fmodquo` and `remquo` also return the low significant quotient bits with the
sign of `x/y`.

When the exponent difference is enormous, the code does not construct an
impractically large shifted integer merely to obtain a remainder. It uses
modular powers of two and exact modular arithmetic to retain the information
needed by the quotient rule.

## Neighboring values

`nextabove`, `nextbelow`, and `nexttoward` increment or decrement the normalized
significand by one representable unit. The code handles normalization carries,
exponent transitions, finite/infinite boundaries, and signed zero directly.
These operations do not raise inexact, underflow, or overflow flags.

## Selection and positive difference

`min` and `max` implement the signed-zero and one-NaN selection rules before
rounding the selected exact operand to the destination. `dim` returns the
positive difference when the first operand is larger and positive zero
otherwise.
