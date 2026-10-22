# Elementary functions without avoidable information loss

Choosing a mathematically equivalent formula can change the accuracy and cost of a floating-point calculation. A dedicated `log1p` call preserves a small increment that would disappear if the program first formed a rounded `1+x`. A scaled-angle function preserves an exact turn fraction that would be perturbed by multiplying by a rounded value of \(\pi\). These are choices about the problem presented to the library, not merely about the number of guard bits used internally.

The complete program [numeric_elementary.c](../../../examples/manual/numeric_elementary.c) demonstrates both effects. It uses exact dyadic inputs, explicit nearest rounding, and a 53-bit destination, making the examples independent of decimal input conversion.

## Start with the intended expression

The elementary families include natural, base-two, and base-ten logarithms; exponentials and their small-increment forms; integer and general powers; trigonometric functions; inverse angles; and hyperbolic functions. The most useful first question is whether the family already contains a function that expresses the mathematical quantity directly.

| Intended quantity | Prefer | Avoid forming first when accuracy matters |
|---|---|---|
| \(\log(1+x)\) for small \(x\) | `ppf_log1p` | A destination-rounded `1+x` |
| \(\exp(x)-1\) for small \(x\) | `ppf_expm1` | A destination-rounded exponential |
| \(\log_2(1+x)\) | `ppf_log2p1` | A rounded sum followed by `ppf_log2` |
| \(2^x-1\) | `ppf_exp2m1` | A rounded power followed by subtraction |
| \((1+x)^n\) with integer \(n\) | `ppf_compound_si` | A prematurely rounded increment |
| \(\sin(\pi x)\) | `ppf_sinpi` | Multiplication by a rounded constant |
| \(\sin(2\pi x/u)\) | `ppf_sinu` | Conversion of an exact turn fraction to radians |

The dedicated functions keep the defining expression inside their numerical evaluation. This gives them an opportunity to preserve relationships that would already be lost in a sequence of public calls. It does not make the external input exact if the application originally imported an approximation.

## A tiny increment can vanish before the logarithm starts

At 53-bit precision, the spacing immediately above one is \(2^{-52}\). Take the exactly represented input

\[
x=2^{-80}.
\]

It is far smaller than half that spacing. Nearest rounding of `1+x` therefore produces exactly one. A subsequent call to `ppf_log` correctly returns zero for the input it actually receives. The logarithm has not made a mistake; the addition changed the question.

`ppf_log1p` receives the original dyadic \(x\) and evaluates the combined expression. Its result remains positive. `ppf_expm1` similarly preserves the small positive difference between \(\exp(x)\) and one.

The example prints:

```text
log of the rounded 1 + 2^-80 is zero at 53 bits.
log1p(2^-80) and expm1(2^-80) remain positive.
sinpi(1) is exactly zero; sin of rounded pi is not.
```

The positive result may still be inexact. Positivity, representability, and exactness are different properties. The example checks the property relevant to the cancellation problem instead of interpreting every nonzero ternary as failure.

Increasing only the logarithm destination precision cannot undo the first addition's rounding. To recompute the original expression at greater precision, rebuild the inputs and intermediate steps whose information was lost. A higher-precision container holding the same rounded one still represents one.

## Preserve exact angle structure

Ordinary `ppf_sin`, `ppf_cos`, and `ppf_tan` take angles in radians. Their input is the exact stored binary number. If the program obtains an approximation `pi` from `ppf_const_pi`, then calls `ppf_sin(pi)`, it asks for the sine of that binary approximation. Since the approximation is not the mathematical constant \(\pi\), the result need not be zero.

`ppf_sinpi(x)` means \(\sin(\pi x)\). The factor of \(\pi\) is part of the function definition, so the implementation can recognize integer and other exact grid cases before ordinary numerical reduction. At \(x=1\), the result is exactly zero. This is the third observation checked by the example.

For degrees, an exact unit-scaled input can be particularly convenient: `ppf_sinu(x,360,...)` represents \(\sin(2\pi x/360)\). For a system that counts phase in turns, use a consistent exact turn scale. The unit parameter is part of the API's domain and must satisfy its documented requirements; do not substitute a native rounded conversion constant when an exact integer scale is available.

Large angles deserve an input-quality check. A binary value of large magnitude can have many significant bits while still having little resolution in its fractional-turn position. Accurate range reduction computes the function of that stored value. It cannot determine which nearby physical angle was intended if measurement or import discarded the needed low bits.

## Inverse angles and quadrants

