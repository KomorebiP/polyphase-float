# Independent Special-Function Block

The independent floating-point library now exposes the complete 24-name special-function and constant family from the implementation inventory. Twenty-two new direct counterparts were added; the existing `pi` and `log(2)` entries remain in the same family.

## Public surface

```text
eint, li2
gamma, gamma_inc, lngamma, lgamma, digamma, beta
zeta, zeta_ui
erf, erfc
j0, j1, jn, y0, y1, yn
agm, ai
const_log2, const_pi, const_euler, const_catalan
```

The production library performs these calculations on `ppf_t` and imports no `mpfr_*` symbols.

## Main architectural result

The block is not a set of wrappers. It introduces one reusable directed special-function evaluator:

\[
\text{function-specific interval builder}
\longrightarrow
[L,U]
\longrightarrow
\text{rounding proof}
\longrightarrow
\texttt{ppf_t}.
\]

The result is accepted only when the interval proves the destination value and ternary direction. Otherwise the working precision grows and the native computation repeats.

## Implemented mathematical routes

| Family | Main route |
|---|---|
| Euler constant | Euler--Maclaurin enclosure of `H_n-log(n)` |
| Catalan constant | `pi/8*log(2+sqrt(3))` plus a positive hypergeometric series |
| Gamma/log-gamma | argument shifting, Stirling/Bernoulli enclosure, reflection |
| Digamma | recurrence, Bernoulli asymptotic expansion, reflection |
| Beta | signed log-gamma composition |
| Upper incomplete gamma | positive-shape series plus exact shape recurrence |
| Ei | defining logarithm-plus-series formula |
| Li2 | reduced series plus real functional transformations |
| Zeta | Euler--Maclaurin, Hasse sum, Bernoulli values, functional equation |
| erf/erfc | alternating series with correlated complement |
| AGM | directed arithmetic/geometric iteration |
| Bessel J | alternating power series and exact ratio certificate |
| Bessel Y | J/log/harmonic representation and recurrence |
| Airy Ai | two interlaced certified power-series branches |

## Correctness evidence

The new block passes:

- all five deterministic ordinary rounding modes
- faithful-rounding neighbor checks
- structured tests at 24, 53, and 96 bits
- deterministic randomized tests
- signed zero, infinity, NaN, poles, and flags
- input/output alias tests
- Euler and Catalan cache, interval, concurrency, and cleanup tests
- 256-bit all-mode tests and selected 512-bit checks
- exact value and ternary-direction comparison with MPFR
- the complete inherited test suite
- the independent-library symbol audit
- clean Make and CMake builds
- 39 of 39 CTest targets
- AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection on the full new special-function suite
- GCC static analysis with no diagnostics in the four new production modules

## Performance boundary

The first special-function kernels are reference implementations. At 128 bits, representative MPFR/Polyphase ratios were approximately:

| Operation | MPFR / Polyphase |
|---|---:|
| Ei | 0.024x |
| Li2 | 0.059x |
| Gamma | 0.011x |
| Upper incomplete Gamma | 0.017x |
| Digamma | 0.035x |
| Beta | 0.009x |
| Zeta | 0.041x |
| erf | 0.040x |
| J0 | 0.024x |
| Y0 | 0.017x |
| AGM | 0.067x |
| Airy Ai | 0.002x |

A value below one means MPFR is faster. This block therefore closes mathematical coverage but deliberately does not claim a special-function speed breakthrough.

## Coverage after this block

Direct inventory coverage rises from 243 to 265 of 349 symbols:

\[
\frac{265}{349}=75.93\%.
\]

The public header contains 338 distinct `ppf_*` declarations, including project-specific contexts, caches, intervals, threaded calls, planning controls, and diagnostics.

## Deferred speed work

The user-requested return to speed optimization is recorded explicitly in `SPEED_RETURN_BACKLOG.md`. It includes both:

1. the previously deferred inverse-angle and dense trigonometric work;
2. new special-function fixed-point, binary-splitting, asymptotic, and shared-kernel work.
