# Polyphase Float 0.19.0 deep audit

## Outcome

The fresh audit found and repaired concrete defects in the canonical 0.18.0
source. Version 0.19.0 is the resulting pre-1.0 release. It preserves the public
ABI and independent GMP-based production implementation. The exact fresh
execution results are recorded in evidence/DEEP_AUDIT_RUNS_0.19.0.json; the
source inventory is hash-bound in PRODUCTION_CHANGESET_0.19.0.json.

The scope was numerical correctness, interval certificates, directed rounding,
special values, recoverable allocation failure, ownership, parser boundaries,
threading, conditional compiler features, installed consumers, and release
claims. The source was recovered from polyphase-float-0.18.0_GITHUB_READY_CANONICAL.zip.
A separate pristine extraction was retained for comparisons and negative controls.

## Confirmed repairs

| Area | Defect and resulting behavior |
| --- | --- |
| Directed sign changes | ppf_neg and ppf_abs now reverse upward/downward rounding when reversing a sign before rounding into lower precision. The error sentinel is never negated. |
| Precision change | ppf_prec_round stages precision and value together, validates rounding first, preserves the original object on allocation failure, and reports the explicit context error. |
| Scalar remainder | ppf_fmod_ui now preserves its destination and propagates allocation flags from its working context. |
| Interval assignment | ppf_set_dyadic_interval propagates endpoint errors and commits without replacing caller-owned external storage. An undecided interval publishes no result or ternary. |
| Random generation | Five random endpoints commit numerical outputs only after success. Partial scratch initialization is cleaned up. A normal pair commits both outputs together. |
| Relative difference | ppf_reldiff preserves ordinary, aliased, and external outputs on project allocation failure. |
| Integer edge cases | INT_MIN custom-kind and dynamic-format-width paths reject invalid arguments without signed overflow. Extreme Bessel orders do not overflow their magnitude calculation. |
| Parsing | Empty and whitespace-only text are rejected. Invalid rounding is checked before special values. Huge-exponent range classification includes significand magnitude and produces the correct directed tiny result. |
| Predicates | ppf_number_p excludes both NaN and infinity, matching its finite-number meaning. |
| Gamma and Beta identities | Exact bounded integer cases and log-Gamma zero identities avoid unsuccessful precision escalation. Exceptional signs, pole flags, and infinities follow the independently checked oracle cases. |
| Special intervals | Positive tail majorants widen only the upper endpoint. Airy rational Gamma inputs respect decreasing Gamma. Bessel denominator conversion is directed; even powers of negative intervals reverse the endpoint selection. |
| Transformed arguments | Gamma/digamma reflection and Li2 reductions retain interval uncertainty. Point-transform paths that require exact shifts explicitly test exactness before certifying. |
| Installed consumers | Nested and multiarch pkg-config paths relocate correctly. CMake core consumers can omit MPFR discovery. Adapter components require the proper header and package. |
| Conditional exports | Release audits use the built library's binary128/decimal capabilities, so a valid Clang configuration does not fail for disabled decimal endpoints. |
| Evidence ledger | Setup calls no longer inherit transactional claims from text references. Variadic initialization retains its actual prefix-cleanup contract. Metadata controls no longer inherit numerical-domain limits. |

## Validation and its interpretation

| Fresh configuration | Result |
| --- | --- |
| GCC 13.3, serial, strict warnings | 64/64 CTest checks passed |
| Clang 18.1, serial, strict warnings | 64/64 CTest checks passed |
| GCC OpenMP, four worker setting | 64/64 CTest checks passed |
| GCC ASan and UBSan hardening | 22/22 checks passed, leak scanner disabled |
| GCC ThreadSanitizer | Thread-safety and cache-lifecycle test passed |
| Text parser and PPFIF fuzzing | 50,000 cases each, fixed seeds and fresh original corpus |
| Make regression targets | All five added targets passed |
| GCC and Clang source/dependency/export audits | Passed, no production MPFR imports |
| Unchanged 0.18 binary consumer with 0.19 library | Identical checked output and preserved SONAME |

The final run inventory records strict GCC and Clang C11 builds, serial and
OpenMP configurations, the configured CTest suites, installed static/core/MPFR
consumer execution, epoch-1 ABI checks, production dependency audit, sanitizer
checks, deterministic allocation-failure sweeps, and bounded parser/interchange
fuzzing. The new special edge matrix contains 1,460 MPFR value, ternary, and
flag comparisons plus eight deliberate Bessel resource-limit checks. Independent
internal checks cover 20 rational Gamma enclosures and three positive-tail
invariants. A separate review adds 1,240 MPFR comparisons across Gamma,
digamma, Li2, and Ei with directed modes and high-precision perturbations.

