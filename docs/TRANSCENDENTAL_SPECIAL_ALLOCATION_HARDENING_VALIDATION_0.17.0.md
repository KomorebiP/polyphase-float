# Polyphase Float 0.17.0 transcendental and special allocation validation

## Deterministic matrix

`tests/test_transcendental_special_allocation_failure.c` directly exercises all
79 governed public endpoints. For each endpoint it first records the successful
project-allocation depth, then injects failure at selected positions spanning
the beginning, interior, and end of that route.

Complete fail-at-every-project-allocation sweeps are applied to representative
high-depth and structurally distinct paths:

- `ppf_log`
- `ppf_exp`
- `ppf_pow`
- `ppf_sin`
- `ppf_sin_cos`
- `ppf_atan2`
- `ppf_sinh_cosh`
- `ppf_gamma`
- `ppf_lgamma`
- `ppf_zeta`
- `ppf_jn`
- `ppf_agm`

The successful baseline depths observed in the local 96-bit matrix range from
one project allocation for exact integer-power routes to 3,921 for `ppf_ai`.
The test uses the observed depth rather than a hard-coded allocation count.

## State checks

Every injected failure checks:

- the required error sentinel and sticky allocation flag
- exact destination identity and limb bytes
- paired-output atomicity
- preservation of the `ppf_lgamma` sign output
- supported in-place destination preservation
- external-significand pointer and storage-flag preservation
- exact allocator sizes, zero leaked project blocks, and zero double frees
- successful recovery after a prior sticky allocation failure

Additional 2,048-bit cases exercise the serial logarithm workspace and the
four-worker OpenMP trigonometric workspace.

## Local candidate record

| Validation | Result |
|---|---:|
| GCC serial shared and static build | **Passed** |
| GCC OpenMP shared and static build | **Passed** |
| Serial 79-endpoint allocation matrix | **Passed** |
| OpenMP 79-endpoint allocation matrix | **Passed** |
| ABI comparison against 0.16.0 | **0 breaking changes** |
| Public additions relative to 0.16.0 | **0** |
| Governed public endpoints | **79 / 79** |
| Allocation-atomicity ledger reduction | **216 to 137** |

The bundled Windows PowerShell and WSL runner repeats the complete configured
serial and OpenMP inventories, Make validation, contract and release audits,
installation-consumer checks, and a focused Clang AddressSanitizer,
UndefinedBehaviorSanitizer, and leak campaign on the target machine.

## Interpretation

The campaign governs allocations visible through the Polyphase Float custom
allocator and its private provenance buffers. GMP-internal allocation remains
in the GMP allocator domain. A passing matrix establishes the stated public
commit boundary for project-controlled failures. It is not a universal
out-of-memory proof for the process or operating system.
