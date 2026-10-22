# Rounding, flags, and exceptional results

A floating-point calculation produces a stored value and information about how that value was obtained. Polyphase Float exposes that information through rounding modes, ternary return values, result categories, and context flags. These mechanisms answer different questions. Using them together makes it possible to distinguish an ordinary rounded answer from an invalid request, an exceptional mathematical result, or a practical implementation limit.

For a first program, use nearest rounding, check the failure sentinel, and inspect any result categories your application cannot accept. Add flag handling according to the calculation's needs. Inexact rounding is expected for many useful calculations; treating it as a program failure would reject values such as one third at every finite binary precision.

## Choose a rounding direction

Imagine the exact real result between two adjacent representable values at the destination precision. A rounding mode decides which value is stored.

| Mode | Meaning |
|---|---|
| `PPF_RNDN` | Nearest; an exact halfway case selects the even significand. |
| `PPF_RNDZ` | Toward zero. |
| `PPF_RNDU` | Toward positive infinity. |
| `PPF_RNDD` | Toward negative infinity. |
| `PPF_RNDA` | Away from zero. |
| `PPF_RNDF` | Faithful rounding: an allowed adjacent representable result. |
| `PPF_RND_DEFAULT` | For context-based numerical operations, use the resolved context's default rounding mode. |

The words *up* and *down* refer to numerical order, not magnitude. For a negative result, rounding toward positive infinity makes the answer less negative; rounding toward negative infinity makes it more negative. Rounding toward zero agrees with downward rounding for positive results and upward rounding for negative results.

For nearest-even, “even” refers to the least significant retained significand bit, not the last decimal digit in a printed string. Halfway decisions therefore depend on the binary representation at the destination precision.

Faithful rounding does not request a unique nearest result in every case. Do not use it when your algorithm requires a specifically directed bound or a particular nearest-even bit pattern. The function's own contract describes its supported settlement and reporting behavior. For introductory calculations, `PPF_RNDN`, `PPF_RNDD`, and `PPF_RNDU` make the intent particularly easy to inspect.

Passing a concrete mode on a call makes that choice independent of later changes to the context default. Passing `PPF_RND_DEFAULT` is useful when a whole operation sequence deliberately follows a policy selected by the caller.

Check the individual contract before passing `PPF_RND_DEFAULT` to other interfaces. Rounding-policy setters and `ppf_can_round` require concrete modes. The `ppf_fits_*_p` predicates use a fresh private context, so their default is nearest-even rather than the calling thread's policy. The naming helpers inspect the enumeration: `ppf_rounding_name` returns the name `"PPF_RND_DEFAULT"`, while `ppf_print_rnd_mode` returns `NULL` for that value.

## Check the failure sentinel before the sign

Many assignment and arithmetic functions return a **ternary** integer. For an ordinary finite rounding result:

| Return relation | Interpretation |
|---|---|
| Negative | The stored result is below the exact result. |
| Zero | The stored result equals the exact result. |
| Positive | The stored result is above the exact result. |

There is also a separate sentinel, `PPF_TERNARY_ERROR`, for an unrecovered failure. That sentinel is a negative integer. Testing `t < 0` before excluding it would misclassify failure as a valid result rounded downward.

Use this order:

```c
int t = ppf_div(&result, &numerator, &denominator,
                PPF_RNDN, &ctx);
if (t == PPF_TERNARY_ERROR) {
    /* Handle the failure; do not interpret t as a direction. */
} else if (t < 0) {
    /* The ordinary finite result is below its exact target. */
} else if (t > 0) {
    /* The ordinary finite result is above its exact target. */
} else {
    /* No inexact direction is reported; also check result category. */
}
```

Use the sign of a non-error ternary, not an assumption that the library will always return exactly `-1` or `+1`. Also do not replace the error check with `if (t != 0)`: that would classify successful inexact results as failures.

The exact target is the mathematical operation on the values actually stored in the operands. If a decimal input was rounded earlier, a later zero ternary says nothing about whether that input was the intended decimal real number. This distinction is especially important when an operation subtracts nearly equal approximations: the subtraction can be exact while the result has poor accuracy relative to the original problem.

Special values require their documented category rules rather than an ordinary finite ordering interpretation. For example, division by zero can produce infinity, and an invalid arithmetic combination can produce NaN. A zero return alone is not a general certificate that the result is a finite exact real value.

## Different functions return different kinds of answers

The C type of a return value is not enough to interpret it correctly.

| Example | Return convention |
|---|---|
| `ppf_init2_ctx`, `ppf_context_init` | Status: zero on success, failure otherwise. |
| `ppf_set`, `ppf_set_ui`, `ppf_mul`, `ppf_div` | Ternary, with `PPF_TERNARY_ERROR` checked first. |
| `ppf_set_str` | Status: zero for an accepted complete input, `-1` on failure. |
| `ppf_strtofr` | Ternary plus an end pointer used to inspect parsing progress. |
| `ppf_out_str` | Output count; zero indicates failure. |
| `ppf_cmp` | Comparison result, subject to its special-value rules. |
| `ppf_nan_p`, `ppf_zero_p` | Predicates: nonzero means the condition holds. |

In particular, a successful `ppf_set_str` call can involve rounding even though its return is zero. That API reports acceptance of the string, not a ternary direction. If you need parsing progress or rounding direction, use the appropriate parsing interface and validate its end pointer as described in the input/output chapter.

Do not build a helper that treats all functions with return type `int` identically. A useful application helper can be specific: one helper for status-returning setup, another for ternary arithmetic, and a separate policy for exceptional numerical categories.

## Observe sticky flags

