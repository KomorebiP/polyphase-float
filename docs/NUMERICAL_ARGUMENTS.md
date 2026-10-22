# Mathematical arguments and their implementation obligations

This document connects the numerical algorithms to the enclosure arguments
they need. It is a review map with explicit dependencies, not a formal proof
of the whole library. The accompanying [limits inventory](NUMERICAL_LIMITS.md)
records finite work budgets and missing boundary tests. Neither document
promotes the release to 1.0.

The key distinction is between **soundness on acceptance** and **completion**.
A builder can supply a valid but wide interval. A later stage may correctly
reject it because the interval does not decide rounding. Conversely, a final
rounding test cannot make an invalid interval valid. A finite term or retry
budget does not establish convergence for every representable input.

The arguments below assume valid initialized objects and contexts, exact GMP
integer/rational arithmetic where used, and the stated contracts of the
lower-level directed operations. Those dependencies are named rather than
silently replaced by differential tests. References to earlier review reports
retain their original bounded scope and source identity. All current changes
need the release's current-source qualification receipt.

## How to follow a calculation

| Stage | Required fact | Where it is addressed |
|---|---|---|
| Exact input and domain | The input value, sign, parity, and branch are determined without a lossy scalar conversion. | A02–A04, A07, A10, A12–A13 |
| Reduction | Every transformed argument contains the exact transformed value, with the identity's hypotheses satisfied. | A04–A07, A09–A12 |
| Finite calculation | Exact arithmetic or outward rounding contains the finite expression. | A01–A06 |
| Omitted part | A tail/remainder bound contains everything not calculated. | A04, A06–A12 |
| Final rounding | Every value in the enclosure has the same requested rounded result, ternary sign, and range flags. | A01 |
| Resource/commit policy | Failure is bounded where promised, and outputs obey the numerical/allocation contract. | A13; `NUMERICAL_LIMITS.md` |

## A01. Exact dyadic rounding and interval finalization

For a finite exact value `z*2^e`, the rounder separates the retained integer
magnitude from the discarded remainder. Directed modes increment according
to the result sign and whether anything was discarded. Nearest mode compares
the remainder with half a retained unit; exact halves use the retained
integer's parity. The comparison can use remainder bit length and a
power-of-two test, so it need not allocate an integer with an enormous
halfway exponent. The ternary sign is the sign of stored value minus exact
value, and therefore reverses when the exact value is negated.

For an enclosure `[L,U]*2^e`, monotonicity of each directed/nearest rounding
map gives the value certificate when both endpoints round to the same value
`r`. This alone is insufficient for the return contract. A strict inequality
`r<L*2^e` or `r>U*2^e` proves a common nonzero ternary sign; an exact singleton
can prove zero. An interval containing `r` may leave that sign undecided.
The endpoint calculations must also agree about underflow and overflow.
Agreement of values and ternaries does not imply agreement of range flags.

The source is `round.c`, `limb_round.c`, and the shared dyadic-interval
finalizer. The earlier core review checked these local rules and corrected
a discarded-bit-width bug and a range-flag disagreement bug. Its exact
counterexamples and finite test ranges are retained in the qualification
record; `test_rounding_width` is the durable regression. This does not prove
every caller obeys the finalizer's preconditions.

For conversion of floating endpoints to common-scale integers, first require
finite ordered endpoints. If only one endpoint is zero, choose the nonzero
endpoint's exponent. Zero can be represented at any common scale and must
not force the other integer to expand. If both are zero, exponent zero is
sufficient. For two nonzero endpoints, the minimum of their exact dyadic
exponents gives nonnegative alignment shifts. Checking both shifts before
writing output makes rejection preserve the caller's output integers and
exponent. The 1000000-bit bound is an allocation policy; the integer equality
itself is exact for every admitted shift. The new 12-case boundary probe
checks this policy in `special_common.c`, not all converters in the library.

