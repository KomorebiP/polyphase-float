# Progress after independently certified constants

## What changed

The independent library can now compute its first two mathematical constants
without entering the MPFR object model:

\[
\pi
\qquad\text{and}\qquad
\log 2.
\]

Each calculation produces an exact dyadic enclosure, and the existing
independent rounding engine converts that enclosure to the destination
`ppf_t`. The same calculation supports every rounding mode, ternary direction,
range behavior, and exception flags.

## Completed structural layers

- owned arbitrary-precision floating objects
- contexts, memory ownership, flags, and exponent ranges
- central exact and interval rounding
- initialization, assignment, parsing, and conversion
- core arithmetic, fused operations, reductions, roots, and remainder
- complete comparison and integer utilities
- reusable-denominator division
- exact constant interval caches
- per-rounding-mode completed-result caches
- generic precision escalation for correctly rounded constants
- independently certified `ppf_const_pi`
- independently certified `ppf_const_log2`

## Public coverage

The independent header now has direct counterparts for 184 of the 349 symbols
in the implementation inventory:

\[
184/349=52.72\%.
\]

The header contains 240 public `ppf_*` callables after including contexts,
threading, cache control, reusable plans, intervals, and diagnostics.

## Performance result

On the development host at one million bits:

- cold one-thread pi was about 2.25 times as fast as `mpfr_const_pi`;
- four-worker pi single-call latency was about 4.34 times as fast as one
  ordinary MPFR call;
- one-thread log(2) was about 17 percent slower than MPFR;
- four-worker log(2) single-call latency was about 1.63 times as fast as one
  ordinary MPFR call.

The internally threaded rows are latency comparisons rather than equal-worker
batch comparisons.

MPFR still has a substantially faster warm constant cache. The Polyphase cache
is correct and thread-safe, but large-result copying and lock overhead remain.

## Current engineering position

The independent numerical foundation is now close to complete. The remaining
work is increasingly mathematical rather than representational:

- general logarithms
- exponentials
- powers
- rigorous argument reduction
- trigonometric and hyperbolic functions
- special functions
- formatted output, random distributions, and remaining compatibility layers

The next major milestone is the first general correctly rounded transcendental
family. The constant cache and precision-escalation loop developed here are the
foundation for that work.
