# Claims and evidence policy

Polyphase Float separates claims by evidence level.

## Level 1: implemented

A symbol exists in the public header and shared-library export surface. The
349-of-349 inventory count establishes direct or conditional counterparts for
the supplied lowercase reference inventory. It does not establish full
semantic equivalence, equal performance, or a stable ABI.

## Level 2: tested

The implementation agrees with exact checks, invariants, or an external oracle
for the domains covered by the supplied tests. Test reports must identify the
input classes, precision, rounding modes, resource conditions, and platform.

Version 0.10.9 adds deterministic allocation-failure tests for a documented
subset of context-owned operations. That evidence must not be generalized to
GMP-internal allocation or every remaining direct process allocation.

Version 0.11.0 adds a documented POSIX-thread ownership contract and a focused
ThreadSanitizer/cache-lifecycle suite. That evidence supports the patterns in
`THREAD_SAFETY.md`; it does not make shared mutable contexts, objects, or random
states safe without external synchronization.

## Level 3: mathematically certified

A written argument establishes an enclosure, tail bound, exact transformation,
or rounding decision. The implementation must be traceable to that argument.
A label such as “certified” is not justified solely by a test pass.

## Level 4: independently reproduced

A person or system outside the original development environment rebuilds the
specified source and reproduces the claim from retained commands and raw data.

## Level 5: production mature

The project has a stable public contract, broad platform coverage, sustained
fuzzing and sanitizer evidence, comprehensive failure injection, external
review, and a maintained release process. The current project is not yet at
this level.

## Current defensible statements

- The source contains direct or conditional `ppf_*` counterparts for all 349
  symbols in the supplied lowercase inventory.
- The 0.13.0 public header and validated GCC core shared-library configurations
  contain 437 `ppf_*` functions.
- The 0.13.0 generated semantic ledger contains one record and curated purpose
  for each of those 437 functions, and every name has a direct test-source
  reference. The same ledger retains unresolved aliasing, allocation, limit,
  and conditional-build gaps explicitly.
- The production core is designed not to link MPFR, and the supplied audit
  checks imported symbols.
- The repository contains exact, differential, randomized, range,
  special-value, aliasing, large-precision, allocation-failure, sanitizer, and
  bounded-fuzzing tests.
- Context allocation failures in the covered 0.10.9 operation set raise
  `PPF_FLAG_ALLOC` and obey the documented transactional rules.
- The supported 0.11.0 thread-ownership and global-cache cleanup patterns pass
  the focused race, leak, and lifecycle suite.
- The governed 0.12.0 Linux ELF ABI has zero breaking source-contract changes
  from 0.11.0, matching LP64/GMP64 layouts, required symbol versions, and an
  unchanged old-binary execution test.
- Included benchmark data shows operation-specific wins and losses on recorded
  hosts.

## Statements not supported by the current evidence alone

- every function is correctly rounded for every possible input
- every allocation in the library is controlled by the context allocator
- the library is a drop-in binary replacement for MPFR
- the ABI is stable on every operating system, architecture, or data model
- the project is universally faster than MPFR
- a multiworker result is an equal-resource comparison with a one-thread result
- machine-specific benchmarks are independently replicated
- literal API coverage or a direct test reference is equivalent to
  exhaustive semantic verification or production maturity

## Benchmark claim template

A public performance statement should name:

1. operation and implementation path
2. precision and operand class
3. rounding mode
4. worker count and affinity policy
5. cold or warm cache condition
6. timed boundary
7. compiler and dependency builds
8. in-process and independent-process counts, estimator, and uncertainty method
9. whether every timing sample was retained
10. correctness gate
11. source, executable, environment, and run-configuration fingerprints
12. raw-data and analysis-manifest locations
