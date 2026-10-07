# Status

## Version

`0.18.0` public pre-1.0 development release.

## Surface

```text
349 / 349 direct or conditional counterparts in the supplied lowercase inventory
437 exported core ppf_* functions in validated GCC shared-library builds
5 optional MPFR-adapter ppf_* functions when the adapter is enabled
0 imported mpfr_* symbols in the audited production core shared library
```

## Semantic-contract status

- all 437 core functions are indexed in `semantics/contract-v1.json`
- all 437 have direct test-source references in the generated ledger
- 246 functions currently have no recorded semantic gap
- 191 functions retain one or more explicit gaps
- all 437 functions have an explicit aliasing classification, with zero unverified aliasing entries
- 81 functions still require individual allocation-failure atomicity verification
- 213 public functions now have verified transactional allocation behavior
- all 56 governed arithmetic, fused/planned-operation, and root endpoints have direct fail-at-every-allocation evidence
- 112 functions expose a documented bounded implementation path or practical limit
- 6 functions are conditional on binary128 or decimal compiler and library support
- the contracts are generated, hash-bound in the public header, installed, and release-audited

The matrix is an evidence ledger. It is not a blanket claim that every semantic
branch is fully proved. A gap remains visible until a specific test or
normative guarantee closes it.

## ABI status

- ABI epoch 1 preserves the 0.11.0 public Linux ELF interface
- version 0.18.0 changes no public declaration, layout, enum value, symbol, or calling contract
- the SONAME remains `libpolyphase_float.so.0`
- 431 inherited functions are versioned `PPF_0.11`
- 6 ABI-introspection functions are versioned `PPF_0.12`
- the source contract has zero breaking differences from 0.11.0 and 0.17.0
- public LP64/GMP64 layouts match the frozen 0.11.0 record
- compatibility is not yet claimed for unvalidated platforms or data models

## Concurrency and resource status

- thread-default contexts receive automatic POSIX thread-exit cleanup
- global cache purge is tested concurrently with active computations
- completed divisor plans and read-only operands are tested across threads
- deterministic allocation-failure injection covers the context-owned core, governed public text and interchange paths, all public certified-constant routes, 79 transcendental or special-function endpoints, and 56 arithmetic, fused/planned-operation, or root endpoints
- covered unrecovered failures preserve initialized destinations transactionally
- `ppf_div_with_plan_many` publishes no numerical or ternary result unless its complete batch succeeds
- certified-constant cache growth commits only after complete construction
- constant and governed trigonometric worker scratch is allocated before OpenMP execution
- optional OpenMP and a genuine serial package path are supported

## Maturity boundary

The functional API-construction phase is complete on the supported baseline.
Remaining production work includes closing or explicitly delimiting the 81
allocation-atomicity gaps, auditing the 112 bounded implementation paths,
expanding difficult rounding and special-value matrices, sustained fuzzing,
cross-platform ABI records, independent proof review, and external
replication.

The conservative version 1.0 readiness estimate is approximately 81.0 percent,
with about 3.5 percentage points of uncertainty.
