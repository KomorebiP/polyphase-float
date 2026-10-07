# Failed and conditional deep-speed experiments

## Full-product Newton inverse square root

The first fixed-point Newton implementation used the correct recurrence but formed the complete residual and complete correction product at every precision level. It was roughly twice as slow as the retained root-plus-quotient route in the first million-bit prototype. Truncating lower recursion inputs reduced the loss but did not make the method competitive.

The failure showed that Newton iteration alone is not the optimization. The important step is proving which residual bits can influence the final rounded result.

## High-level floating Newton iteration

Precision-doubling iteration composed from complete rounded `ppf_t` operations remained slower. It allocated and rounded too many complete objects.

Rejected at the high-level object layer.

## Five-product refinement

A five-product Toom-style multiplication was tested inside the truncated Newton kernel. On the development machine it did not improve on the simpler three-channel product graph. Evaluation and interpolation cost more than the additional concurrency saved.

Rejected as a portable default. The broader rank-adaptive multiplication experiments remain available elsewhere in the project.

## Direct exact reduced-endpoint conversion

Replacing GMP floating radix conversion with the existing exact `ppf_get_str` arithmetic on the reduced endpoint was much slower. Even a short significand can carry a very large radix exponent, forcing construction of a large base power.

Rejected.

## Custom GMP floating scratch

A manually managed GMP floating scratch object was tested to remove `mpf_init` and `mpf_clear`. The extra layout work and defensive handling were slower than GMP's ordinary small allocation path.

Rejected.

## One-conversion radix boundary certificate

One extended conversion followed by exact boundary arithmetic was correct, but the base-power and boundary calculations cost more than the second directed endpoint in ambiguous cases.

Rejected.

## Maximum internal threading at small precision

The reciprocal-square-root channel graph does not amortize OpenMP overhead at one hundred thousand bits on the development host. The internally threaded latency row is therefore not expected to win at every small precision.

The automatic threshold remains conservative.