**Open dependencies:** all primitive implementations, all machine-width
paths, all callers' endpoint containment, and a universal proof of storage,
carry, sticky-bit, and exponent arithmetic. Faithful rounding has its own
allowed-neighbor contract; five-mode comparisons do not establish it.

## A02. Core arithmetic, products, division, roots, and remainders

Exact addition and fused arithmetic reduce to signed integers on a common
binary scale, followed by one destination rounding. A shortcut that discards
low limbs must retain a sufficient error interval or sticky information;
calling the finalizer afterward does not excuse a missing carry bound.
Multiplication algorithms must compute the same exact product or a proved
product window. Algorithm selection and worker counts affect performance,
while interpolation, carries, and interval radii affect correctness.

A divisor plan based on a floor reciprocal obtains a quotient estimate below
the exact quotient. The recorded reciprocal-error bound limits the necessary
correction; the code must check the residual rather than assume the estimate
is exact. `RANGE_FLAGS_ROOTS_DIVISION.md` gives the quotient-correction and
range semantics. Its proof is conditional on the exact/truncated products
used to construct and apply the reciprocal.

For a positive integer radicand `A`, let `r=floor(A^(1/n))`. Then
`r^n<=A<(r+1)^n`. Equality supplies an exact root; otherwise the two
neighboring integers enclose it at the selected binary scale. For negative
odd roots, negate and reverse the endpoints. A reciprocal reverses positive
endpoints. These statements explain the reference root certificate without
requiring a heuristic number of guard bits to guarantee success. The
optimized reciprocal-square-root refinement has additional correction and
product-error obligations.

For remainder, choose the exact quotient integer according to the operation:
truncate for `fmod`, nearest-even for IEEE remainder. Form `x-n*y` exactly
and round once. A large binary shift can be handled through modular powers
of two instead of materializing the full quotient. Low returned quotient
bits are a separate semantic requirement. The derivation is in
[INDEPENDENT_UTILITIES_AND_REMAINDERS.md](INDEPENDENT_UTILITIES_AND_REMAINDERS.md).

**Status:** the reference algebra is recorded, and exact-oracle/core rounding
campaigns give substantial finite evidence. A complete implementation proof
of all product windows, optimized root refinements, reciprocal planning,
parallel interpolation, and platform-width cases remains open. Dispatch
threshold tests and retry-exhaustion tests are separate obligations.

## A03. Conversions and formatted input/output

A finite radix string denotes an exact signed integer times a radix power,
possibly divided by another radix power. Exact GMP integer/rational
construction followed by the core rounder therefore has a straightforward
dependency chain. Binary floating formats similarly denote exact dyadics;
classification of zero, infinity, and NaN precedes conversion.

A fast text-conversion route must bound the logarithm/power approximation
used to choose digits and then resolve boundary comparisons exactly or with
a certified interval. Output digit count, rounding direction, exponent, and
sign are all part of the contract. Large parsed exponents in `assignment.c`
use a guarded `long double` range estimate; a complete mathematical error
bound for the platform's `log2l`, cancellation, and subsequent arithmetic is
still an obligation. A generous empirical guard is not itself a proof.

FPIF admission is structural: validate metadata, size arithmetic, kind, and
finite representation before allocating or committing the result. A test
that refuses a 64 MiB request before allocation proves rejection behavior for
that metadata. It does not demonstrate successful reading of a full 64 MiB
object. See [FPIF_RESOURCE_CONTRACT.md](FPIF_RESOURCE_CONTRACT.md).

**Status:** exact-conversion algebra and many round-trip/boundary regressions
exist. All approximate fast-format branches, large-power estimate bounds,
and conditional machine-format configurations do not yet have a complete
source-linked argument and boundary matrix.

## A04. Reference logarithms, exponentials, and powers

The general logarithm normalizes positive `x` to `m*2^n`, `1<=m<2`, and
repeatedly takes directed square roots. For the reduced value put
`z=(m-1)/(m+1)`. The identity