Use `ppf_atan2` when both Cartesian coordinates are available and the quadrant matters. Computing an ordinary quotient and passing it to `ppf_atan` can lose quadrant information, can encounter division by zero unnecessarily, and introduces an intermediate rounding. The two-argument function has explicit rules for axes, signed zeros, infinities, and NaNs.

Scaled inverse-angle variants return answers in the corresponding \(\pi\) or unit convention. They are useful when the caller ultimately wants a turn fraction rather than radians. State the convention in interfaces and stored data; the same numerical value can mean a different angle under a different scale.

For `asin` and `acos`, check that a real input is in [-1,1]. A dot product of nominal unit vectors can drift slightly outside this interval after earlier rounding or because the vectors were not exactly normalized. Silently clamping the input changes the mathematical problem. If the application intentionally clamps to accommodate a known modeling tolerance, document that tolerance and retain the original residual for diagnosis.

A similar distinction applies to `atanh`: the finite real interior is (-1,1), with poles at the endpoints. `acosh` has a real domain beginning at one. A domain check should compare the actual `ppf_t` value, not a conversion to `double` that can round a nearby out-of-domain value back onto the boundary.

## Integer powers and general powers have different information

When the exponent is an integer in the application's model, pass it through an integer-power interface such as `ppf_pow_ui`, `ppf_pow_si`, or `ppf_pow_z`, according to its representation. This retains exact parity and avoids treating a rounded floating exponent as if it were the intended integer.

A general negative base has a real result for integer exponents, with sign determined by parity. The expression \((-8)^{1/3}\) illustrates a different issue: a finite binary approximation to one third is not exactly one third. A general real-power call with that exponent does not communicate the intention “take the real cube root.” Use `ppf_cbrt` or an appropriate integer-degree root function when that is the intended operation.

`ppf_powr` has a stricter domain than `ppf_pow`. It is not simply an alternative spelling. In particular, cases involving a negative base or ambiguous zero/infinity exponents follow its documented invalid-value rules. Choose the function based on the mathematical convention needed by the application, then handle its special values explicitly.

Some noninteger powers are exact, such as \(9^{1/2}=3\) and \(81^{1/4}=3\). The implementation contains exact-case routes so that a numerical enclosure need not be forced to prove equality by indefinitely narrowing around a representable answer. An application should still inspect the returned ternary rather than infer exactness from a familiar-looking decimal rendering.

## What the internal enclosure establishes

For nontrivial cases, the implementation constructs directed bounds at a working precision, attempts to settle the requested output, and refines when the decision remains ambiguous. Depending on the function and argument, the first path may use fixed-point integer endpoints or floating interval objects. The public meaning is the requested function of the exact stored inputs, rounded to the destination precision and mode.

For a deterministic rounding mode, a completed endpoint decision includes the value and the applicable rounding direction and range information. Merely obtaining identical nearest-rounded approximations at two working precisions would not establish these facts. The library's certificate-oriented paths therefore use enclosure finalization rather than a “digits stopped changing” test.

There are still finite implementation limits. Iteration counts, workspace sizes, transformed-argument alignment, and available memory constrain execution. The current logarithm/exponential interval layer caps a particular dyadic alignment step at 1,000,000 bits. This is a workspace bound on that representation step, not a universal limit on input precision or exponent. An exact shortcut or a proved saturation path may avoid it; another request can be rejected before an enormous temporary is allocated.

A representation or certification rejection returns the documented error sentinel and status flags. Repeating the same request without changing its representation or work policy is not a general remedy. Nor should an application remove a guard simply to obtain an unchecked approximation. Record which function and inputs failed, then choose an explicitly supported alternative formulation or report the limitation to the caller.

## Compose functions with a clear error model

Successful scalar calls do not automatically give a final expression the same single-rounding contract. For example, `log(x)` followed by multiplication by `y`, followed by `exp`, introduces visible intermediate roundings. `ppf_pow(x,y)` can keep those stages inside one evaluation of the power. Both programs can be useful, but they answer different floating-point specifications.

For an application-level bound, use directed operations and valid interval formulas throughout. Monotone functions allow endpoint propagation only on a domain where the monotonicity assertion holds. Sine over an interval spanning a maximum cannot be bounded by evaluating just its two endpoints. Division requires a denominator interval that avoids zero, or a more elaborate treatment of the singularity.

For an application-level approximation, record input precision and the complete formula, perform a precision study, and describe the result as observed stability unless an error bound has been proved. This distinction is useful even in ordinary engineering work: it tells the next reader whether the uncertainty is from input data, numerical rounding, or an unresolved mathematical bound.
