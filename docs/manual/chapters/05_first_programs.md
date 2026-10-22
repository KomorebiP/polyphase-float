# First complete programs

The four programs in this chapter turn the preceding concepts into code you can compile and inspect. Each is a complete C11 program with deterministic output. Each checks its own expected numerical relationships and returns a nonzero process status if setup, calculation, verification, or output fails. Their shared structure is intentional: learning one cleanup and error-checking pattern gives you a foundation for a larger application.

The complete source files are in [`examples/manual`](../../../examples/manual/). The build chapter gives a direct compiler command for the serial core. With examples and tests enabled, CMake also provides a target and a test for each program:

| Source file | Build target | Main lesson |
|---|---|---|
| `intro_first.c` | `example_manual_intro_first` | A complete checked calculation |
| `intro_rounding.c` | `example_manual_intro_rounding` | Directed and nearest rounding |
| `intro_flags.c` | `example_manual_intro_flags` | Sticky flags and special values |
| `intro_precision.c` | `example_manual_intro_precision` | Widening versus recomputing |

For example, from the source directory after configuring `build/release`:

```sh
cmake --build build/release --target example_manual_intro_first -j2
./build/release/example_manual_intro_first
ctest --test-dir build/release -L manual --output-on-failure
```

The `manual` label selects the manual examples registered in that build. The [example results](../EXAMPLE_RESULTS.md) describe their verification scope. These small demonstrations check their stated behavior; they are not a substitute for the full library test suite or validation of an application's numerical algorithm.

## Program 1: multiply two exact integers

Complete source: [`intro_first.c`](../../../examples/manual/intro_first.c).

The program computes two times three in a 128-bit destination. Small integers make the arithmetic exact, so any discrepancy points to setup, usage, or execution rather than a subtle decimal approximation.

```c
#include "polyphase_float.h"
#include <stdio.h>

int main(void) {
    ppf_context_t ctx;
    ppf_t a = {0}, b = {0}, product = {0};
    int status = 1;
    int ternary;

    if (ppf_context_init(&ctx) != 0) {
        fputs("Cannot initialize the context.\n", stderr);
        return 1;
    }
    if (ppf_init2_ctx(&a, 128, &ctx) != 0 ||
        ppf_init2_ctx(&b, 128, &ctx) != 0 ||
        ppf_init2_ctx(&product, 128, &ctx) != 0)
        goto cleanup;
    if (ppf_set_ui(&a, 2, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR ||
        ppf_set_ui(&b, 3, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
        goto cleanup;

    ternary = ppf_mul(&product, &a, &b, PPF_RNDN, &ctx);
    if (ternary == PPF_TERNARY_ERROR) goto cleanup;
    if (ternary != 0 || ppf_cmp_ui(&product, 6, &ctx) != 0)
        goto cleanup;
    if (fputs("2 * 3 = ", stdout) == EOF ||
        ppf_out_str(stdout, 10, 6, &product, PPF_RNDN, &ctx) == 0 ||
        fputc('\n', stdout) == EOF || fflush(stdout) == EOF)
        goto cleanup;
    status = 0;

cleanup:
    if (status != 0) fputs("The calculation or output failed.\n", stderr);
    ppf_clear(&product);
    ppf_clear(&b);
    ppf_clear(&a);
    ppf_context_clear(&ctx);
    ppf_thread_cleanup();
    return status;
}
```

Expected output:

```text
2 * 3 = 6.00000e0
```

The `ui` in `ppf_set_ui` denotes an unsigned C integer input. Both input assignments are exact at this precision. Multiplication returns zero because six is exactly representable. The program then checks the numerical answer using `ppf_cmp_ui`, rather than trusting that a successful call necessarily matches its intended example.

`ppf_out_str(stdout, 10, 6, ...)` prints in base ten with six significant digits. The `e0` exponent means multiplication by ten to the zeroth power. The output precision of six digits is a display choice; the destination still has 128 binary bits of precision.

The example checks both library output and the surrounding C stream operations. A calculation can succeed while writing its answer fails. `fflush` makes a pending buffered output failure visible before the program reports success.

For a C beginner, the `||` operators in setup are short-circuit tests: once one condition reports failure, later initializations in that expression are skipped and execution goes to cleanup. Because all object structures started at `{0}`, clearing the remaining structures is safe. `status` stays one until every required step succeeds.

## Program 2: see where one third lands

Complete source: [`intro_rounding.c`](../../../examples/manual/intro_rounding.c).

This program stores one and three exactly, then divides them into three eight-bit destinations. The central calls are:

```c
down = ppf_div(&lower, &one, &three, PPF_RNDD, &ctx);
near = ppf_div(&nearest, &one, &three, PPF_RNDN, &ctx);
up = ppf_div(&upper, &one, &three, PPF_RNDU, &ctx);
```

The complete program checks every failure sentinel before inspecting the signs. It also verifies that the lower result is smaller than the upper result and that the nearest result equals the upper result in this particular example.

Expected output:

```text
down    = 3.32031250e-1 (below)
nearest = 3.33984375e-1 (above)
up      = 3.33984375e-1 (above)
```

At eight-bit precision, the adjacent representable values around one third are

\[
L=\frac{170}{512}=0.33203125,
\qquad
U=\frac{171}{512}=0.333984375.
\]

Their distances from one third are \(1/768\) and \(1/1536\), respectively. The upper neighbor is closer, so nearest rounding chooses it. This is not a halfway case. The directed results bracket the exact quotient of the stored inputs, which are exactly one and three.