\[
\log m=2\sum_{k=0}^{\infty}\frac{z^{2k+1}}{2k+1}
\]

has positive terms when `m>=1`. Successive absolute ratios are at most
`z^2`. Once a directed upper bound establishes that this is below one, the
first omitted term divided by `1-z^2` bounds the tail. The source uses a
more conservative factor when reduction guarantees a ratio at most one
half. Undoing square-root reduction and adding `n*log(2)` with certified
constant endpoints preserves containment.

The general exponential reduces by an integer multiple of certified
`log(2)` and halves the residual before its Taylor series. The series term
recurrence is `t[k+1]=t[k]*r/(k+1)`. A directed bound on every later absolute
ratio gives a geometric radius for the omitted terms; repeated squaring
and exact binary scaling restore the original argument. The argument must
contain the exact residual, not only a rounded approximation to it.
`LOG_EXP_POWER.md` describes the common evaluator and near-one variants.

`log1p` forms an interval for `1+x`; `expm1` subtracts one inside the
certificate. Positive-base powers compose `exp(y*log(x))` with intervals.
Negative bases require an exact integer exponent and parity. Exact dyadic
powers and economical integer powers bypass this composition when their
algebraic exactness can be established. Declining an expensive exact
shortcut is sound only because the subsequent evaluator retains the same
input and domain semantics.

The conservative range preflight uses a simple sufficient comparison. For
any base `b>=2`, `b^t>=2^t` when `t>=0`, and `b^t<=2^t` when `t<=0`.
Thus `t>=max(emax,0)` or `t<=min(emin-2,0)`, in the admitted
positive/negative input branches respectively, proves overflow or a value
no greater than half the minimum normal magnitude.
The exact `exp2` halfway case follows nearest-even zero selection. Signed
Gamma values use the corresponding sign-reversed rounding choices. The
portable comparison forms an exact 64-bit integer bound rather than narrowing
it to `long`. These sufficient conditions do not classify every extreme
input. `BRANCH_REVIEW_0.20.0.md` records the detailed source and range checks.

The exact integer `exp2` path sets a dyadic exponent directly. For other
integer exp/m1 shortcuts, `p+64` binary bits and `floor((p+64)/4)` decimal
exponents are construction budgets; `log2(10)<4` justifies the decimal
bit-cost estimate. An `m1` subtraction also admits the endpoint-to-one gap
before exact arithmetic can expand it. These policies can reject a valid
mathematical input; they do not return an unproved approximation.

**Status:** reference identities, sufficient range bounds, and selected
resource admissions are documented and locally reviewed. This is conditional
on the elementary interval operations and constants. It does not cover every
optimized backend, every planner-width conversion, or universal completion.

## A05. Fixed-point, midpoint-radius, and optimized correction paths

The optimized elementary kernels represent `[L,U]*2^-S`, or a ball
`[m-r,m+r]*2^-S`, using exact integers. A correct fixed-point multiply takes
the minimum and maximum endpoint products and applies floor/ceiling after
scaling. For a ball product, an admissible radius must include
`abs(m1)*r2 + abs(m2)*r1 + r1*r2`, together with every discarded-unit error.
A square-root radius needs a valid derivative bound over the whole positive
input interval, plus the integer-root rounding error. An approximate center
may accelerate work; it cannot reduce a proved radius without justification.

The same obligations apply at each step of blocked Taylor/atanh evaluation,
Horner evaluation, binary splitting, bit-burst reconstruction, and table
correction. A truncation bound for the analytic series and a rounding bound
for its finite implementation are distinct. Increasing guard bits may make
a valid enclosure narrower; it cannot validate an incorrect radius formula.

The Halley inverse-angle prepass produces a candidate angle. Certification
must use a directed tangent interval at that candidate, a residual identity
with a denominator bounded away from zero, the correct arctangent branch,
and a final common-scale enclosure. Its seed quality or convergence rate
alone is not a correctness argument. Source: `inverse_angle_fast.c`.