The random failure campaign visits 414 project allocation positions, plus
rounding-mode, large-staging, optional-output, alias, and external-storage cases.
Strict custom allocator accounting requires zero live blocks and bytes and
checks the size supplied to each free. Restoring the five original API source
files makes all seven narrow negative controls fail. Numerical negative
controls reproduce Gamma(1), log-Gamma sign, negative-order Bessel zero, and
Airy subinterval defects in the original code. New regression tests are part
of both CMake and Make rather than existing only in an audit transcript.

GMP-managed storage and random-state consumption are outside rollback. Project
allocator balance checks do not establish recoverable GMP allocation failure.
AddressSanitizer and UndefinedBehaviorSanitizer results are recorded separately
from LeakSanitizer. LeakSanitizer cannot complete in this container because
process introspection is unavailable. ThreadSanitizer availability is recorded
as an actual run result, not inherited from old reports. The short fuzz runs
are bounded regression evidence and do not replace sustained fuzzing. The full sanitizer
allocation campaign took 641.79 seconds. Existing precision retries can perform
another numerical evaluation after a transient allocation error before the
outer transaction rejects the result. This is a remaining efficiency issue,
not evidence that the new rounding guards failed. No performance claim is made.

## Execution issues and corrections

The initial package mirror could not supply development tools. The toolchain
was provisioned through a working package source, and builds use official GMP
and MPFR headers. Early ledger checks correctly rejected stale hashes after
new evidence documents were added. The generated records and header hashes
were regenerated until the current contract checks agreed.

A reused Clang build initially loaded an older shared object through a stale
SONAME symlink. The clean GCC and Clang build directories both execute 0.19.0
and pass their complete 64-test inventories. The old-directory failures are
not counted as current-release passes. LeakSanitizer remains unavailable
because its process introspection fails in this environment; that failure is
reported rather than converted into a pass.

## Completion boundary

The corrected ledger has 190 endpoints with transactional evidence, 79 entries
with incomplete allocation evidence, 104 bounded numerical endpoints, and six
conditional interfaces. There are 253 entries without a modeled gap. The
reduced inherited transactional count is an evidence correction, not a claim
that previously working operations regressed.

The library is complete as an auditable development release on its stated
baseline. A universal claim of correct rounding for all representable inputs
is not established. Numerical enclosures, exact final rounding, failure
behavior, and performance are separate evidence obligations. Remaining
proof-to-source review includes general special-function reflection and
transformed-argument paths even after conservative exactness guards.

Explicit limits include parsed exponent resource caps, ordinary trig reduction
extra-bit caps, Airy finite |x|<=500, and finite Bessel order magnitude<=100000.
Special-value cases can follow different paths. Iteration and precision retry
caps can reject difficult inputs and are documented resource policies.
COMPLETION_SCOPE_0.19.0.md distinguishes these cases and states the 1.0 gates.
No new universal theorem or general performance improvement is claimed.

## Reproduction

Use the official GMP and MPFR development headers on Linux or WSL. MPFR is
needed for oracle tests, not the independent production core.

```sh
cmake -S . -B build/validation -DCMAKE_BUILD_TYPE=Release \
  -DPPF_MPFR_SUPPORT=ON -DPPF_OPENMP=OFF -DPPF_WARNINGS_AS_ERRORS=ON
cmake --build build/validation -j
ctest --test-dir build/validation --output-on-failure
python3 tools/release_audit.py --library build/validation/libpolyphase_float.so
python3 tools/create_source_release.py --output-dir dist
```

Add PPF_OPENMP=ON for the supported parallel configuration. Select Clang through
CMAKE_C_COMPILER for the separate compiler check. BUILDING.md and TESTING.md
state sanitizer, fuzz, component, and installed-consumer commands. Raw run logs
and a manifest are retained in evidence/. The generated regression corpora are
retained in fuzz/corpus/audit-0.19.0/. The unchanged-binary replay in this audit
uses the pristine 0.18.0 source; the normalized older interface is checked
separately against the frozen 0.11.0 contract and layout. Historical 0.18 reports are preserved
as historical records and do not substitute for these fresh runs.
