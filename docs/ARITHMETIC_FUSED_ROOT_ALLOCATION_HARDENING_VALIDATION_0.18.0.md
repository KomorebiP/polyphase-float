# Polyphase Float 0.18.0 arithmetic, fused-operation, and root allocation validation

## Deterministic endpoint matrix

`tests/test_arithmetic_root_allocation_failure.c` directly invokes all 56
governed public entry points by exact name. For every function, the test first
records the complete successful project-allocation depth at 1,088-bit
precision. It then reconstructs a fresh fixture and injects failure at every
observed allocation position from one through the successful depth.

Every injected failure verifies:

- the required error result, `PPF_TERNARY_ERROR` or `-1` for batch division
- a sticky `PPF_FLAG_ALLOC`
- exact preservation of all numerical destination metadata and limb bytes
- preservation of all batch ternary outputs
- no live-allocation or live-byte delta
- no unknown free, size mismatch, double free, or allocator bookkeeping error

The matrix is executed in both serial and OpenMP builds. Its custom allocator
uses a C11 atomic lock so concurrent worker callbacks receive a deterministic
allocation index and race-free live-block accounting.

## Additional transaction cases

The same test also covers:

- in-place `ppf_mul` failure with stack-backed staging
- successful recovery while a prior allocation flag remains sticky
- failure and success with application-managed external significand storage
- preservation of the external pointer across commit
- successful same-index overlap in `ppf_div_with_plan_many`
- failed same-index batch execution with all numerical and ternary outputs unchanged

## Governed surface

| Family | Endpoints |
|---|---:|
| Arithmetic and scalar variants | 33 |
| Reusable division, fused operations, scaling, factorial, and dot product | 15 |
| Roots and hypotenuse | 8 |
| **Total** | **56** |

## Candidate validation record

| Validation | Result |
|---|---:|
| GCC serial warnings-as-errors build | **Passed** |
| GCC OpenMP warnings-as-errors build | **Passed** |
| Serial complete allocation matrix | **56 / 56 passed** |
| OpenMP complete allocation matrix | **56 / 56 passed** |
| Clang AddressSanitizer, UndefinedBehaviorSanitizer, and leak check | **Passed** |
| GCC ThreadSanitizer execution | **Passed** |
| Generated semantic and aliasing policy checks | **Passed** |
| ABI comparison against 0.17.0 | **0 breaks, 0 additions** |
| Release audit with shared-library export inspection | **Passed** |
| Installed CMake consumer | **Passed** |
| Public symbol and declaration additions | **0** |
| Allocation-atomicity ledger reduction | **137 to 81** |
| Verified transactional public functions | **157 to 213** |

The bundled Windows PowerShell and WSL runner repeats the configured serial and
OpenMP test inventories, release and contract audits, installation-consumer
checks, focused sanitizer execution, archive reconstruction, and checksum
validation on the target machine.

## Interpretation

A passing matrix establishes the stated public commit boundary for allocations
visible through Polyphase Float's project allocator. It does not establish
control over GMP's process-global allocator, external stream backends, the
operating system, or every possible invalid-object misuse.
