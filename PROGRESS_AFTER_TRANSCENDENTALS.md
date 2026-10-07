# Progress after logarithms, exponentials, and powers

The complete 24-function logarithm, exponential, and power family is now
implemented on the independent `ppf_t` representation.

## Coverage

```text
208 direct counterparts / 349 inventory symbols = 59.60%
273 total public ppf_* callables
```

The following inventory families are complete:

```text
initialization and precision
combined initialization and assignment
comparison and classification
logarithms, exponentials, and powers
integer rounding and remainder
exponent range and exception flags
```

## Structural progress

The library now contains the full sequence required before trigonometric
argument reduction:

```text
independent object and context
central rounding, range, and flags
assignment, parsing, conversion, and text output
arithmetic, roots, fused operations, sums, and dot products
integer utilities, remainders, and neighboring values
certified pi and log(2)
certified log/exp/power evaluator
```

## Validation result

The block is tested against MPFR over:

- all five deterministic ordinary rounding modes;
- faithful-rounding neighbor checks;
- precisions from 2 through 256 bits in the structured matrix;
- deterministic randomized values at 8, 17, 53, and 96 bits;
- differing input and output precisions;
- narrow exponent ranges;
- NaN, signed zero, infinity, aliases, and negative bases;
- exact powers, exact logarithms, and exact compound results;
- scalar and GMP-integer power interfaces.

## Performance result

The initial certified kernels are slower than MPFR.  This is expected from the
chosen reference algorithms:

- logarithm uses repeated square roots and a directed atanh series;
- exponential uses a conservative Taylor enclosure and repeated squaring;
- general power composes two interval evaluations and an interval product.

The implementation should be treated as the independent correctness baseline
for later optimization, not as a finished performance kernel.

## Next major block

The next broad mathematical block is trigonometric and hyperbolic functions.
Before implementing all 35 names, the reusable core should be built in this
order:

1. certified reduction by pi and pi/2;
2. a shared `sin_cos` kernel;
3. tangent and reciprocal functions from the paired result;
4. inverse trigonometric functions and two-argument `atan2`;
5. shared `sinh_cosh` and inverse hyperbolic kernels;
6. scaled-unit and pi-scaled interfaces.

That block can reuse the newly completed Ziv evaluator, the certified pi cache,
and the full log/exp family.

## Speed-refinement update

The transcendental family now uses a certified fixed-point backend before the
original native interval evaluator. On the development host, this reduced the
preceding independent implementation by approximately:

```text
5,000-bit log:      10x
5,000-bit log1p:    20x
5,000-bit exp:      45x
5,000-bit expm1:   149x
5,000-bit pow:      19x
```

The high-precision exponential family now contains genuine one-thread wins
against the installed MPFR build. The natural logarithm and general
noninteger power remain slower and are the principal transcendental
performance boundary.

This changes the next recommended work from broad API expansion to a focused
certified logarithm kernel, followed by reuse of that kernel inside general
power and inverse functions.
