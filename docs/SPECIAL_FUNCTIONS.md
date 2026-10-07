# Independent Special Functions and Remaining Constants

The independent library now implements the complete 24-entry special-function and constant family:

```text
eint, li2
gamma, gamma_inc, lngamma, lgamma, digamma, beta
zeta, zeta_ui
erf, erfc
j0, j1, jn, y0, y1, yn
agm, ai
const_log2, const_pi, const_euler, const_catalan
```

`const_log2` and `const_pi` were already present. This block adds the other twenty-two public counterparts and extends the existing constant cache system to Euler's constant and Catalan's constant.

## Shared certified evaluation

Every nontrivial special function is evaluated through a common directed-interval loop. At a working precision `q`, a function-specific builder produces

\[
L \leq f(x) \leq U.
\]

Both endpoints are rounded to the requested destination precision and rounding mode. The result is accepted only when the interval proves both the floating-point value and the ternary relation to the exact result. An undecided interval causes the working precision to grow and the calculation to repeat. The implementation never completes an operation by calling the corresponding MPFR function.

The shared interval layer supplies alias-safe directed addition, subtraction, multiplication, division, square root, logarithm, exponential, absolute value, powers, and dyadic conversion. It is used by all four source modules in this block.

## Euler's constant

Euler's constant is enclosed from the Euler--Maclaurin expansion of

\[
H_n-\log n.
\]

The implementation forms

\[
H_n-\log n-\frac{1}{2n}
+
\sum_{k\geq1}\frac{B_{2k}}{2k\,n^{2k}},
\]

using exact Bernoulli rationals and directed binary evaluation. The first sufficiently small omitted correction is used to form a two-sided enclosure. The result is stored in the same interval cache system used by `pi` and `log(2)`.

## Catalan's constant

Catalan's constant is evaluated from

\[
G=
\frac{\pi}{8}\log(2+\sqrt3)
+
\frac38\sum_{n=0}^{\infty}t_n,
\qquad
 t_0=1,
\]

with the exact recurrence

\[
\frac{t_{n+1}}{t_n}
=
\frac{(n+1)(2n+1)}{2(2n+3)^2}.
\]

The ratio is eventually below one quarter, giving a simple geometric bound for the remaining positive tail. The resulting dyadic interval is cached independently of the requested rounding mode.

## Gamma family

For positive inputs, `lgamma` shifts the argument upward and applies a directed Stirling expansion:

\[
\log\Gamma(z)
=
\left(z-\frac12\right)\log z-z
+\frac12\log(2\pi)
+
\sum_{k\geq1}
\frac{B_{2k}}{2k(2k-1)z^{2k-1}}.
\]

The recurrence terms removed during argument shifting are accumulated exactly as logarithms. Negative nonintegral inputs use the reflection identity

\[
\Gamma(x)\Gamma(1-x)=\frac{\pi}{\sin(\pi x)}.
\]

`gamma` exponentiates the certified logarithmic enclosure and restores the sign determined from `sin(pi*x)`. `lngamma` returns a real logarithm only where Gamma is positive, while `lgamma` returns `log(abs(Gamma(x)))` and the Gamma sign separately.

`digamma` uses positive-argument recurrence and an asymptotic Bernoulli expansion. Negative nonintegral inputs use

\[
\psi(x)=\psi(1-x)-\pi\cot(\pi x).
\]

`beta` is formed from the signed log-gamma identity

\[
B(a,b)=\frac{\Gamma(a)\Gamma(b)}{\Gamma(a+b)}.
\]

`gamma_inc` uses a positive-shape lower-gamma series, subtracts it from Gamma, and extends the result to zero and negative real shape parameters through the exact upper-incomplete-gamma recurrence.

## Exponential integral and dilogarithm

The real exponential integral follows

\[
\operatorname{Ei}(x)
=
\gamma+\log|x|
+
\sum_{k=1}^{\infty}\frac{x^k}{k\,k!}.
\]

Positive tails use a geometric majorant once the exact term ratio is below one. Negative arguments use alternating enclosures.

The real dilogarithm starts from

\[
\operatorname{Li}_2(x)=\sum_{k=1}^{\infty}\frac{x^k}{k^2}
\]

on a reduced interval and recursively uses functional identities to move larger positive and negative arguments into that interval. The exact value

\[
\operatorname{Li}_2(1)=\frac{\pi^2}{6}
\]

is handled separately.

## Zeta

The real zeta function uses separate algorithms by region:

- Euler--Maclaurin summation for `s > 1`
- a Hasse-type globally convergent sum for `0 < s < 1`
- exact Bernoulli values at nonpositive integers
- the functional equation for remaining negative nonintegral inputs

The pole at `s = 1`, trivial zeros, infinities, and NaN are handled explicitly.

## Error functions

`erf` uses the alternating power series

\[
\operatorname{erf}(x)
=
\frac{2}{\sqrt\pi}
\sum_{n=0}^{\infty}
\frac{(-1)^n x^{2n+1}}{n!(2n+1)}.
\]

The first omitted term encloses the alternating tail once the terms are decreasing. `erfc` is evaluated as the correlated interval expression `1-erf(x)` so the subtraction remains inside the certificate.

## AGM

The arithmetic-geometric mean iterates

\[
a_{n+1}=\frac{a_n+b_n}{2},
\qquad
b_{n+1}=\sqrt{a_nb_n}.
\]

Directed interval arithmetic is retained until both sequences overlap tightly enough to settle the destination rounding.

## Bessel functions

`J_n` uses its exact power series and accepts the alternating tail only after an exact ratio test proves that subsequent terms decrease.

`Y_0` and `Y_1` combine `J_0`, `J_1`, logarithmic terms, harmonic-number series, and the finite `-2/(pi*x)` contribution. Their nonalternating tails are widened with an exact geometric majorant. Higher integer orders use the standard three-term recurrence from the certified order-zero and order-one enclosures.

## Airy Ai

`Ai(x)` is evaluated from its two interlaced power-series branches, with initial coefficients built from `Gamma(1/3)` and `Gamma(2/3)`. Each branch uses an exact ratio test and a symmetric geometric-tail enclosure.

The present implementation explicitly limits the practical Airy domain to `abs(x) <= 500`. Larger arguments return `PPF_TERNARY_ERROR` with the range-error flag rather than attempting uncontrolled allocation or delegating to MPFR.

## Constant caches

The public cache interface now covers four constants:

```c
PPF_CONSTANT_PI
PPF_CONSTANT_LOG2
PPF_CONSTANT_EULER
PPF_CONSTANT_CATALAN
```

Euler and Catalan support ordinary, threaded, uncached, precompute, interval-export, cache-clear, and cache-free operations through the same machinery as the older constants.

## Current performance position

This block is a correctness and coverage release. The current kernels are conservative reference algorithms built from complete interval objects. They are substantially slower than MPFR across the representative benchmark, especially Gamma/Beta, Bessel Y, and Airy Ai. The benchmark is retained to define the optimization boundary rather than to imply a speed win.

The explicit return-to-speed-work plan is in `SPEED_RETURN_BACKLOG.md`.

## Practical-domain boundaries

- `Ai` is limited to `abs(x) <= 500` in this implementation.
- Very high Bessel orders or unusually large arguments can require substantial time and memory.
- Beta values at singular pole combinations are rejected explicitly instead of being guessed from cancellation of infinities.
- The reference series and asymptotic planners are validated over the structured, randomized, and large-precision test domains included in the package; universal resource-independent completion for every representable extreme input is not claimed.
- No operation falls back to MPFR arithmetic.
