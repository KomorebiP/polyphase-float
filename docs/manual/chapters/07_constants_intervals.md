# Constants and certified intervals

A long string of digits is useful for display, but it is a weak foundation for a numerical constant. The last supplied digit may be rounded in the wrong direction for the caller's purpose, and the string may not contain enough information to answer a higher-precision request. Polyphase Float's constant interface is built around enclosures: lower and upper bounds known to contain the mathematical constant.

This chapter explains how to request a constant, how to export its enclosure, and what an interval decision actually certifies. The complete example is [numeric_constants.c](../../../examples/manual/numeric_constants.c). The independent square-root refinement example in [Numerical recipes](10_numerical_recipes.md) shows how an application can supply its own justified enclosure.

## Request the constant you mean

The public constant family includes four values:

| Function | Mathematical value |
|---|---|
| `ppf_const_pi` | \(\pi\), the circle constant |
| `ppf_const_log2` | \(\log 2\), the natural logarithm of two |
| `ppf_const_euler` | Euler–Mascheroni constant \(\gamma\) |
| `ppf_const_catalan` | Catalan's constant \(G\) |

Euler's constant is not the exponential base \(e\), and `log2` in the constant name denotes the value \(\log 2\). By contrast, the function `ppf_log2(x)` evaluates the base-two logarithm of its argument. Keeping these names distinct avoids a subtle substitution error in formulas.

Initialize the destination at the desired precision and call the selected function with an explicit rounding mode and context. In a deterministic mode, a successful ternary has the same direction meaning as for arithmetic. The destination precision, not the length of a later printed decimal string, determines the computed binary result.

For an upper bound on \(\pi\), request `PPF_RNDU`; for a lower bound, request `PPF_RNDD`. A nearest-rounded approximation does not automatically provide either side. If an application subsequently multiplies a lower bound by a negative number, the inequality direction reverses. Bounds remain meaningful only when every subsequent operation respects signs and monotonicity.

## What a dyadic enclosure contains

The interval-export interface returns GMP integers \(L,U\) and a binary exponent \(e\), with

\[
L2^e\le c\le U2^e.
\]

Both endpoint values are exact because the integer and power-of-two representation is exact. The width is \((U-L)2^e\). It is an absolute width, not a relative-error estimate and not a count of correct decimal digits.

A negative exponent does not mean the constant itself is small. For example, a large integer multiplied by \(2^{-S}\) can represent a number near three. The common scale simply permits both endpoints to be recorded without first rounding them to an ordinary floating-point object.

`ppf_get_constant_interval` accepts a constant identifier, initialized GMP output integers, an exponent output pointer, requested precision, and worker count. It returns status: 0 for success and -1 for failure, rather than a ternary. It has no explicit context argument: its work is governed through the thread-default path. This distinction matters in applications that otherwise use a custom explicit context. Do not assume an unrelated context's allocator or flags govern this export call.

The destination precision used later for rounding the enclosure is independent of the precision requested for interval construction. A practical pattern is to request a somewhat tighter interval, try the desired destination rounding, and refine only if needed. The number of extra bits is a work policy, not a proof by itself.

## The decision requires more than matching endpoints

The public interval assignment call is

```c
ppf_interval_status_t s = ppf_set_dyadic_interval(
    &result, lower, upper, exponent, rounding, &ternary, &ctx);
```

Its three outcomes have different meanings:

| Status | Meaning | Normal caller response |
|---|---|---|
| `PPF_INTERVAL_DECIDED` | The enclosure determines the destination value, ternary direction, and compatible range flags | Use the result and recorded certificate |
| `PPF_INTERVAL_UNDECIDED` | The available enclosure does not establish the full requested decision | Obtain tighter justified bounds and retry |
| `PPF_INTERVAL_ERROR` | The request failed its validation or execution requirements | Diagnose the context and arguments |

A decided result requires the two rounded endpoints to select the same value. It also requires a proven ternary relation and agreeing underflow and overflow decisions. Endpoint agreement alone is insufficient.

To understand the ternary requirement, suppose both endpoints round to a representable number \(r\), but the interval straddles \(r\). Some possible exact values are below \(r\), others above it. The final floating-point value is determined, but its direction relative to the unknown exact value is not. A non-singleton interval containing \(r\) does not establish that the answer is exactly \(r\), either. The correct outcome is undecided.

Likewise, an interval can cross a range-flag boundary while both endpoints happen to select the same stored value. A full decision must resolve that ambiguity. This is why an application's stopping rule should use the returned interval status instead of comparing two printed numbers.

