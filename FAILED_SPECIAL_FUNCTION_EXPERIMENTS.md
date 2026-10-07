# Failed, Corrected, and Deferred Special-Function Experiments

This record keeps unsuccessful or incomplete ideas visible instead of allowing them to disappear from the development history.

## Stopping a Bessel series only because the next term is small

The first Bessel termination rule looked only at the magnitude of the next term. That is not sufficient before the term magnitudes have begun to decrease.

The retained rule first proves an exact ratio bound showing that the subsequent alternating terms decrease. Only then is the next term used as a valid enclosure of the remaining alternating tail.

**Initial rule rejected.**

## Stopping an Airy branch only from term size

The two Airy series can have nonmonotone term magnitudes before entering their tail region. The first draft stopped when a term crossed a precision threshold.

The retained implementation separately proves a ratio bound for each branch, then widens the branch sum by a geometric majorant.

**Initial rule rejected.**

## One zeta formula over the entire real line

No single simple sum behaved well across all tested real regions. The implementation now uses separate routes for `s>1`, `0<s<1`, nonpositive integers, and negative nonintegers.

**Universal-formula approach rejected.**

## Inferring Beta pole cancellation from independent Gamma infinities

At singular combinations, `Gamma(a)Gamma(b)/Gamma(a+b)` can involve cancellations that cannot be decided by independently evaluating three divergent factors.

The current baseline rejects unsupported singular pole combinations explicitly rather than manufacturing a finite answer from `Inf/Inf`.

**Unsafe pole inference rejected.**

## Unbounded Airy power-series evaluation

The reference Airy series is useful on moderate real inputs but is not an appropriate universal algorithm for very large positive or negative arguments. The implementation therefore enforces `abs(x)<=500` and records asymptotic Airy evaluation as future work.

**Unbounded series path rejected.**

## MPFR completion fallback

Calling an MPFR special function when the native interval is undecided would make coverage claims impossible to interpret and would violate the independent-library rule.

The production library instead increases its working precision or returns an explicit error.

**Arithmetic delegation rejected.**