The program prints nine decimal significant digits so these particular binary values can be read clearly. It does not require nine accurate decimal digits in the quotient: eight binary bits have much coarser resolution. Here the extra printed digits expose the exact small dyadic values being stored.

Directed rounding is a tool for constructing bounds, but a complete interval algorithm must account for every operation and the uncertainty in its inputs. Running the last step once downward and once upward does not automatically enclose a mathematical expression whose earlier steps were rounded without bounds.

**Try it.** Increase all five object precisions from eight to sixteen. The printed endpoints should get closer, but the existing assertions about which endpoint is nearest are specific to the original example and must be reconsidered. When exploring, compare numerical relationships and ternary directions; do not simply change expected output until a test passes.

## Program 3: distinguish flags from return values

Complete source: [`intro_flags.c`](../../../examples/manual/intro_flags.c).

This program performs a sequence whose diagnostic state is easy to predict. It begins with eight-bit one third, performs an exact assignment, clears flags, and then evaluates two exceptional divisions.

Expected output:

```text
1 / 3: INEXACT is set
exact assignment: INEXACT is still set
after clearing: no flags are set
1 / 0: positive infinity; DIVBY0 is set
0 / 0: NaN; NAN is set
```

The exact assignment is two. Its zero ternary does not erase the earlier `INEXACT` flag. The program explicitly checks both facts: assignment was exact, and the previous flag remains. It then calls `ppf_context_flags_clear(&ctx, PPF_FLAGS_ALL)` to start a fresh observation period.

For the exceptional divisions, the code checks the failure sentinel, saves the flags, and inspects the result category. It requires positive infinity after one divided by positive zero, and NaN after zero divided by zero. The output text describes those verified conditions; it does not claim that every other possible flag bit is absent.

This is a useful pattern for an application boundary. Decide which categories and flags are acceptable for the task, then test them explicitly. A calculation intended to produce an ordinary finite measurement may reject both infinities and NaNs. A diagnostic tool can display them and their causes. Both policies can use the same library operations.

**Try it.** Remove the flag-clear call immediately before the final zero-over-zero division. The previous `DIVBY0` condition will remain in the context together with the newly reported NaN condition. That demonstrates why a flag set is a history since the last clear, not an automatic description of only the most recent call. Restore the clear afterward to retain the original example's intended observation periods.

## Program 4: widening does not restore lost input

Complete source: [`intro_precision.c`](../../../examples/manual/intro_precision.c).

This program compares three objects: a decimal string parsed at eight bits, an exact copy of that stored approximation into 128 bits, and the original string parsed directly at 128 bits.

```c
if (ppf_set_str(&coarse, "0.1", 10, PPF_RNDN, &ctx) != 0 ||
    ppf_set_str(&fresh, "0.1", 10, PPF_RNDN, &ctx) != 0)
    goto cleanup;
ternary = ppf_set(&widened, &coarse, PPF_RNDN, &ctx);
```

Notice the two return conventions. `ppf_set_str` returns a status, so nonzero is failure. `ppf_set` returns a ternary, so the complete source checks `PPF_TERNARY_ERROR` first and then verifies that this widening copy is exact.

Expected output:

```text
parsed at 8 bits     = 1.00097656250e-1
copied to 128 bits   = 1.00097656250e-1
parsed at 128 bits   = 1.00000000000e-1
changing the default leaves existing precisions unchanged
```

The first two objects are numerically equal. The wider copy has room for more bits, but the copy operation receives only the value already stored in `coarse`. The direct 128-bit parse receives the original decimal input and therefore produces a different, much closer approximation to one tenth. Its displayed twelve digits do not imply that binary one tenth has become exact.

The program also sets the explicit context's default precision to 256 and verifies that existing objects retain their eight-bit and 128-bit precisions. Policy changes do not silently rewrite previously created numbers.

For decimal data, parsing a decimal string directly avoids an intermediate conversion through a C `double`. In contrast, `ppf_set_d(&x, 0.1, ...)` receives the binary value of the C literal after the compiler has represented it as a `double`. A wider destination can preserve that input faithfully but cannot recover the decimal value the literal was intended to suggest. Neither route invents information absent from the input; choose the route that represents your actual data source.

## Adapt the pattern to an application

Before replacing these small calculations with a larger formula, write down the precision of each input, temporary, and final destination. Decide how input is obtained, which rounding modes are required, and which exceptional categories the caller accepts. Place flag-clearing boundaries where they correspond to meaningful units of work.

Keep the numerical operation separate from presentation. A useful calculation helper accepts initialized objects and a context, performs the work, and reports a checked result. The caller decides how many digits to print and where to send them. That separation makes it easier to test numerical relationships without depending on a particular output format.

Finally, keep a meaningful check. Exact small cases, symmetries, independently computed bounds, and comparisons at higher working precision can all be useful, provided you understand what they establish. Repeating one algorithm at more bits is a diagnostic technique, not automatically a proof of the final digits. The later numerical chapters explain the stronger tools available for specific operation families.

**Exercise.** Why is the widened eight-bit approximation exactly copied even though it is still an inaccurate representation of decimal `0.1`?

**Answer.** The copy's exact input is the stored eight-bit binary value. That dyadic value fits into 128 bits without rounding. Its discrepancy from the original decimal real number arose during the earlier parse, and the copy has no access to that lost information.