**Open obligation:** this review has inventoried the active variants, guards,
reduction schedules, and attempt caps, but has not proved the complete
fixed-point/ball recurrence and error invariant for every variant in
`transcendental_fast.c`, `trig_fast.c`, and `inverse_angle_fast.c`. Existing
large/random/alias tests are finite evidence. A fast path that accepts an
invalid interval does not reach the reference fallback, so fallback cannot
close this gap. Disabled strategies and macro overrides need their own
review before being included in a supported correctness claim.

## A06. Trigonometric and hyperbolic reductions

For ordinary angles, reduction by certified intervals for multiples of
`pi/2` must either determine the quadrant or retain uncertainty until it
can. Each reduced sine/cosine Taylor calculation requires a bound on the
next and all later term ratios. Alternating next-term hulls require a
proved decreasing regime; positive hyperbolic series require a geometric
tail. Quadrant changes, sign changes, and reciprocal identities transform
whole intervals. Division must reject a denominator interval crossing zero.

Scaled-angle functions admit exact integer/rational grid cases before
ordinary irrational-constant reduction. Those cases should retain exact
zero and ternary semantics. The identity
`atan(t)=2*atan(t/(1+sqrt(1+t*t)))` reduces positive arguments; reciprocal
reduction then uses `pi/2-atan(1/t)`. The inverse functions need the correct
branch and monotonic endpoint order. Hyperbolic/inverse hyperbolic formulas
compose certified exp/log/sqrt intervals, including cancellation near zero
and domain endpoints.

[TRIG_HYPERBOLIC.md](TRIG_HYPERBOLIC.md) records the identities and reference
series. The current fast paths also depend on A05. The million-bit reduction
policy and finite series/retry caps describe completion limits, not an
analytic domain on which an answer is guaranteed.

**Status:** reduction architecture and reference arguments are recorded;
complete all-branch quadrant, scaled-grid, fast-radius, and cap-boundary
proof/coverage remains open. General correctness wording in older algorithm
notes should be read together with this explicitly bounded status.

## A07. Gamma, digamma, Beta, and incomplete Gamma

