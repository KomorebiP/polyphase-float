# Polyphase Float 0.16.0 certified-constant allocation validation

## Deterministic failure matrix

`tests/test_constant_allocation_failure.c` first records the number of
project-controlled allocations made by a successful route, then repeats that
route with allocation positions forced to fail one at a time.

The matrix covers:

- `ppf_const_pi`, `ppf_const_pi_threads`, and `ppf_const_pi_uncached`
- `ppf_const_log2`, `ppf_const_log2_threads`, and `ppf_const_log2_uncached`
- `ppf_const_euler`, `ppf_const_euler_threads`, and `ppf_const_euler_uncached`
- `ppf_const_catalan`, `ppf_const_catalan_threads`, and `ppf_const_catalan_uncached`
- all four named precompute functions and `ppf_precompute_constant`
- `ppf_get_constant_interval`
- cache precision, cache clear, and global cache purge
- constant thread controls and last-statistics export

For every injected evaluation failure, the test checks destination identity,
limb bytes, cache state, allocation signaling, exact deallocation sizes, zero
leaked project blocks, and successful retry. Precompute sweeps check that a
failed build cannot install a partial cache and that a subsequent precompute
can complete normally.

## Candidate validation record

The exact 0.16.0 source candidate passed the local Linux validation recorded
below.

| Validation | Result |
|---|---:|
| Direct constant allocation-failure matrix | **Passed** |
| Clang AddressSanitizer, UndefinedBehaviorSanitizer, and leak checks | **Passed** |
| Embedded pi allocator-domain audit | **Passed** |
| Independent constants differential test | **Passed** |
| Semantic and aliasing contract verification | **Passed** |
| ABI comparison against 0.15.0 | **0 breaking changes** |
| Public core declarations represented | **437 / 437** |
| Constant-family entries with direct allocation evidence | **24 / 24** |

The bundled Windows PowerShell and WSL runner repeats the complete configured
GCC serial and OpenMP inventories, the Make suite, installation-consumer tests,
and a focused Clang AddressSanitizer, UndefinedBehaviorSanitizer, and leak
matrix on the target machine. The sanitizer matrix includes successful pi
evaluation, deterministic constant-allocation failure sweeps, and formatted
output ownership.

## Interpretation

The failure campaign governs project allocations observable through the custom
allocator. GMP internal allocation is not redirected by this test. A passing
sweep therefore establishes the stated project transaction boundary, not a
universal process out-of-memory theorem.

The numerical identities and enclosure formulas are unchanged. The public pi
builder selects the exact non-adic binary-splitting execution path because the
optional p-adic standalone paths still contain process-fatal allocation exits
and are outside this release contract.