## Undecided is useful information

An undecided result preserves the destination, the caller's ternary output variable, and the existing context flags. This makes refinement convenient: a previous valid result remains available while a new enclosure is being improved. It does not mean the previous value solves the new problem.

Keep a separate Boolean or application status indicating whether the current request has been certified. A destination containing an old value is still a valid object, but validity of storage is not validity of the new numerical answer. The refinement example starts the destination at one and uses a recognizable ternary marker so that this distinction is explicit.

An error has a different contract. Do not extend the undecided preservation guarantee to every error code or every arithmetic API. In particular, a numerical failure can leave a numerical error result, while a governed project-allocation failure has its own rollback rule. These cases are discussed further in [Special functions](09_special_functions.md).

## Walking through the constant example

`numeric_constants.c` creates two 80-bit destinations. It evaluates \(\pi\) directly into the first, exports a 160-bit enclosure, and asks the interval interface to round that enclosure into the second. It checks both value equality and equality of ternary signs.

The expected output is:

```text
Pi: direct evaluation and exported interval agree at 80 bits.
The interval proves the value and its rounding direction.
```

The program deliberately does not test a particular decimal spelling or a cache-hit count. Those observations are not the property being demonstrated. Its useful invariant is that the direct API and the exported enclosure establish the same 80-bit answer and direction.

This agreement is a consistency check within the constant subsystem. The two paths share the constant construction and finalization architecture, so the example is not an independent proof of the formula for \(\pi\). The enclosure's mathematical justification comes from the builder's documented construction; release qualification supplies additional finite tests. Keep those kinds of evidence distinct when describing results to users.

## How the constant builders justify their bounds

The constant implementations use exact integer or rational construction together with outward rounding. The \(\pi\) builder uses a Chudnovsky series and exact binary splitting. Its enclosure accounts for the omitted series tail and the rounding steps used in the scaled result. The natural-logarithm constant uses an exact series derived from

\[
\log 2=2\operatorname{atanh}(1/3).
\]

The finite sum is paired with a bound for the positive remainder, then converted outward to dyadic endpoints. Euler's and Catalan's constants use their documented series and asymptotic interval machinery. The common finalizer does not accept a result merely because the last two approximations agree.

These descriptions explain why an enclosure is available; they are not a substitute for the detailed derivations and implementation reviews. A client usually needs to rely on the public contract, retain the actual exported endpoints when reproducibility matters, and avoid presenting a finite validation suite as a universal theorem about every execution environment.

## Caches retain reusable mathematical information

A constant cache stores a certified interval, allowing it to serve different destination precisions and rounding modes. It does not merely store a nearest-rounded number and reuse that number as if it were exact. A separate rounded-result cache may answer a repeated request more cheaply.

This makes precomputation useful when many calls will request the same constants. `ppf_precompute_constant` and the constant-specific precompute functions allow the application to arrange that work before a latency-sensitive phase. A higher-precision cached enclosure can also help later lower-precision requests. The cache-precision query reports cache state; it is not a certificate for an unrelated expression that uses the constant.

Cache construction and access are synchronized. Clearing a cache deliberately discards reusable work. Reserve explicit cache cleanup for a lifecycle boundary, controlled benchmark, or memory policy rather than clearing it after every call in a repeated numerical workload. The small standalone example clears caches at exit to demonstrate complete cleanup.

Worker-count entry points allow parallel construction where the implementation supports it. A worker request is not a promise of a speedup. Initialization, synchronization, cache hits, problem size, and the selected algorithm affect actual cost. When reporting timing, distinguish cold construction from a warm lookup and record precision, mode, worker policy, and cache state.

## A certificate needs an enclosure argument

`ppf_set_dyadic_interval` validates and rounds the interval supplied by the caller. It cannot establish that an arbitrary interval encloses the mathematical quantity the application has in mind. If a program supplies bounds around a poor approximation without a justified error estimate, a decided result certifies only the decision implied by those supplied bounds.

For a complete application-level certificate, record the target quantity, exact input interpretation, derivation of the bounds, endpoint integers and exponent, destination precision, rounding mode, and interval decision. Include the output ternary and relevant flags if these are part of the claimed result. A rounded decimal rendering can accompany this record for readability, but it should not replace the exact endpoints.

This discipline is especially useful when composing several operations. Each successful scalar call concerns its own exact stored inputs. An enclosure for a complete expression must propagate input uncertainty and intermediate bounds through that expression. The library gives the application reliable building blocks; the application must still connect them to the mathematical question it is answering.
