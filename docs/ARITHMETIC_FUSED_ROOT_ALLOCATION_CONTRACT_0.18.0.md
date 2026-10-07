# Polyphase Float 0.18.0 arithmetic, fused-operation, and root allocation contract

## Scope

This contract closes the remaining project-allocation transaction gaps in the
three public families that form the core numerical execution surface:

- 33 arithmetic and scalar-variant operations
- 15 reusable-division, fused, scaling, factorial, and dot-product operations
- 8 root and hypotenuse operations

The 56 governed entry points are indexed individually in
`semantics/contract-v1.json`. They include ordinary serial operations,
worker-count and product-algorithm variants, the multi-output
`ppf_div_with_plan_many` interface, supported in-place calls, and destinations
backed by application-managed external significands.

The contract governs allocations made through Polyphase Float's project
allocator and private provenance-carrying work buffers. It does not intercept
GMP-internal allocation or claim recovery from invalid objects, process
termination, operating-system failure, or allocator callbacks that violate the
public allocator contract.

## Single-result transaction

Each governed single-result function evaluates into an independent staging
object with the destination precision. The caller's destination is committed
only after the implementation returns without a newly observed project
allocation failure.

For common precisions up to 16 GMP limbs, staging uses fixed storage inside the
wrapper's transaction record. This avoids adding a heap allocation to ordinary
arithmetic calls. Larger results retain allocator-backed staging so every
project allocation remains visible to deterministic failure injection.

An unrecovered project-allocation failure obeys the following rule:

1. the function returns `PPF_TERNARY_ERROR`
2. `PPF_FLAG_ALLOC` remains sticky on the resolved context
3. the initialized destination remains byte-for-byte unchanged
4. caller-owned and external significand pointers retain their identity
5. every transaction and project work buffer is released
6. a later successful call may commit even when the allocation flag was already set

Destination preservation covers the significand pointer, allocator provenance,
storage flags, capacity, used-limb count, precision, exponent, sign, value
category, and complete limb contents.

## Batch division transaction

`ppf_div_with_plan_many` has a batch-level commit boundary. For a valid batch,
all destinations and optional ternary outputs are staged before execution.
Nothing is published unless every element succeeds and every destination
passes fixed-size commit validation.

The existing aliasing contract is retained:

- `outputs[i] == numerators[i]` is supported
- cross-index output/input overlap is rejected
- duplicate output objects are rejected

On project-allocation failure or any failed element, the function returns `-1`,
sets the sticky allocation flag when applicable, and preserves every caller
output and every supplied ternary slot.

## Internal name boundary

The public ABI remains unchanged. Internal production definitions and calls for
the governed functions are remapped to private uncommitted names. The public
symbols are supplied by a dedicated transaction layer. Internal arithmetic can
therefore compose operations without recursively entering public wrappers,
while every external call crosses exactly one public commit boundary.

The private names are built with hidden visibility and are not public ABI
entries.

## Threaded execution

The contract applies with OpenMP enabled or disabled. The batch-division
implementation continues to clone a private context per worker. The
deterministic test allocator serializes its own bookkeeping so concurrent
callbacks can be audited without a race in the test harness.

The transaction detects project allocation failure through the resolved
context flag and the thread-local project-allocation failure epoch. A
preexisting allocation flag is preserved without being mistaken for a new
failure in a later operation.

## Numerical boundary

Version 0.18.0 changes the allocation and publication boundary. It does not
change the public declarations, numerical formulas, exact product identities,
rounding rules, exponent-range behavior, special-value rules, product-planner
selection policy, or established aliasing classifications.

The following remain outside this contract:

- allocations performed internally by GMP
- the 81 public entries still marked allocation-atomicity incomplete
- the 112 entries with bounded implementation paths
- conditional interfaces not enabled by the active compiler or data model
- platforms outside the currently validated Linux and WSL baseline
