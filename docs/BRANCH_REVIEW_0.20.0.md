# Special-function branch review

## Scope and evidence

This review follows the Gamma and digamma reflection paths, the real
dilogarithm reductions, the Bessel J/Y series and recurrence paths, and the
Airy Ai power-series certificate in the source distributed with this report.
It checks the mathematical reduction, direction of interval bounds, and
resource-admission decisions separately from the final rounding test.
The arguments below assume the documented containment contracts of the
existing interval arithmetic primitives. They are a source review, not a
machine-checked theorem about the complete C implementation.

`tests/test_release_boundaries.c` supplements the existing independent
special-function and interval audits. It compares final values, ternary
signs and exception flags against MPFR in all five rounding modes, exercises
narrow exponent ranges, and uses exact 512-bit inputs near reflection and
admission boundaries. Its final executed result is recorded in the release
validation evidence. Finite comparisons support the listed cases and do not
replace the enclosure arguments.

## Positive Gamma and digamma foundation

`lgamma_positive_input_interval` and `digamma_positive_input_interval` in
`src/special_gamma.c` shift a positive argument interval by integers. Each
logarithm or reciprocal correction is evaluated as an interval. The shift
loop exits only after the lower endpoint meets its positive threshold.
Thus every represented argument is positive throughout the subsequent
Stirling expansion.

For positive real arguments, the remainder in each of the logarithmic Gamma
and digamma expansions has the sign of, and magnitude at most, the first
omitted term. The code takes the hull of the partial sum and the partial
sum plus that term, then subtracts the interval shift correction. Even if
the maximum Bernoulli term count is reached before the desired smallness
threshold, this remains an enclosure. Final rounding may remain undecidable
and trigger another precision attempt.

Primary reference: [DLMF 5.11, equations 5.11.1–5.11.2 and subsection ii](https://dlmf.nist.gov/5.11).

## Reflection preserves argument uncertainty

For a negative noninteger input `x`, `lgamma_abs_interval` evaluates
`1-x` as a directed interval. It passes that interval to the positive
Gamma foundation rather than substituting a rounded point. Its formula is

```
log|Gamma(x)| = log(pi) - log|sin(pi*x)| - log Gamma(1-x).
```

The sine interval is evaluated at the original exact dyadic input. Its
absolute value must be strictly positive before taking the logarithm.
An unresolved zero crossing fails the builder and permits a precision retry.
The Gamma sign is obtained from the sine sign, with noninteger and nonzero
checks before use.

`build_digamma` similarly computes

```
psi(x) = psi(1-x) - pi*cos(pi*x)/sin(pi*x).
```

Its sine and cosine retain the original input. The interval division rejects
a denominator interval containing zero. No monotonicity of a trigonometric
function on a nontrivial transformed interval is assumed.

Primary reference: [DLMF 5.5.3–5.5.4](https://dlmf.nist.gov/5.5).

## Real dilogarithm reductions

`li2_series` in `src/special_misc.c` handles `|x| <= 1/2`. Its term recurrence
is `t[k+1] = t[k]*x*k^2/(k+1)^2`. Negative inputs use an alternating-series
hull. For nonnegative inputs, after including the current term, the remaining
positive tail is bounded by that term times `|x|/(1-|x|)`. Only the upper
endpoint is enlarged by this positive remainder.

For `x < -1/2`, the transformation `u=x/(x-1)` lands in `(0,1)`. Directed
bounds on the negative denominator give directed bounds on `u`, with the
endpoint ordering preserved. For `x > 1/2`, directed bounds on `1-x` are
kept. `li2_monotone_bounds` requires ordered arguments with upper endpoint
at most one, where the derivative `-log|1-x|/x` is positive.

The positive-input identity uses `log|1-x|`. Beyond one, it is the identity
for the real part of the principal dilogarithm. This follows by differentiating
both sides on `(1,infinity)` and matching the limiting value at one. The
special case `x=1` is handled before taking a logarithm.

Primary references: [DLMF 25.12.1–25.12.6](https://dlmf.nist.gov/25.12) and
[MPFR real-dilogarithm convention](https://www.mpfr.org/mpfr-current/mpfr.html#Transcendental-Functions).

## Bessel interval directions and remainders

`bessel_j_interval` in `src/special_bessel.c` starts from
`(x/2)^n/n!`. For a negative input and even `n`, the power decreases as its
negative argument increases, so the input endpoints are reversed before
forming the directed power endpoints. The exact factorial denominator is
converted downwards and upwards separately.

Successive terms have ratio

```
t[k+1]/t[k] = -(x*x/4)/((k+1)*(k+n+1)).
```

The alternating remainder hull is used only after the following ratio is at
most one. Later denominators increase, so the needed decrease in absolute
term magnitudes persists. Negative integer orders apply the final parity
sign to the whole interval.

`y0_interval` and `y1_interval` use the integer-order logarithmic/harmonic
series. Their tail checks impose a next-step ratio at most one quarter on
the factorial part. Harmonic weights grow by less than a factor two at the
indices admitted by those checks, giving a remaining ratio at most one
half. The symmetric radius twice the current omitted magnitude is therefore
conservative. Higher `Y_n` values use the exact three-term recurrence with
interval operations, so numerical instability can widen an enclosure but
does not justify shrinking it.

Primary references: [DLMF 10.2.2](https://dlmf.nist.gov/10.2),
[DLMF 10.8.1–10.8.2](https://dlmf.nist.gov/10.8), and
[DLMF 10.6.1](https://dlmf.nist.gov/10.6).

## Airy series and exact admission

`build_ai` uses the two power-series branches determined by `Ai(0)` and
`Ai'(0)`. The coefficients use Gamma intervals at `1/3` and `2/3`.
`ppf_special_iv_gamma_decreasing` restricts its argument interval to `(0,1]`
and reverses endpoints, which is the correct Gamma direction there.

The two next-term ratios are `x^3/((3k+3)(3k+2))` and
`x^3/((3k+4)(3k+3))`. The future-ratio guards bound absolute ratios by one
half. A radius twice the first omitted absolute term encloses each remaining
tail. Adding the two interval branches retains cancellation uncertainty.

The public finite-input admission bound remains exactly `|x| <= 500`.
The previous code checked this after a conversion to `long double`, allowing
an input such as `500+2^-400` to round onto the boundary. The revised code
uses `ppf_cmpabs_ui(x,500,ctx)`, avoiding that loss of information. The new
regression checks both signs in every rounding mode.

Primary references: [DLMF 9.2.3–9.2.4](https://dlmf.nist.gov/9.2) and
[DLMF 9.4.1](https://dlmf.nist.gov/9.4).

## Certification and resource failure

The final conversion in `ppf_special_evaluate` is attempted only after a
builder returns an ordered interval. The dyadic-interval routine must decide
one rounded output and its ternary direction. Unresolved intervals retry
with increased precision. The default schedule is limited to twelve
attempts. Exhaustion returns `PPF_TERNARY_ERROR`, sets `PPF_FLAG_ERANGE`, and
publishes NaN according to the existing numerical error contract. Allocation
failure has the separate destination-preservation transaction contract.

The new test uses an internal builder returning `[1,2]` at every precision.
It therefore exercises actual production retry exhaustion deterministically,
without guessing that a selected transcendental input will defeat a specific
working-precision schedule. Airy admission rejection and Bessel order bounds
remain explicit resource policies. No new unlimited-domain or universal
correct-rounding claim is made.
