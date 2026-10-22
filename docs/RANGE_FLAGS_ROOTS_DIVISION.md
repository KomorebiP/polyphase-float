# Range controls, roots, and reusable-denominator division

## Public exponent range

Each execution context stores:

\[
\operatorname{emin}\le e\le\operatorname{emax}.
\]

The short-form range and flag functions operate on the calling thread's
default context. Programs using explicit contexts can continue using the
`ppf_context_*` interfaces.

`ppf_check_range` assumes its input is the correctly rounded result in an
extended exponent range together with the corresponding ternary direction. It
then applies the current range, updates the value when overflow or underflow
occurs, propagates the ternary relation to the exact mathematical result, and
raises the required flags.

`ppf_subnormalize` emulates a fixed-point/subnormal system without adding a
subnormal value category to `ppf_t`. It rounds to the effective precision
implied by the current exponent and `emin`, using the incoming ternary value to
avoid double-rounding errors.
When this step reduces the effective precision, it raises `PPF_FLAG_UNDERFLOW`
even if the value remains exact, matching `mpfr_subnormalize`.
`PPF_FLAG_INEXACT` is raised only when the returned ternary value is nonzero.
This flag rule is specific to subnormal emulation; ordinary arithmetic uses
the context's exponent range and its usual rounding rules.

## General roots

For a positive exact dyadic input

\[
x=M2^e,
\]

and degree \(n\), the implementation chooses a scale \(S\), constructs an
integer radicand, and calculates

\[
a=\left\lfloor R^{1/n}\right\rfloor
\]

with an exact remainder. A nonzero remainder proves

\[
a2^{-S}<x^{1/n}<(a+1)2^{-S}.
\]

The two endpoints are passed to the common interval-rounding engine. If the
interval does not yet decide the destination, the scale is increased and the
calculation is repeated.

Odd roots of negative values negate the positive interval. Even roots of
negative values produce NaN. Signed zero, infinity, reciprocal degrees, and
the older `root` zero-sign convention are handled separately.

`ppf_hypot` forms the exact value

\[
x^2+y^2
\]

and rounds its square root once. It does not round two squares and an
intermediate sum independently.

## Reusable denominator plans

A plan owns an exact normalized copy of the denominator and the destination
precision. For reusable workloads it also stores a portable reciprocal
estimate.

If \(D\) is the normalized positive denominator, the plan constructs

\[
R=\left\lfloor\frac{2^S}{D}\right\rfloor.
\]

For a suitably scaled numerator \(N\), it estimates

\[
q_0=\left\lfloor\frac{NR}{2^S}\right\rfloor.
\]

The estimate is exact or one below the true quotient. The implementation forms

\[
r_0=N-q_0D.
\]

When \(r_0\ge D\), one exact correction produces

\[
q=q_0+1,\qquad r=r_0-D.
\]

Otherwise \(q=q_0\) and \(r=r_0\). The exact remainder supplies sticky
information for the final floating-point rounding decision.

The plan is immutable after construction. Independent numerators may use it
concurrently. Each worker receives its own local context and scratch data, and
flags are merged into the caller's context after the batch.

## Aliasing and failure behavior

A planned output may alias its corresponding numerator. Cross-index aliasing
in a parallel batch is rejected because concurrent writes would make the
result order-dependent.

No calculation in this subsystem delegates to MPFR. Unsupported resource or
representation cases return `PPF_TERNARY_ERROR` and raise the appropriate
context status rather than silently switching libraries.