The positive log-Gamma and digamma builders shift a directed argument
interval upward by integers, retaining all logarithm/reciprocal corrections.
The positive-real first-omitted-term remainder result for the expansions of
`log Gamma(z)` and `psi(z)` supplies a signed hull at each finite truncation.
This result, with its positive-real hypothesis, is stated in
[DLMF 5.11.1–5.11.2 and 5.11(ii)](https://dlmf.nist.gov/5.11).
It does not say the asymptotic series converges. Reaching a Bernoulli cap can
therefore leave a valid but undecidable interval.

Reflection evaluates `1-x` as an interval. The identities for
`log|Gamma(x)|` and `psi(x)` retain `sin(pi*x)`/`cos(pi*x)` at the original
exact input; a possible zero denominator causes refinement or rejection.
Gamma sign is established separately. Beta combines signed log-Gamma
values only in admitted nonsingular cases. The full local branch argument
and references are in [BRANCH_REVIEW_0.20.0.md](BRANCH_REVIEW_0.20.0.md).

For positive shape `a` and `x>0`, write the lower incomplete Gamma as

\[
\gamma(a,x)=x^a e^{-x}\sum_{n=0}^{\infty}
 \frac{x^n}{a(a+1)\cdots(a+n)}.
\]

The next-term ratio `x/(a+n+1)` decreases with `n`. Once its directed upper
bound is below one, a positive geometric tail bounds the remainder.
Subtract this interval from Gamma to enclose the upper incomplete Gamma;
positivity permits replacing a negative lower bound by zero. The series
normalization follows [DLMF 8.7.1](https://dlmf.nist.gov/8.7.E1).
For negative shapes the recurrence

\[
\Gamma(a+1,x)=a\Gamma(a,x)+x^a e^{-x}
\]

is [DLMF 8.8.2](https://dlmf.nist.gov/8.8.E2). The zero-shape case is handled
separately rather than dividing by zero. Every shifted scalar must remain
exact or be represented as an interval; cancellation can widen the result.

For finite `a<0`, the recurrence count is `ceil(-a)`. The exact equivalence

\[
\lceil-a\rceil\le100000\quad\Longleftrightarrow\quad a\ge-100000
\]

justifies the new admission check before conversion to an integer. Its
placement after `x=0` and `x=+Inf` preserves those special routes. This
specific source repair and the all-five-mode allocator-admission regression
have a separate independent local review. It does not prove that executing
the entire admitted 100000-step recurrence is economical or always decisive.

**Open dependencies:** elementary functions and interval primitives, complete
machine-width planner/recurrence arithmetic, all singular/reflected branches,
and extreme cap saturation. Earlier 20 rational Gamma interval certificates
and finite MPFR comparisons remain bounded tests, not a whole-family proof.

## A08. The four constants

For log(2), exact packet leaves and the transfer law represent a finite
product-sum. Each ratio is below 1/9. If the root packet is `(P,Q,T)`, the
endpoints `8*T/(12*Q)` and `(8*T+9*P)/(12*Q)` enclose log(2); outward integer
floor/ceiling retains the bound. The derivation and packet normalization are
in [CERTIFIED_CONSTANTS.md](CERTIFIED_CONSTANTS.md). Term-count estimates
change cost but do not replace the exact tail formula.

For pi, the public builder forces the exact non-adic Chudnovsky path. The
previous core review bounds the scaled series-truncation error below one
unit using the actual integer tail test, the square-root floor contribution
below one unit, and the final nearest quotient by at most half a unit.
The total is below 5/2, inside the implemented radius 4. The Chudnovsky identity
is an external mathematical dependency; exact packet multiplication,
merging, cancellation, and scheduling are implementation dependencies.
A positive result from the tail test does not independently prove all those
packet operations correct.

For Euler's constant, combine `psi(n)=H_(n-1)-gamma` with the positive-real
digamma remainder statement to obtain the hull between
`H_n-log(n)-1/(2*n)` plus its retained Bernoulli corrections and the same
expression plus the next correction. This uses the digamma expansion
itself; no differentiation of a remainder estimate is needed. The cap
still gives an enclosure under that theorem, although certification can fail.

Catalan's builder uses the identity recorded in the MPFR team's
[Algorithms and Proofs, section 5.4](https://www.mpfr.org/algorithms.pdf), with
terms `t_n=1/((2*n+1)^2*binomial(2*n,n))`. Its ratio satisfies

\[
\frac14-\frac{(n+1)(2n+1)}{2(2n+3)^2}
 =\frac{6n+7}{4(2n+3)^2}>0\qquad(n\ge0).
\]

Thus the omitted positive tail starting at `next` is at most `4*next/3`.
The source adds it only to the upper endpoint before the positive 3/8
multiplier. This is valid for every nonnegative index; “eventually below
one quarter” understates the available bound.

**Status:** these mathematical bounds received a prior independent local
core review, conditional on named identities and primitives. The record
contains 64 exported interval checks, 384 rounding/faithful comparisons, and 32
independent exact-rational pi/log(2) checks on the stated prior source.
It is not outside review, and production-byte reuse must be explicitly
verified. Full optimized packet proof, all cap-boundary execution, and
narrow-`unsigned long` coefficient construction remain open.

## A09. Ei, dilogarithm, erf, and erfc

For positive Ei, successive power-series magnitudes have ratio
`x*k/(k+1)^2`. A bound below one on the future ratios yields a positive
geometric tail. For negative Ei the signs alternate. For erf the magnitude
ratio is `x*x*(2*n+1)/((n+1)*(2*n+3))`, with alternating signs. Each ratio
sequence eventually decreases below one. To use a next-term hull, the
source must establish that its stopping test occurs in that decreasing
regime, including interval roundoff and all admitted input sizes. An
absolute-smallness test alone is not a general alternating-tail theorem.
The full source-level implication is retained as an open obligation here.

Dilogarithm starts in `abs(x)<=1/2` with ratio
`x*k*k/((k+1)*(k+1))`. On that region the absolute ratio is at most 1/2.
Positive tails use a geometric majorant; negative tails use an alternating
hull. Outside the region, the real identities transform `x/(x-1)` or `1-x`.
Both endpoints must be retained; directed division through a negative
denominator reverses the appropriate endpoint choice. On `(-infinity,1]`
the real dilogarithm is increasing, so endpoint evaluation bounds a
transformed interval. The branch for `x>1` returns the real-part convention
and uses `log|1-x|`. The detailed reviewed transformations are in
`BRANCH_REVIEW_0.20.0.md`.

`erfc=1-erf` is formed inside the certificate so cancellation remains visible.
It can require more precision or exhaust a budget. A recursive depth cap
for Li2 or a series cap for Ei/erf is not evidence that every mathematical
input in the real domain completes.

**Status:** reduced-series algebra and selected transformation repairs are
recorded. Descending-regime source proofs, complete recursive descent bounds,
large cancellation cases, and cap-boundary coverage remain open.

## A10. Zeta by region

### Euler–Maclaurin region: real s>1

The following derivation records the needed first-omitted-term statement;
the indexing and positive-integral bound were independently checked in a
second local source review. Implementation still depends on outward arithmetic
and exact coefficient construction. Let `m>=1` and `N>=1` be integers. Write the unsummed positive tail as

\[
T_N(s)=\frac1{\Gamma(s)}\int_0^\infty
 \frac{t^{s-1}e^{-Nt}}{1-e^{-t}}\,dt.
\]

This follows by integrating the positive geometric series. The integrand is
integrable at zero for `s>1`. From the partial fraction expansion of coth
in [DLMF 4.36.3](https://dlmf.nist.gov/4.36.E3),

\[
\frac1{1-e^{-t}}=\frac1t+\frac12+
 2t\sum_{j=1}^{\infty}\frac1{t^2+(2\pi j)^2}.
\]

Put `a=2*pi*j`. The finite geometric identity leaves, after the first
`m-1` correction terms, the remainder

\[
\frac{(-1)^{m-1}t^{2m-2}}
 {a^{2m-2}(a^2+t^2)}.
\]

Its sign is fixed and its magnitude is strictly less than
`t^(2m-2)/a^(2m)`. This includes `m=1` with an empty polynomial.
Multiplication by the positive integration weight and summation preserve
both facts; the majorant is integrable at both endpoints. The resulting
first omitted correction is

\[
C_m=\frac{B_{2m}}{(2m)!}(s)_{2m-1}N^{-s-2m+1},
\]

where `(s)_r=s(s+1)...(s+r-1)`. The Bernoulli normalization follows the even
zeta value in [DLMF 25.6.2](https://dlmf.nist.gov/25.6.E2). Therefore the
remainder has the sign of `C_m` and magnitude below `abs(C_m)`.
`zeta_em` forms the finite sum through `N-1`, the integral and half-endpoint
terms, and corrections in that order. The hull of its partial expression
and that expression plus the next correction is the required enclosure,
even at its correction-count cap. A full source proof must also establish
that the implemented rising factorial and powers enclose these exact terms.

### Hasse region: 0<s<1

Define

\[
A_n(s)=\sum_{k=0}^n(-1)^k\binom nk(k+1)^{-s}
 =\frac1{\Gamma(s)}\int_0^\infty
  t^{s-1}e^{-t}(1-e^{-t})^n\,dt.
\]

The equality follows from the Gamma integral and the finite binomial theorem.
Since the integral factor lies between zero and one, `0<=A_n<=1` for `s>0`.
The positive sum `sum(2^(-n-1)*A_n)` has tail at most `2^-N` after `N` terms.
Summing the geometric series under the positive integral identifies the sum
with the eta numerator in [DLMF 25.5.3](https://dlmf.nist.gov/25.5.E3).
Division by `1-2^(1-s)`, which is negative for `0<s<1`, gives zeta. Interval
division must retain that sign reversal and reject a denominator enclosure
containing zero. The 1024-term cap can prevent high-precision certification;
increasing working precision alone does not reduce this fixed analytic tail.

### Nonpositive integers and reflection

At zero, zeta is −1/2. For a negative integer write the exact input as
`M*2^e` using its stored significand. If `e>0`, it is even and therefore a
trivial zero. If `e<=0`, integer admission proves the right shift is exact;
no left expansion of a huge exponent is needed. The new compact-parity
repair returns exact positive zero consistently with the existing branch.
The trivial-zero identity is [DLMF 25.6.4](https://dlmf.nist.gov/25.6.E4).
The negative odd Bernoulli shortcut has a separate 4095 bound. Remaining
negative values use the functional equation and depend on directed Gamma,
trigonometric, logarithm, and power calculations, including exact transformed
arguments and possible range rejection.

**Status:** the two positive-region mathematical majorants are explicitly
recorded; source-to-formula mapping, machine arithmetic, all reflected cases,
and cap-boundary execution remain dependencies. The compact integer repair
has independent local source review and finite allocation-capped regression
evidence. These facts do not prove the entire zeta implementation.

## A11. Arithmetic-geometric mean

For `a>=b>0`, arithmetic averaging decreases the upper sequence, geometric
averaging increases the lower sequence, and both enclose their common AGM
limit. Directed interval operations retain those inequalities. Returning
`[geometric.lower, arithmetic.upper]` is sound if each iteration obeys that
invariant. The 128-iteration limit can make the builder fail to reach the
requested width without invalidating an earlier enclosing bracket. Exact
zero/equality/infinity rules require separate domain handling. Optimized
midpoint-radius AGM used by optional logarithm strategies additionally needs
A05; the reference AGM invariant does not prove its radius updates.

## A12. Bessel and Airy

The Bessel J power series begins with `(x/2)^n/n!`. Its ratio is
`-(x*x/4)/((k+1)*(k+n+1))`. Once the future absolute ratio is at most one,
increasing denominators preserve the decreasing-term condition for the
alternating hull. Negative inputs and negative integer orders require the
correct parity and endpoint reversal. In particular, an even power on a
negative interval decreases as the interval argument increases.

Y0/Y1 add logarithmic and harmonic-weighted terms to J0/J1. The reviewed
tail admission bounds the factorial ratio by one quarter and harmonic
weight growth by less than two, giving a future ratio below one half.
A symmetric radius twice the appropriate omitted magnitude is conservative.
Higher integer orders use an exact three-term recurrence with interval
operations; instability widens the enclosure instead of licensing a tighter
unproved result.

Airy Ai adds two interlaced series. Their next ratios are
`x^3/((3*k+3)*(3*k+2))` and `x^3/((3*k+4)*(3*k+3))`. The future-ratio guards
bound absolute ratios by one half, so twice the first omitted magnitude is
a valid symmetric tail radius for each branch. Initial values depend on
Gamma intervals at 1/3 and 2/3; the helper uses Gamma's decreasing direction
on `(0,1]`. Cancellation between branches is retained.

[BRANCH_REVIEW_0.20.0.md](BRANCH_REVIEW_0.20.0.md) supplies the source-specific
directions and primary identities. The Ai admission comparison uses the
exact input against 500, so `500+2^-400` cannot be admitted by rounding it to
500 in a machine scalar. Bessel order magnitude avoids negating `LONG_MIN`.

**Status:** selected direction/remainder arguments and rational/MPFR interval
checks exist. Whole-family implementation proof, term/recurrence cap tests,
and coefficient-product width checks on every claimed platform remain open.
The admitted domains are practical limits, not a guarantee of completion.

## A13. Admission, retry, allocation, and range rejection

A work budget is mathematically legitimate when success still requires a
valid certificate and exhaustion produces the documented error or fallback.
It need not be proved large enough to solve all inputs unless the public
contract promises that. The admission check must occur **before** converting
a compact huge-exponent number into an enormous integer. A later count check
cannot protect an earlier allocation.

The two new regressions illustrate this distinction. On the prior qualified
source, `a=-2^(2^40)` at 128-bit precision caused SIGFPE in both zeta and
`gamma_inc(a,1)` during impossible integer materialization. A 4 MiB GMP-request
cap was installed, but GMP terminated before its allocation hook was called;
no huge request was actually allocated. The fixes classify parity or reject
the recurrence count from the compact representation first.

The repaired regression executes 51 assertions: huge inputs across five
rounding modes, preserved `x=+Inf` behavior, selected exact/MPFR zeta cases,
and allocator-admission checks at −100000/−100001. The full boundary suite
contains 741 checks and passed in the focused local build. These counts are
case evidence, not a proof of all resource behavior. The report retains
baseline failures, post-repair hashes, process limits, and independent source
review. The current release-wide build matrix is a separate receipt.

Numerical failure and project allocation failure are not interchangeable.
A numerical rejection may publish NaN/ERANGE. Transaction wrappers preserve
the destination when the documented allocator contract requires it. GMP
allocation is a separate external boundary; an application cannot assume
all internal GMP failures return a recoverable project error.

## A14. Random values

A finite random-bit prefix describes a dyadic interval of possible uniform
values. Extending the prefix narrows it. Monotone transforms such as
`-log(u)` reverse the endpoint order where required; paired normal generation
must also enclose its trigonometric transform. A final interval decision
can certify the rounded value associated with the underlying infinite bit
stream, provided the transform and prefix coupling are correct.

Finite retries and the all-zero-prefix limit define a rejection event.
They require a separate distribution argument if a claim is made about
unbiased output conditional on success or about retries performed by a
caller. Seeded comparisons and statistical checks cannot establish that
argument. Source: `random.c`; tests: `test_random_runtime`.

**Status:** the prefix/enclosure architecture is recorded; a complete
implementation/distribution proof and forced prefix-limit tests remain open.

## A15. Evidence and release interpretation

The qualification record distinguishes four layers:

| Layer | What it supports | What it does not establish |
|---|---|---|
| Mathematical derivation | A stated identity, remainder, or admission implication under exact hypotheses | Correctness of every C operation implementing it |
| Source review | The reviewed branches implement the stated implication, conditional on dependencies | All unreviewed variants or all platform widths |
| Finite experiment | The enumerated values, modes, flags, and resource outcomes passed for hashed source/build inputs | A universal theorem or unlimited completion |
| Independent reproduction | Another party repeats the agreed check on the exact candidate | Fulfillment by a second compiler run in the same session |

The accompanying machine-readable inventory binds all 47 production files.
The new focused runs are the 51-case compact regression, 741-case full boundary
suite, and 12-case exact shared-dyadic cap probe. Earlier core/constants,
322027-case numerical, and full compiler/OpenMP/sanitizer qualification
receipts retain their own inputs and dates. They may be reused only with
an explicit byte/dependency comparison, or rerun after a relevant change.

No external reviewer or outside reproducer is claimed by this work. The
remaining proof and boundary obligations in A02–A14 and
`NUMERICAL_LIMITS.md` mean the whole numerical 1.0 gate is still open, even
though the specific compact-input failures are repaired and tested.
