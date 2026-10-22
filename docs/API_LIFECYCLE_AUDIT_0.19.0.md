# API lifecycle and parser audit for 0.19.0

This audit closes a bounded random-result allocation block and corrects
reproducible defects in the 0.18.0 implementations. The production library
continues to use GMP, without adding an MPFR dependency or changing the public
ABI layout or exported function inventory.

## Confirmed defects and changes

| Area | Original behavior | Corrected behavior |
|---|---|---|
| Exponential random scratch objects | A later initialization failure leaked previously initialized objects | Every initialized object is cleared |
| Normal random scratch objects | The initialization-failure label cleared the context but leaked previously initialized objects | The shared cleanup path clears the complete initialized prefix |
| Random numerical destinations | An allocation error could escape from a cloned context without a caller allocation flag or leave a modified result | Five random endpoints stage their numerical outputs, detect allocation failures through the existing transaction epoch, and preserve caller destinations on failure |
| Relative difference | A failed temporary allocation replaced the destination with NaN | The destination is committed only after project allocations succeed |
| Custom kind code | Negating `INT_MIN` invoked signed-integer undefined behavior | Out-of-range signed codes are rejected before taking a magnitude |
| Dynamic format width | Negating `INT_MIN` invoked signed-integer undefined behavior | The call returns a formatting error with the range flag and `EOVERFLOW` where available, before modifying the destination buffer |
| Finite-number predicate | `ppf_number_p` returned true for infinity, contradicting its public semantic purpose | It returns true for regular finite values and signed zero, and false for infinity and NaN |
| Empty text | `ppf_set_str` accepted empty and whitespace-only strings as zero | No-conversion input returns an error and preserves the previous value |
| Large text exponent | The underflow shortcut compared the unscaled significand with the midpoint, producing a wrong nearest result; range classification omitted significand magnitude | The classifier includes significand bit length and a conservative floating estimate guard, and proven tiny values use the correct directed zero/minimum result |

The partial-initialization cleanup in `ppf_can_round` was also corrected by
zero-initializing its four scratch objects and clearing the initialized prefix
on failure. This last change is supported by source inspection, rather than a
dedicated injected-failure execution, because that helper creates its own
default allocator context.

## Numerical allocation contract

The focused regression verifies project-controlled allocation atomicity for:

```text
ppf_urandomb
ppf_urandom
ppf_erandom
ppf_nrandom
ppf_grandom
ppf_reldiff
```

On an observed project allocation failure, every initialized numerical
destination retains its precision, value, limbs, allocator provenance, and
storage ownership. The resolved public context retains `PPF_FLAG_ALLOC`.
`ppf_urandomb` keeps its established status value of `1` for failure; the four
other random functions return `PPF_TERNARY_ERROR`. `ppf_reldiff` retains its void
return type and communicates allocation failure through the context flag.

The optional second output of `ppf_grandom` preserves its packed ternary
convention when omitted. Paired outputs commit together. A common output
object remains an invalid-overlap error, as documented in the existing aliasing
contract. Externally backed significands remain externally backed after both
success and allocation failure.

Random-state consumption is not rolled back. GMP-internal allocation remains
outside the project's allocator interception and failure contract. This work
does not establish complete allocation atomicity for all public functions.

## Executed regression

`tests/test_api_lifecycle_audit.c` uses a custom allocator that records the exact
old and new allocation sizes, injects one failure at a selected allocation
position, and checks for zero live blocks and bytes after every case.

For the successful 24-bit, nearest-rounding, seed `0x51a73` trace, it then
injects failure at every observed project allocation position:

| Endpoint | Observed positions exercised |
|---|---:|
| `ppf_urandomb` | 1 |
| `ppf_urandom` | 1 |
| `ppf_erandom` | 16 |
| `ppf_nrandom` | 197 |
| `ppf_grandom` | 199 |
| `ppf_reldiff` | 2, repeated with ordinary, aliased, and external outputs |

The random cases also cover the first failure under all other rounding modes,
1100-bit allocator-backed result staging, successful external storage, failed
external storage, pre-existing sticky allocation and division-by-zero flags,
and the omitted normal-pair output.

Parser regressions cover empty and whitespace-only input, invalid text
fragments, valid zero surrounded by spaces, and signs and all five deterministic
rounding modes for `1e2000001` and `1e-2000001`. The range cases compare values,
ternary directions, signed zero, and flags with exact dyadic range oracles.
Additional 256-bit significand cases test overflow classification and
cancellation near the declared text-power resource limit. The latter still
reports the resource bound rather than inventing a numerical underflow.

The complete focused test passed in a serial GCC build with AddressSanitizer
and UndefinedBehaviorSanitizer enabled. LeakSanitizer could not complete in the
audit container because its `/proc` task introspection was inaccessible.
Consequently this report claims no LeakSanitizer pass. The exact custom
allocator balance checks passed throughout.

## Negative controls

For negative controls, the five affected implementation files were restored
from the canonical 0.18.0 source archive into a separate static test library,
while keeping the remaining library code and the focused test unchanged. The
seven narrow modes all failed against those original implementations:

| Mode | Original-source failure |
|---|---|
| `kind` | UBSan reported negation of `INT_MIN` |
| `width` | UBSan reported negation of `INT_MIN` |
| `parser` | Empty text succeeded instead of returning an error |
| `finite` | Infinity passed the finite-number predicate |
| `range` | Large negative decimal exponent produced the wrong ternary direction |
| `random` | Exponential random failure left live allocator blocks |
| `reldiff` | Failed allocation changed the numerical destination |

These controls establish that the regression detects the original defects.
They do not replace the broader differential, threading, installation, ABI,
and release checks documented elsewhere.
