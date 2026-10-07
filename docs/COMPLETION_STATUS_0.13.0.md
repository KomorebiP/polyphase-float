# Polyphase Float 0.13.0 completion status

## Conservative estimate

The project is approximately **61 percent ready for version 1.0**, with about
five percentage points of uncertainty.

The functional numerical surface is farther along than this percentage. The
estimate weights production obligations, adversarial correctness, platform
coverage, proof review, and independent replication heavily.

## Major block completed in 0.13.0

Version 0.13.0 establishes a generated, reviewable semantic ledger for every
core function declared in `include/polyphase_float.h`.

The release records, for all **437** functions:

- exact declaration and conditional availability;
- curated purpose and function family;
- return-value category;
- rounding and context policy;
- destination, source, and auxiliary output roles;
- conservative flag behavior;
- aliasing classification;
- thread-safety and ownership classification;
- allocation-failure atomicity status;
- known implementation bounds;
- direct test and documentation references;
- explicit unresolved gap categories.

Every public function has a direct reference in a test source. This is a
surface-coverage result, not a claim that every branch, rounding mode, alias
combination, resource failure, or practical limit has been exhaustively
validated.

## Current semantic gap ledger

A function can appear in more than one category.

| Gap category | Functions |
|---|---:|
| Aliasing not individually verified | **192** |
| Allocation-failure atomicity incomplete | **232** |
| Bounded implementation path | **112** |
| Conditional build surface | **6** |

The ledger therefore improves completion by making the remaining work exact
and machine-auditable. It does not treat unverified behavior as complete.

## Major blocks completed through 0.13.0

- complete lowercase reference inventory mapping;
- public open-source release and reproducible packaging;
- strict official-header and compiler validation;
- retained-sample and counterbalanced benchmark evidence;
- automated performance-regression gates;
- evidence-gated machine product profiles;
- beginner and complete function documentation;
- optional OpenMP and verified serial packages;
- parser and PPFIF fuzz harnesses;
- context-allocation failure injection and transactional state preservation;
- thread ownership, thread-exit cleanup, cache lifecycle, and ThreadSanitizer;
- ABI epoch 1 source contract, runtime layout record, ELF symbol governance,
  and old-binary compatibility validation;
- generated all-function semantic contract, direct surface test, and explicit
  gap ledger.

## Highest-weight remaining work

1. systematic legal and illegal alias matrices for the 192 unresolved entries;
2. deterministic allocation-failure coverage for the 232 unresolved entries;
3. hard-to-round, narrow-range, boundary, and special-value matrices by
   function family;
4. proof or normative documentation for every fixed iteration, attempt, and
   domain limit;
5. sustained fuzzing and permanent corpus governance;
6. native Windows, macOS, ARM64, and additional data-model validation;
7. cross-machine independent correctness and performance replication;
8. completion and review of the principal mathematical certification proofs;
9. performance refinement for logarithms, reciprocal square root, inverse
   angles, hyperbolic functions, and special-function families;
10. final 1.0 SONAME and support-window decision.

## ABI status

ABI epoch 1 remains governed on the supported Linux ELF LP64/GMP64 baseline.
Version 0.13.0 adds only source-level semantic-contract constants and does not
add a core shared-library symbol or change a governed public structure.
