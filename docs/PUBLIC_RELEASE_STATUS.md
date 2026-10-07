# Public release status

## Current release

Polyphase Float 0.18.0 is a public pre-1.0 development release under
`LGPL-3.0-or-later`.

The validated public surface contains:

```text
349 / 349 direct or conditional counterparts in the supplied inventory
437 exported core ppf_* functions in validated GCC shared-library builds
5 optional MPFR-adapter ppf_* functions
0 imported mpfr_* symbols in the audited production core shared library
```

## Public semantic, aliasing, and allocation ledgers

Version 0.18.0 retains one semantic record and one aliasing record for each of
the 437 core functions. Every function has a curated purpose and direct
test-source reference. The alias contract contains zero unverified entries.

The arithmetic, fused-operation, and root allocation matrix closes 56
function-level allocation gaps. It governs ordinary and scalar arithmetic,
threaded and algorithm-selected products, reusable batch division, fused
operations, binary scaling, factorial, dot product, roots, and hypotenuse.
Supported in-place calls and caller-managed external significands remain
transactional. `ppf_div_with_plan_many` has a batch-level numerical and ternary
commit boundary.

The remaining explicit semantic gaps are 81 allocation-atomicity entries, 112
bounded implementation paths, and six conditional functions. A function may
appear in more than one category. The ledger records 213 functions with
verified transactional allocation behavior and 246 entries with no currently
recorded gap.

See `PUBLIC_SEMANTIC_CONTRACT_0.18.0.md`,
`SEMANTIC_CONTRACT_VALIDATION_0.18.0.md`,
`ALIASING_CONTRACT_VALIDATION_0.18.0.md`, and
`ARITHMETIC_FUSED_ROOT_ALLOCATION_CONTRACT_0.18.0.md`.

## ABI epoch 1

Version 0.18.0 preserves ABI epoch 1 and the 0.17.0 public interface. It adds no
public symbol, declaration, type, layout, enum value, or calling-contract
change. The SONAME remains `libpolyphase_float.so.0`. The normalized ABI
comparison records only the expected semantic and aliasing evidence-hash
updates.

The governed compatibility baseline remains Linux ELF on LP64/GMP64. Native
Windows, macOS, ARM64, big-endian, and 32-bit compatibility remain outside the
current ABI claim.

## Completed public-project foundations

- open-source license, contribution, security, governance, and provenance policy
- shared and static CMake and Make builds
- relocatable CMake package and `pkg-config` metadata
- optional MPFR adapter and differential validation
- strict tests, sanitizers, fuzz harnesses, and deterministic releases
- retained-sample and counterbalanced benchmark evidence
- evidence-gated product-planner profiles
- optional OpenMP with a verified serial build
- allocation-failure signaling and transactional hardening
- thread ownership, thread-exit cleanup, and cache-lifecycle validation
- ABI source contract, layout baseline, symbol versions, and old-binary testing
- generated all-function semantic contract and explicit gap ledger
- systematic aliasing contract with zero unverified public functions
- transactional public text, formatting, stream, and PPFIF allocation paths
- transactional certified-constant destination and cache updates
- transactional allocation behavior for 79 transcendental and special endpoints
- transactional allocation behavior for 56 arithmetic, fused/planned-operation, and root endpoints

## Deliberately not claimed complete

- control over allocation failures internal to GMP, libc, the kernel, or external stream backends
- allocation atomicity for the 81 functions still marked incomplete
- removal or proof of all 112 bounded implementation paths
- native Windows, macOS, ARM64, big-endian, or 32-bit ABI validation
- sustained multi-day fuzzing
- independently reproduced correctness or performance
- branch-complete proof-to-source traceability
- universal performance superiority over MPFR
- production maturity

The official MPFR development header remains part of target-machine validation
because optional declarations depend on the installed compiler and MPFR build.