A context accumulates diagnostic flags. “Sticky” means that a later successful or exact operation does not automatically erase a flag set by an earlier operation. This allows a program to examine whether any step in a sequence encountered a condition.

| Flag | General meaning |
|---|---|
| `PPF_FLAG_INEXACT` | A result required inexact rounding. |
| `PPF_FLAG_UNDERFLOW` | An operation encountered its underflow condition. |
| `PPF_FLAG_OVERFLOW` | An operation encountered its overflow condition. |
| `PPF_FLAG_DIVBY0` | A division-by-zero condition was reported. |
| `PPF_FLAG_NAN` | A NaN-related or invalid arithmetic condition was reported. |
| `PPF_FLAG_ERANGE` | An invalid, out-of-range, or otherwise rejected request was reported. |
| `PPF_FLAG_ALLOC` | A configured project allocator rejected an allocation request. |

These descriptions introduce the flags; individual operation contracts specify their exact triggering rules. For example, exponent-range operations and subnormalization have their own defined underflow semantics. Do not infer every function's behavior from a hardware floating-point exception model.

To isolate one operation, clear the flags you intend to monitor before it, then save the resulting flags immediately afterward:

```c
ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL);
int t = ppf_div(&result, &one, &three, PPF_RNDN, &ctx);
ppf_flags_t observed = ppf_context_flags(&ctx);
```

Check `t` before using the numerical result. The saved `observed` value describes the context state at that point. Subsequent conversions or other numerical work may alter the live flags, so saving them before formatting diagnostics avoids accidentally attributing later events to the division.

If an application wants flags for an entire algorithm, clear them once at the algorithm boundary and inspect them at the end. Clearing after every call would lose that accumulated record. If you need both an outer accumulated record and a local observation, use the save/restore interfaces with a deliberate mask and combine the observations according to your application's policy.

An allocation flag does not always mean the calculation ultimately failed. Some operations can recover through an exact fallback after an optional allocation fails, leaving the flag set as a diagnostic. Check the operation's return and result as well as the flag. The allocation contract covers project-controlled paths; GMP, libc, and operating-system allocation behavior are separate domains.

## Recognize special values

Polyphase Float distinguishes regular finite nonzero values, signed zeros, signed infinities, and NaNs. Use predicates such as `ppf_regular_p`, `ppf_zero_p`, `ppf_inf_p`, and `ppf_nan_p` instead of inspecting internal fields or relying on text formatting.

A numerical zero is finite, but it is not a *regular* value in this category terminology. Thus `ppf_regular_p` alone is not a test accepting every finite number. If your application accepts ordinary finite values including zero, accept regular values or zeros and reject the other categories.

Positive and negative zero compare as equal numerical zeros, but their signs can matter for sign-sensitive operations. `ppf_signbit` lets you inspect the retained sign. NaN is not an ordinary ordered number: comparisons involving it need the documented predicate or ordering behavior. Classify a value before using it in a policy that assumes an ordinary finite order.

The introductory flag program demonstrates two different exceptional operations. Dividing positive one by positive zero produces positive infinity and sets `DIVBY0`. Dividing zero by zero produces NaN and sets `NAN`. Neither case should be silently treated as an ordinary finite answer, even if the API follows its special-value rule without returning the failure sentinel.

An application decides whether infinity or NaN is acceptable. A scientific routine may propagate them with diagnostics; a file-format validator may reject them at input; an optimization algorithm may treat them as a failed candidate. Make that policy explicit at the boundary where the value enters the application.

## Understand exponent range separately from precision

Precision determines significand resolution. The context's `emin` and `emax` limit the exponent range used by operations. Increasing precision does not necessarily fix an overflow caused by a restrictive exponent range, and widening the exponent range does not restore precision lost through rounding.

Regular values are normalized; the object model does not store an implicit ladder of subnormal values beneath the minimum normal value. Ordinary range settlement below that boundary chooses signed zero or the minimum normal value according to the applicable rounding rule. Overflow chooses infinity or the largest finite value according to sign and rounding. Advanced range and subnormalization APIs expose additional operations with specific contracts.

For most first programs, retain the default range. If your application changes it, do so deliberately and check the status returned by `ppf_context_set_range`. Existing objects are not automatically recomputed just because policy changes. A subsequent assignment or operation can settle a result under the new range.

## Separate recoverable diagnostics from failed computation

A successful rounded result may be entirely suitable for an application. An unrecovered failure needs an explicit path: return an application error, select a documented alternative, or retry with a justified change of parameters. Do not print the destination as though it were newly valid merely because it still contains something.

Destination preservation depends on the kind of failure and the function's contract. Covered project-allocation failures preserve an initialized destination. Exhaustion of a mathematical function's numerical refinement budget can instead publish NaN with `ERANGE` and `PPF_TERNARY_ERROR`. Invalid input and resource-limit rejection also have operation-specific rules. Consult the relevant reference entry before relying on an old value after a failed call.

Increasing precision is not a universal remedy for rejection: a request can exceed a domain bound, an iteration budget, or a resource policy. Capture the arguments, precision, rounding mode, relevant flags, and result category when diagnosing such a case. That record is more informative than a decimal answer alone.

**Exercise.** An operation returns a negative integer. What must you test before describing the answer as rounded downward?

**Answer.** Test whether it equals `PPF_TERNARY_ERROR`. If it does, it is failure. If it does not, interpret the ternary sign under the operation's ordinary-result contract and inspect any special category relevant to the application.

**Exercise.** An inexact division is followed by an exact assignment, and `INEXACT` remains set. Did the assignment fail?

**Answer.** No such conclusion follows. The flag is sticky. The division may have set it, and the assignment does not clear it. To diagnose the assignment alone, clear the monitored flags before the assignment or use an explicit save/restore policy.
