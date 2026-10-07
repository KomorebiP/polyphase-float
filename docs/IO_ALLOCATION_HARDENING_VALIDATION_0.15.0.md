# Polyphase Float 0.15.0 I/O allocation-hardening validation

## Scope

Version 0.15.0 governs project-controlled allocation and commit boundaries in
the public text, formatting, stream, and PPFIF interfaces. It does not claim to
inject or recover allocation failure occurring inside GMP, libc, the kernel, or
an arbitrary stream backend.

## Deterministic fail-at-N matrix

`tests/test_io_allocation_failure.c` discovers the project-allocation count of
each covered success path and repeats the operation with each allocation
position forced to fail.

The matrix directly exercises:

- `ppf_set_str` and `ppf_strtofr` temporary construction and commit;
- `ppf_get_str` allocator provenance and `ppf_free_str` ownership;
- `ppf_out_str` and `ppf_inp_str` stream boundaries;
- `ppf_fpif_export` pre-write staging and `ppf_fpif_import` transactional
  destination replacement;
- `ppf_asprintf`, `ppf_vasprintf`, `ppf_snprintf`, `ppf_vsnprintf`,
  `ppf_sprintf`, `ppf_vsprintf`, `ppf_fprintf`, `ppf_vfprintf`, `ppf_printf`,
  and `ppf_vprintf` through their shared staging implementation.

For each injected failure, the test checks the applicable subset of:

1. `PPF_FLAG_ALLOC` signaling;
2. byte-for-byte numerical destination preservation;
3. caller-buffer preservation;
4. absence of a returned partial string;
5. zero stream bytes before the commit boundary;
6. exact deallocation size and allocator provenance;
7. zero live project allocations after cleanup;
8. successful reuse on a following non-failing call.

## Parser distinction

The test confirms that `ppf_set_str` rejects a valid numeric prefix followed by
non-space trailing text without modifying the destination. `ppf_strtofr`
retains prefix parsing and reports the first unconsumed character through
`endptr`, but commits only after the temporary value has been completed.

## Completed validation

The exact 0.15.0 candidate produced the following results in the local release
environment.

| Validation | Result |
|---|---:|
| Strict GCC serial configured inventory | **68 / 68 passed** |
| Strict GCC OpenMP configured inventory | **68 / 68 passed** |
| Strict Clang serial configured inventory | **55 / 55 passed** |
| GCC hardening label | **14 / 14 passed** |
| GCC extended label | **7 / 7 passed** |
| GCC benchmark-smoke label | **13 / 13 passed** |
| Clang ASan, UBSan, and leak-focused tests | **3 / 3 passed** |
| Complete Make differential suite | **Passed** |
| Make semantic and aliasing audits | **Passed** |
| Make ABI audit | **Passed** |
| CMake installed downstream consumer | **Passed** |
| Make staged installation | **Passed** |
| Text-parser libFuzzer campaign | **100,000 executions** |
| PPFIF-import libFuzzer campaign | **100,000 executions** |
| Fuzzer crashes or sanitizer findings | **0** |
| Public core declarations and exports | **437 / 437** |
| Optional adapter exports | **5 / 5** |
| Production MPFR imports | **0** |
| Public release audit | **Passed** |

The local environment contained the MPFR runtime library but not the official
`mpfr.h` development header. MPFR-dependent local builds therefore used the
repository compatibility declaration against the installed runtime. The target
runner installs and requires the official development header.

## Stream limitation

The tests establish that project-controlled allocation failure occurs before
any output-stream write. They do not claim rollback after writing begins, or
restoration of an input-stream position after token consumption.

## Remaining boundary

The unresolved allocation program remains concentrated in GMP-internal
allocation, certified-constant packet trees, transcendental and special-
function work arrays, selected worker metadata, and cache growth.
