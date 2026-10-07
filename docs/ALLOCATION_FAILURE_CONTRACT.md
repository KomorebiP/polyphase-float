# Allocation failure contract

## Purpose

Polyphase Float permits an application to supply allocation callbacks through
`ppf_context_init_custom`. Version 0.10.9 defined the first explicit failure
contract for the context-owned allocation paths used by the object model,
scratch arena, core arithmetic, reusable divisor plans, roots, and PPFIF
import.

This contract is intentionally narrower than a universal out-of-memory claim.
Some modules still use GMP-managed allocation or process allocation directly.
Those remaining paths are listed under **Current boundary**.

## Allocation flag

`PPF_FLAG_ALLOC` is a sticky context exception flag. It is raised when a request
to the allocator configured in a `ppf_context_t` returns null.

The convenience functions are:

```c
ppf_clear_allocflag();
ppf_set_allocflag();
int raised = ppf_allocflag_p();
```

The generic flag functions also accept `PPF_FLAG_ALLOC`:

```c
ppf_flags_clear(PPF_FLAG_ALLOC);
ppf_flags_set(PPF_FLAG_ALLOC);
ppf_flags_test(PPF_FLAG_ALLOC);
```

The flag is diagnostic. It does not replace the ordinary function return value.
An operation may recover through an exact fallback and still leave the
allocation flag set so the caller can observe that the preferred path could
not allocate its scratch storage.

## Transactional destination rule

For the context-owned operations covered by the deterministic failure tests,
an unrecovered allocation failure obeys the following rule:

> An already initialized destination remains byte-for-byte unchanged.

The comparison includes:

- value category
- sign
- exponent
- precision
- limb capacity and used-limb count
- limb contents
- allocation pointer
- ownership and storage flags

This rule applies to the covered out-of-place and aliased arithmetic, exact
sum, dot product, multiplication, squaring, division, roots, reciprocal square
root, and PPFIF import paths.

A function can still change context flags. The transaction guarantee concerns
the destination object, not the context's sticky diagnostic state.

## Initialization failure

`ppf_init2_ctx` now accepts a mutable context because it may raise
`PPF_FLAG_ALLOC`.

When object initialization cannot allocate its significand:

1. the function returns failure
2. the allocation flag is raised on the resolved context
3. the destination structure is left in an all-zero state
4. the structure may be passed safely to `ppf_clear`

This is a pre-1.0 source compatibility correction. Applications must rebuild
against 0.10.9.

## Precision and reserve operations

`ppf_reserve` and `ppf_set_prec` are transactional with respect to the object.
A failed reallocation leaves the original allocation and object state intact.
Their existing public signatures do not accept a context, so these two
functions cannot raise a caller-selected context's allocation flag.

## Scratch arena

The scratch arena uses context-owned linked blocks. If a new block cannot be
allocated:

- the allocation flag is raised
- the requested allocation returns null
- the previous arena head, current block, and active mark remain valid
- blocks acquired before the failure remain owned by the context

Releasing to a prior mark after a failed growth request is supported.

## Reusable divisor plans

When plan construction fails:

- the output plan pointer remains null
- every completed suballocation is released
- the context allocation flag is raised
- no partially initialized plan is exposed

When plan application cannot allocate optional scratch storage, the
implementation may use its exact fallback. A successful fallback returns the
normal numerical result while preserving `PPF_FLAG_ALLOC` as a diagnostic.

## PPFIF import

The PPFIF importer allocates its payload through the resolved context. A failed
payload allocation leaves an initialized destination unchanged and raises the
allocation flag.

Version 0.15.0 added transactional public text, formatting, stream, and PPFIF export staging. See `IO_ALLOCATION_CONTRACT_0.15.0.md` for the stream-specific commit boundary.

## Custom allocator requirements

A custom allocator must provide all three callbacks:

```c
malloc_fn
realloc_fn
free_fn
```

The allocator's opaque state must outlive every object and context that retains
a copy of those callbacks. `realloc_fn` must preserve the original block when
it reports failure. `free_fn` receives the size that Polyphase Float associated
with the allocation.

The fail-at-N test allocator used by the release suite also tracks every live
block and byte count. This detects leaks, double frees, size mismatches, and
incorrect ownership transfer in the covered paths.

## Current boundary

Version 0.16.0 added the certified-constant family to the deterministic project
allocation contract. Version 0.17.0 added 79 logarithmic, exponential, power,
trigonometric, hyperbolic, and special-function endpoints. Version 0.18.0 adds
56 arithmetic, fused/planned-operation, and root endpoints, including a
batch-level transaction for `ppf_div_with_plan_many`. See
`CONSTANT_ALLOCATION_CONTRACT_0.16.0.md`,
`TRANSCENDENTAL_SPECIAL_ALLOCATION_CONTRACT_0.17.0.md`, and
`ARITHMETIC_FUSED_ROOT_ALLOCATION_CONTRACT_0.18.0.md`.

Polyphase Float still does not claim that every allocation made anywhere in the
library is controlled by `ppf_context_t`. Remaining examples include:

- GMP internal allocations
- the 81 public functions still marked allocation-incomplete by the semantic ledger
- optional standalone p-adic pi-engine paths
- failures in external systems outside the project allocator domain

Those paths remain part of the broader all-function allocation-hardening
roadmap.

## Caller pattern

```c
ppf_context_t ctx;
ppf_t x, y, out;

ppf_context_init(&ctx);
ppf_init2_ctx(&x, 256, &ctx);
ppf_init2_ctx(&y, 256, &ctx);
ppf_init2_ctx(&out, 256, &ctx);

ppf_flags_clear(PPF_FLAG_ALLOC);
int ternary = ppf_mul(&out, &x, &y, PPF_RNDN, &ctx);

if (ternary == PPF_TERNARY_ERROR &&
    (ppf_flags_test(PPF_FLAG_ALLOC) & PPF_FLAG_ALLOC) != 0) {
    /* out is unchanged for the covered failure path */
}

ppf_clear(&out);
ppf_clear(&y);
ppf_clear(&x);
ppf_context_clear(&ctx);
```
