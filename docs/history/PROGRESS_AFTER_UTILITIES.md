# Progress after the independent utility layer

## What can now be done independently

The library can now perform the following complete path without MPFR:

1. initialize a floating object at arbitrary binary precision;
2. assign from C values, GMP values, rationals, or text;
3. calculate with independent arithmetic;
4. compare, classify, round to integers, split integer and fractional parts,
   find adjacent floating values, and select min/max;
5. calculate `fmod`, IEEE remainder, and quotient-bit variants exactly;
6. convert the result back to C, GMP, rational, or text form;
7. maintain its own rounding, exponent range, flags, allocation, and thread
   context.

MPFR is confined to the optional interoperability library and test oracle.

## Completed major layers

- owned normalized binary floating-point representation
- arbitrary per-object precision
- contexts, allocation, scratch storage, and exception flags
- six rounding modes and ternary results
- exact dyadic and certified interval rounding
- portable initialization, assignment, parsing, conversion, and fit tests
- independent addition, subtraction, multiplication, square, division, square
  root, fused arithmetic, scaling, exact sum, and dot product
- complete comparison and classification family
- complete integer-rounding and remainder family
- neighboring values, min/max, positive difference, and exponent access
- Make, CMake, sanitizer, static-analysis, installation, and independence tests

## Function coverage

The public header contains 176 independent callables. Of these, 141 correspond
directly to entries in the 349-symbol inventory, giving 40.40% literal
coverage.

The following inventory families are now fully represented:

- initialization and precision
- combined initialization and assignment
- comparison and classification
- integer rounding and remainder

The portable assignment and conversion families are complete apart from six
compiler-dependent binary128 and decimal interfaces.

## Performance boundary

The new direct same-precision fraction and `modf` paths are competitive and
were faster than the installed MPFR build in several large development rows.
Integer round-to-even reached parity at five million bits. Exact remainder is
still slower than MPFR in the current implementation, although IEEE remainder
is closer than truncated-quotient `fmod`.

The remaining remainder cost is concentrated in general exponent alignment,
large exact quotient construction, and temporary allocation. It is now a clear,
measurable optimization target rather than an unknown semantic gap.

## What remains before a broad arithmetic library

- scalar variants of addition, subtraction, multiplication, and division
- cube root, reciprocal square root, and general signed/unsigned roots
- two-product fused operations and reusable denominator plans on `ppf_t`
- the public range-control and subnormal-emulation family
- independent constant caches and direct `ppf_t` computation of pi and log(2)
- stream and formatted I/O
- random generators
- logarithm, exponential, and power functions
- trigonometric and hyperbolic functions
- special functions
- stable ABI policy, portability matrix, licensing, and external replication

## Current assessment

The independent binary floating foundation is substantially complete. The
library is now usable for real arithmetic and exact utility work rather than
only as an experiment in representation.

A reasonable engineering estimate is:

- independent numerical foundation: 80–90% complete;
- broad binary arithmetic and utility library: 60–70% complete;
- literal MPFR-style public surface: 40.40%;
- total effort toward a mature MPFR replacement: roughly 25–35%, because the
  correctly rounded transcendental and special-function layers dominate the
  remaining work.
