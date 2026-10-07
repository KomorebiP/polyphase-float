# MPFR precision-type compatibility fix for 0.10.3

## Trigger

The strict differential build reached `tests/test_divisor_plan.c` and stopped
under `-Werror=sign-compare`. The official MPFR header defines `mpfr_prec_t` as
a signed integer type. The repository's test-only minimal MPFR declaration
layer had incorrectly modeled it as `unsigned long`, which concealed the
mixed-signedness comparison in local fallback-header builds.

## Correction

The fallback declaration layer now derives `mpfr_prec_t` and `mpfr_exp_t` from
GMP's signed `mp_size_t` and `mp_exp_t` types. This matches MPFR's default ABI
selection on both int-sized and long-sized GMP configurations.

The divisor-plan differential test now converts MPFR precisions through a
small checked helper. The helper requires a positive precision, converts it to
`ppf_prec_t`, and verifies that converting back reproduces the original MPFR
value before the precision is used by Polyphase APIs.

`test_mpfr_type_model` adds compile-time assertions that MPFR precision and
exponent types are signed. It is compiled in both official-header and fallback-
header configurations and runs in the fast CMake and Make test suites.

## Scope

The change is limited to tests, test-only compatibility declarations, release
auditing, metadata, and runner diagnostics. No file under `src/` changed.
