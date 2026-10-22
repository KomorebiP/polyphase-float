# Special functions and difficult domains

Special functions often expose a gap between a formula that exists mathematically and a numerical request that can be completed economically. Poles, sign changes, cancellation, large parameters, and finite implementation bounds all affect how an application should use the interface. A useful wrapper must distinguish these cases instead of reporting every unusual outcome as “not enough precision.”

The complete program [numeric_special_domains.c](../../../examples/manual/numeric_special_domains.c) demonstrates four different situations: an exact Gamma value, a finite logarithmic magnitude with a separate sign, a real-domain NaN, and an explicit practical-domain rejection. These are representative calling patterns, not a claim that the example covers every special-function branch.

## Choose the quantity and its convention

The special-function surface includes the Gamma family, beta, zeta, exponential integral, dilogarithm, error functions, Bessel functions of integer order, the arithmetic–geometric mean, and Airy Ai. Consult the function's mathematical convention before replacing a formula from another library. Similar names can refer to a logarithm, a normalized quantity, or a different branch.

| Interface | Quantity or convention |
|---|---|
| `ppf_gamma` | Real Gamma function, with documented poles and special values |
| `ppf_lgamma` | \(\log|\Gamma(x)|\), plus a separate Gamma sign output |
| `ppf_lngamma` | Real \(\log\Gamma(x)\), where Gamma is positive |
| `ppf_digamma` | Logarithmic derivative of Gamma |
| `ppf_gamma_inc` | Upper incomplete Gamma function |
| `ppf_beta` | Beta function with the implementation's stated real-domain rules |
| `ppf_erf`, `ppf_erfc` | Error function and complementary error function |
| `ppf_jn`, `ppf_yn` | Bessel functions of integer order |
| `ppf_ai` | Airy Ai, subject to a practical finite-argument limit |

For positive shape and nonnegative lower limit, the upper incomplete Gamma convention is

\[
\Gamma(a,x)=\int_x^\infty t^{a-1}e^{-t}\,dt.
\]

It is not the lower integral from zero to \(x\), and it is not automatically divided by \(\Gamma(a)\). If a probability formula needs a regularized ratio, that normalization is part of the application's expression and error analysis. The implementation also has real-parameter continuation paths; their existence should not be read as an unrestricted promise of cheap completion for every extreme parameter.

## Gamma: magnitude and sign are separate information

For positive integers, \(\Gamma(n)=(n-1)!\). The example uses the exactly representable input five and verifies that the 80-bit result is exactly 24 with zero ternary. This is a useful sanity check, but it is much easier than evaluating a point close to a negative-integer pole.

For negative nonintegral arguments, Gamma changes sign between poles. At \(x=-1/2\),

\[
\Gamma(-1/2)=-2\sqrt{\pi}.
\]

Its absolute value has a finite real logarithm. `ppf_lgamma` therefore provides a finite logarithmic magnitude and a separate sign of -1. `ppf_lngamma` cannot return a real logarithm of that negative value and reports a numerical NaN under its real-domain convention.

These outcomes are not interchangeable. Exponentiating the magnitude returned by `lgamma` loses the Gamma sign unless the application restores it. Treat the auxiliary sign as part of the result, validate it only after a successful call, and preserve it with the numerical output when storing or passing the result onward.

For large positive arguments, a logarithmic representation often avoids an otherwise unnecessary overflow. An application computing ratios of large Gamma values may benefit from a logarithmic formulation, but cancellation in the difference of logarithms still needs analysis. Subtracting two separately rounded large logarithms is not automatically equivalent to one certified beta or ratio evaluation.

## Poles and nearby points require exact input awareness

A negative integer and a nearby negative noninteger can produce radically different Gamma behavior. The distinction must be made using the stored high-precision value. Converting it to a native floating type for an “is this an integer?” check can destroy the very offset that distinguishes the two cases.

The same principle applies when generating a point near a pole. If the intended input is \(-n+2^{-k}\), construct that dyadic expression at sufficient precision. Writing a long decimal number through a native `double` temporary may round it to \(-n\). A correctly rounded special-function result for the rounded integer would then answer the wrong input question.

Increasing output precision does not repair that change. Reconstruct the original input from its exact integer/rational description or original text at the new precision. Record whether a test is probing the mathematical pole itself, a representable neighboring value, or a decimal value rounded onto one of those points.

A wrapper should classify NaN and infinity explicitly. A documented pole may return an infinity and set a division-by-zero flag without returning `PPF_TERNARY_ERROR`. A real-domain invalid value may return NaN and set the NaN flag. These are specified numerical outcomes, while the sentinel identifies a request that did not produce its ordinary requested result.

## A practical bound is different from a mathematical domain

Airy Ai is mathematically meaningful for every finite real argument. The current implementation nevertheless restricts its ordinary finite-input evaluation to \(|x|\le500\). A finite argument outside that interval produces `PPF_TERNARY_ERROR` with `PPF_FLAG_ERANGE`. This is an implementation boundary, not a claim that Ai is undefined there.

