# Numerical recipes and reproducible results

A numerical recipe should say what its inputs mean, what result it promises, and how it decides that enough work has been done. This chapter connects the library's arithmetic and interval interfaces to those application-level questions. The recipes distinguish three increasingly strong observations: a value was computed successfully, repeated calculations look stable, and a justified enclosure certifies a particular rounding decision.

Two complete programs accompany the discussion: [numeric_import.c](../../../examples/manual/numeric_import.c) compares decimal text with a native floating input, and [numeric_refinement.c](../../../examples/manual/numeric_refinement.c) certifies a 53-bit square root using an independently constructed exact interval. Both programs check their claims before printing their expected output.

## Recipe: import the information you actually possess

Suppose a data file contains the decimal text `0.1`. There are at least three different objects the application might intend:

1. the exact rational number \(1/10\);
2. the nearest binary value at a selected Polyphase precision;
3. the binary value already stored in a native `double`.

These are related, but they are not identical. A binary floating-point number with finite precision cannot equal \(1/10\), because the reduced denominator contains a factor of five. A direct decimal parse can round that rational to the selected precision. Importing an existing native `double` instead preserves the native value, including its earlier rounding.

The import example assumes a 53-bit binary `double`, checks that platform condition, and creates two 256-bit destinations. It parses the text into one and imports the C literal through `ppf_set_d` into the other:

```c
ppf_set_str(&decimal, "0.1", 10, PPF_RNDN, &ctx);
ppf_set_d(&native, 0.1, PPF_RNDN, &ctx);
```

These two calls have different return contracts. The complete program checks status zero for `ppf_set_str`. It checks exact ternary zero for this particular `ppf_set_d`, because the already-rounded native binary value fits exactly into 256 bits. That exact import does not mean the native input equals the intended decimal rational.

To compare errors without another floating approximation, the example exports both stored values to GMP rationals and subtracts the exact rational \(1/10\). It compares the absolute rational errors. The 256-bit decimal parse is closer, though still not exact. It then widens the native-import object to 512 bits and verifies that its rational value is unchanged.

Expected output:

```text
Neither binary value is exactly 1/10.
The 256-bit decimal parse is closer to 1/10.
Widening the native import preserves its old value.
```

If the input is already a `double` supplied by another API, preserving that value may be precisely the right operation. The problem arises only when the program claims to have imported the original decimal or measured quantity more accurately than the available data allows. Keep original text or an exact integer/rational representation when later recomputation at higher precision is part of the workflow.

## Recipe: separate computation precision from output formatting

Choose the computational precision before the calculation. Choose the number of display digits afterward, based on what the output is intended to communicate. Printing more digits does not add information, and printing fewer digits can hide a meaningful difference between two internal values.

For reproducibility, retain the binary value or an exact dyadic export in addition to a human-readable decimal string. `ppf_get_z_2exp` expresses a finite regular value as an integer times a power of two. That representation makes the stored value unambiguous. It is especially useful when a later reviewer must distinguish input rounding from algorithmic differences.

For user-facing results, avoid implying that all printed digits are independently established digits of the mathematical target. A formatter can accurately print a stored approximation whose input or modeling error is much larger than its last displayed digit. Report an error bound or uncertainty separately when one is available, and state how it was obtained.

## Recipe: use precision refinement as a diagnostic

A practical precision study recomputes the entire expression at several precisions, for example 64, 128, and 256 bits. Recreate inputs from their original description at each stage. Reusing a low-precision intermediate after widening it defeats the purpose of the study.

Compare results at a common target precision or compare their exact difference. Otherwise the display itself can make unrelated outputs look equal or make harmless low-order changes look alarming. Record whether every call completed successfully, whether the result remained finite, and whether the same range policy was used.

Stability is useful evidence. It can reveal an accidental native conversion, insufficient precision in one intermediate, a cancellation problem, or an unexpected dependence on evaluation order. However, two calculations can share the same input error, formula defect, or biased approximation. Agreement at two precisions does not prove a correct final rounding decision.

Give an approximation-based algorithm a stopping rule that matches its claim. If the application only requires observed stability to a stated display tolerance, label that outcome accordingly and set a finite retry limit. If it requires a certified binary result, construct or obtain justified bounds and use a certification test. Increasing precision is a work strategy; the stopping test supplies the evidence.

