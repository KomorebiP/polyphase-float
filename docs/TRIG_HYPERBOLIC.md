# Trigonometric and hyperbolic functions

The independent library now implements the complete trigonometric and hyperbolic families on `ppf_t`:

```text
cos sin tan cosu sinu tanu cospi sinpi tanpi sin_cos
sec csc cot acos asin atan acosu asinu atanu
acospi asinpi atanpi atan2 atan2u atan2pi

cosh sinh tanh sinh_cosh sech csch coth acosh asinh atanh
```

No function calls MPFR to perform its mathematical operation. MPFR is used only by the optional bridge and the external differential tests.

## Correct-rounding architecture

Each nontrivial operation uses directed lower and upper `ppf_t` endpoints. A builder constructs an enclosure

\[
L \le f(x) \le U.
\]

The common evaluator rounds both endpoints to the destination precision and requested mode. A result is accepted only when both endpoints determine the same floating-point value and the same ternary relation. Otherwise the working precision is increased and the enclosure is rebuilt.

This is the same proof pattern used by the independent constants and the logarithm/exponential family.

## Ordinary sine and cosine

For a finite ordinary angle, the library constructs a certified interval for \(\pi/2\), divides the input interval by it, and proves a unique nearest integer \(n\). It then forms

\[
r=x-n\frac{\pi}{2}
\]

with directed arithmetic. The quadrant is \(n\bmod 4\).

On the reduced interval, sine and cosine are enclosed by their alternating Taylor series. The first omitted term bounds the tail. The reduced results are then mapped through the exact quadrant identities.

Tangent, secant, cosecant, and cotangent are formed from the same certified sine and cosine intervals. Poles and signed zeros are handled separately.

## Scaled angles

The `pi`-scaled and unit-scaled functions reduce the exact binary input as an exact rational before multiplying by the cached interval for \(\pi\).

For example, `sinpi(x)` uses

\[
\sin(\pi x),
\]

while `sinu(x,u)` uses

\[
\sin\left(\frac{2\pi x}{u}\right).
\]

Because the input is dyadic, half-integer and quarter-grid cases can be detected exactly. The implementation includes exact values and signed-zero conventions at integer, half-integer, quarter, and the rational \(m\pi/6\) grid where applicable.

## Paired sine and cosine

`ppf_sin_cos` implements the required paired return encoding and alias behavior. In this correctness-first release, it invokes the established sine and cosine evaluators separately. Sharing one reduction and one reduced series pass is an explicit performance target for the next optimization branch.

## Inverse trigonometric functions

The inverse-angle layer is built around a certified arctangent kernel. The reduced arctangent series is

\[
\operatorname{atan}(t)=t-\frac{t^3}{3}+\frac{t^5}{5}-\cdots.
\]

A half-angle transform reduces larger positive arguments before the series is evaluated. The library then uses

\[
\arcsin x=\operatorname{atan}\left(\frac{x}{\sqrt{1-x^2}}\right)
\]

and

\[
\arccos x=\frac{\pi}{2}-\arcsin x.
\]

Exact endpoints, signed zero, infinities, and out-of-domain inputs are handled before interval evaluation.

Scaled inverse functions divide the certified angle by \(\pi\) or multiply it by \(u/(2\pi)\). Exact axis and quarter-turn values are returned directly.

## Two-argument arctangent

`ppf_atan2`, `ppf_atan2u`, and `ppf_atan2pi` implement the complete quadrant map for finite numbers, signed zeros, infinities, and NaNs. The finite general case evaluates the ratio in the correct quadrant and adds or subtracts a certified \(\pi\) interval.

## Hyperbolic functions

Let

\[
a=|x|,\qquad u=e^{-a}.
\]

The six direct hyperbolic functions are enclosed through

\[
\sinh a=\frac{1-u^2}{2u},\qquad
\cosh a=\frac{1+u^2}{2u},
\]

\[
\tanh a=\frac{1-u^2}{1+u^2},\qquad
\operatorname{sech}a=\frac{2u}{1+u^2},
\]

\[
\operatorname{csch}a=\frac{2u}{1-u^2},\qquad
\coth a=\frac{1+u^2}{1-u^2}.
\]

Odd functions recover the sign of the input. Large-argument shortcuts prove saturation, overflow, or underflow before constructing an impractically small exponential.

`ppf_sinh_cosh` implements the paired result and return encoding. As with `ppf_sin_cos`, this release favors reuse of independently validated evaluators over an unverified shared fast path.

## Inverse hyperbolic functions

The implementation uses

\[
\operatorname{asinh}x=
\operatorname{sign}(x)\log\left(|x|+\sqrt{x^2+1}\right),
\]

\[
\operatorname{acosh}x=
\log\left(x+\sqrt{x^2-1}\right),
\]

and

\[
\operatorname{atanh}x=
\frac{1}{2}\log\left(\frac{1+x}{1-x}\right).
\]

Every component is evaluated with directed independent arithmetic. Domain endpoints and poles are handled exactly.

## Practical boundary

This release establishes correctness and full public coverage, not competitive speed. The present ordinary trigonometric path constructs full floating intervals throughout argument reduction and series evaluation. Its development timings are substantially slower than MPFR, especially for sine, cosine, tangent, arctangent, and `atan2` at large precision.

The clearest optimization targets are:

1. one shared reduction and one paired `sin_cos` kernel;
2. a common-scale fixed-point interval representation, analogous to the optimized log/exp backend;
3. binary splitting or rectangular splitting for reduced sine, cosine, and arctangent series;
4. cached multiword reciprocal constants for reduction by \(\pi/2\);
5. shared `sinh_cosh` evaluation rather than two public calls;
6. cancellation-aware formulas for inverse hyperbolic functions near zero and domain endpoints.
