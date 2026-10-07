# Polyphase Float 0.10.9 allocation-failure validation

## Scope

This release adds deterministic fault injection at the context allocator
boundary and validates transactional object behavior for a bounded set of core
operations. The numerical formulas are unchanged. The work concerns allocation
ownership, error signaling, destination preservation, recovery, and release
safety.

## Test allocator

`tests/test_allocation_failure.c` provides an allocator that can fail exactly
one selected allocation or reallocation call. It records:

- allocation sequence number
- live block count
- live byte count
- allocation, reallocation, and free counts
- pointer ownership
- recorded allocation size
- double-free and size-mismatch errors

Each operation is first run without failure to determine the number of context
allocation sites reached. The test then repeats the operation once for every
fail position.

## Object snapshots

Before a covered operation begins, the destination is snapshotted. After an
unrecovered allocation failure, the test compares the complete object state,
including its significand bytes and allocation pointer. Aliased tests use the
same snapshot rule when the destination is also an input.

## Covered cases

The deterministic suite covers:

1. allocation-flag convenience and generic APIs
2. object initialization failure
3. reserve and precision-change reallocation failure
4. scratch-arena first-block and growth failure
5. reusable divisor-plan construction
6. reusable divisor-plan application and exact fallback
7. exact sum, including aliased destination
8. exact dot product, including aliased destination
9. multiplication and squaring with in-place variants
10. division with an in-place destination
11. integer roots
12. reciprocal square root, including later scratch allocations
13. PPFIF payload import
14. successful reuse after each injected failure
15. final zero-live-allocation checks

## Acceptance rules

A fail-position case is accepted only when all applicable conditions hold:

- the allocation flag is raised
- no range flag is substituted for a genuine allocation failure
- the public return convention reports failure, or an explicitly permitted
  exact fallback succeeds
- an initialized destination is unchanged after an unrecovered failure
- plan outputs remain null when construction fails
- arena state remains releasable
- no custom-allocation block remains live after cleanup
- the same object or context can perform a later successful operation

## Validation matrix

The final source was built and tested under strict warnings in the GCC OpenMP
and GCC serial configurations:

```text
OpenMP enabled: 54 / 54 CTest entries passed
OpenMP disabled: 54 / 54 CTest entries passed
```

The test-label totals are:

```text
fast:       33
hardening:   1
extended:    7
smoke:      13
```

A complete Clang serial validation tree also passed 54 of 54 configured tests.
That compiler exposes 425 shared-library exports on this host because its C mode
does not enable the four conditional decimal conversion entry points. The
allocation suite separately passed in a Clang serial build with
AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection enabled.

## Interpretation

This evidence establishes the 0.10.9 contract for the context-owned paths
listed above. It does not establish universal behavior for GMP-internal
allocation or every remaining process-allocation call in the transcendental,
special-function, formatting, constant, and worker-metadata modules.