## Recipe: build an exact enclosure for a square root

The refinement example targets \(\sqrt2\). For a positive integer scale \(k\), form the exact integer

\[
R=2\cdot2^{2k}=2^{2k+1}
\]

and use GMP integer square root with remainder to obtain

\[
L=\lfloor\sqrt R\rfloor.
\]

Because \(R\) is a power of two with an odd exponent, it is not an integer square. Thus the returned remainder is positive and

\[
L^2<R<(L+1)^2.
\]

Taking positive square roots and dividing by \(2^k\) proves

\[
L2^{-k}<\sqrt2<(L+1)2^{-k}.
\]

This is an enclosure argument, not an estimate inferred from agreement of approximations. It uses exact integer arithmetic and supplies endpoints whose width is exactly \(2^{-k}\). The program checks the nonzero remainder explicitly rather than silently assuming that every future variant of the recipe has the same irrational target.

The central construction is short:

```c
mpz_set_ui(radicand, 2);
mpz_mul_2exp(radicand, radicand, 2u * k);
mpz_sqrtrem(lower, remainder, radicand);
mpz_add_ui(upper, lower, 1);

ppf_interval_status_t s = ppf_set_dyadic_interval(
    &result, lower, upper, -(ppf_exp_t)k,
    PPF_RNDN, &ternary, &ctx);
```

The complete program supplies initialization, checks, a finite sequence of scales, and cleanup. It never treats a failed allocation or an interval error as an instruction to continue with unchecked bounds.

## Recipe: refine until the requested decision is established

The destination stays at 53 bits throughout the square-root example. Only the enclosure scale changes. The program tries 8, 16, 32, 64, and, if necessary, 128 bits of fixed-point scale. This separates *target precision* from *work used to establish the target result*.

At the first three scales, the enclosure is too wide to settle the 53-bit request. The program verifies that the undecided call leaves its initial destination value and ternary marker unchanged. At scale 64, the enclosure determines the result and proves that the nearest 53-bit value is above the exact square root.

Expected output:

```text
scale 8: refine
scale 16: refine
scale 32: refine
scale 64: certified 53-bit sqrt(2), rounded upward
```

Here “rounded upward” describes the sign of the result's error under nearest rounding. It does not mean that the requested mode was `PPF_RNDU`. A nearest-rounded answer can lie on either side of its exact target; the ternary records which side was selected in this particular case.

If the finite scale schedule ends without a decision, the program exits unsuccessfully and says that it did not obtain its certificate. It does not promote the last approximation to a certified answer. An application can choose a larger schedule or a different enclosure method, but it should report the outcome actually established by the work it performed.

## Recipe: record a certificate that another reader can inspect

For the square-root recipe, a compact reproducibility record consists of the exact target expression, scale \(k\), integer endpoints, endpoint exponent \(-k\), target precision, rounding mode, interval status, ternary, and context range. The integer-square-root remainder supplies a direct check of the enclosure construction.

For a constant, the record can contain the exported enclosure and the constant identifier, together with the construction/validation provenance on which the enclosure relies. For a general expression, record how input intervals and intermediate bounds were propagated. A collection of endpoints without an enclosure argument is not yet a certificate for the intended mathematical expression.

A numerical report should also state its boundaries: tested input ranges, platform and dependency versions, random seed if sampling was used, any retry or resource cap, and whether an independent oracle was involved. These details let another reader reproduce a finite assertion without mistaking it for a universal result.

## Recipe: preserve an application result across rejected requests

Applications sometimes need to keep the last accepted value while attempting a new calculation. Use a separate initialized temporary of the intended destination precision. Run the new calculation into that temporary, inspect the sentinel, numerical classification, and application-specific domain requirements, and copy into the persistent result only after acceptance.

This pattern complements the library's allocation rollback guarantees. It also covers numerical rejections and domain outcomes for which retaining the old value is an application preference rather than an API guarantee. Keep an explicit validity or freshness marker: an unchanged value can be a valid old result while the current request remains unresolved.

For a service or long-running calculation, pair that marker with a concise diagnostic record: the operation, exact or original input description, requested precision and mode, flags, and reason for rejection. Avoid returning a plausible old number without saying that the new calculation failed. A useful numerical interface communicates both the value it has and the question that value actually answers.