The bound is checked on the exact stored input. A number slightly greater than 500 remains out of range even if conversion to `double` or `long double` would round it back to 500. Applications that validate this boundary should therefore use the library comparison functions, as the example does with `ppf_cmpabs_ui`.

The example deliberately calls Ai at 501 after identifying that the input exceeds the supported interval. It verifies the documented rejection and checks that no project-allocation failure was reported. Its output is:

```text
Gamma(5) = 24 exactly.
lgamma(-1/2) returns a finite log magnitude and sign -1.
lngamma(-1/2) reports a real-domain NaN.
Ai(501) exceeds the implementation domain: ERANGE, not ALLOC.
```

An application would normally turn the same precheck into a clear user-facing message or choose a different supported method. The example makes the rejected call so that the distinction between numerical range status and allocation failure is executable and reviewable.

Integer-order Bessel evaluation has another explicit practical bound: the general nontrivial `jn` and `yn` paths reject order magnitude above 100,000. Special inputs can be resolved before that work is needed. Even below the bound, a large order or argument can be expensive. A numeric cutoff is not a promise of uniform running time throughout the accepted region.

For upper incomplete Gamma at a finite positive lower limit, the negative-shape recurrence admits shape parameters \(a\ge-100000\). A smaller shape is rejected before converting it into a recurrence count. The special cases at a zero or infinite lower limit are handled separately. This is another practical recurrence bound, not the mathematical domain of the defining integral for positive lower limit. Exact-case recognition can avoid some expensive general routes: for example, negative even integer zeta inputs can be recognized from their compact dyadic representation without materializing an enormous integer.

## Complementary functions and cancellation

When the desired quantity is a small complementary tail, prefer its dedicated interface. For example, evaluating `erf(x)` and subtracting its rounded value from one can erase a small `erfc(x)` value. A direct complementary-function call keeps the defining relationship within the evaluator and avoids this particular external rounding step.

The same caution applies to incomplete Gamma formulas assembled from a complete Gamma value minus a lower contribution. Such differences can be much smaller than either term. A formula that is algebraically correct can be numerically fragile if its terms are rounded independently before subtraction.

Near a zero of an oscillatory special function, absolute error and relative error tell very different stories. A small absolute uncertainty can correspond to a large relative uncertainty when the true value is near zero. Choose the accuracy measure that matters to the application. If the sign near a root is important, seek an interval that excludes zero rather than assuming that a displayed nonzero decimal digit proves the sign.

## Certification rejection and allocation rollback

The special-function evaluators use finite working-precision schedules and bounded internal algorithms. An unresolved enclosure, an unsupported practical domain, or an internal representation limit can cause an ordinary numerical rejection. The error sentinel tells the application not to use the destination as a newly computed answer. It does not, by itself, promise that the destination still contains its previous value.

The project-allocation contract is more specific. For the governed public transcendental and special-function entry points, an unrecovered failure of a configured Polyphase allocation raises the sticky allocation flag, returns the sentinel, and preserves initialized numerical destinations. Paired outputs commit together under that contract, and the `lgamma` auxiliary sign is protected with its numerical output. The staging machinery can distinguish a newly failed allocation from an allocation flag that was already sticky before the call.

A numerical rejection without a project-allocation failure may commit a staged NaN or other documented error result. Therefore, code that requires its previous numerical value after *any* unsuccessful request should arrange its own temporary destination and copy only after applying the application's full success test. That test may include classification and domain policy in addition to the sentinel.

This allocation guarantee concerns project-controlled requests and valid initialized objects. It does not intercept GMP's internal allocation behavior or promise recovery from process termination or invalid allocator callbacks. Keep those ownership boundaries visible in a service's resource policy without conflating them with numerical correctness.

## A useful failure-handling sequence

First decide whether the input belongs to the intended real domain and whether it satisfies known practical bounds. Then clear or save the relevant context flags, execute the operation, and check its return contract. If the result is numerical, classify it before treating it as a finite quantity. If the call fails, inspect whether a new allocation failure occurred and retain the original input description for diagnosis.

The response should fit the failure. Correct a mistaken domain convention rather than merely increasing precision. Reconstruct an input that lost its offset during import. Use a logarithmic quantity if the application actually needs a log magnitude. Report a practical-domain limit when the implementation rejects the requested region. Escalate working precision only when the algorithm and application have a justified refinement policy and a finite resource budget.

Tests should include ordinary values, exact cases, points near poles or zeros, practical-boundary neighbors, and invalid-domain cases. A comparison with an independent implementation is valuable evidence over the tested set. Agreement at a few convenient values does not establish all-input correctness or resource-independent completion. The manual examples are intentionally small; the release's broader validation records state their own exact tested ranges and limitations.
