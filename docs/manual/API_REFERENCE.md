# Core API reference

This reference covers every core function declared in `<polyphase_float.h>`. The five optional MPFR adapter functions are described separately in the integration chapter. Entries are arranged by task, then by name. Use PDF search or the alphabetical index to find a symbol.

All ordinary arithmetic objects must be initialized and valid. Unless an operation explicitly changes precision, the destination's precision governs its result. Mutating an object or an explicit context requires exclusive access. No function transfers ownership unless its entry says so.

A recorded allocation contract describes recoverable project allocation, not every allocation made by GMP, libc, or the operating system. Numerical range rejection and certification exhaustion have their own output rules. In particular, an error sentinel alone is not a promise of destination preservation.

The numerical-limits appendix applies throughout this reference. Absence of a function-specific limit in an entry is not a claim of unlimited work, a termination proof, or universal correct rounding. The full source and individual numerical arguments remain the authority for implementation details.

## Contexts, allocators, flags, and scratch memory

### ppf_allocflag_p

Report whether the allocation-failure flag is set in the calling thread default context.

```c
int ppf_allocflag_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_arena_alloc

Allocate temporary aligned memory from a context scratch arena.

```c
void *ppf_arena_alloc(ppf_context_t *ctx, size_t bytes, size_t alignment);
```

**Result.** A pointer with the ownership conditions below. Failure indicator: `NULL where documented`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Ownership.** storage remains owned by the context until release, reset, or clear.

**Allocation and failure.** On unrecovered project-allocation failure, returns NULL without adding a new arena block or changing the existing arena allocation position; earlier arena allocations remain valid.

### ppf_arena_mark

Mark the current scratch-arena position.

```c
ppf_arena_mark_t ppf_arena_mark(const ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

### ppf_arena_release

Release scratch allocations made after a saved arena mark.

```c
void ppf_arena_release(ppf_context_t *ctx, ppf_arena_mark_t mark);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_arena_reset

Reset the context scratch arena for reuse.

```c
void ppf_arena_reset(ppf_context_t *ctx);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_clear_allocflag

Clear the allocation-failure flag in the calling thread default context.

```c
void ppf_clear_allocflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_context_clear

Release memory owned by an explicit context.

```c
void ppf_context_clear(ppf_context_t *ctx);
```

**Result.** No return value.

**Context and rounding.** The supplied context must be non-null and initialized.

### ppf_context_clone

Create a new context with the same policy settings as another context.

```c
int ppf_context_clone(ppf_context_t *dst, const ppf_context_t *src);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** The supplied context must be non-null and initialized.

### ppf_context_flags

Read all exception flags from a context.

```c
ppf_flags_t ppf_context_flags(const ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

### ppf_context_flags_clear

Clear selected exception flags in a context.

```c
void ppf_context_flags_clear(ppf_context_t *ctx, ppf_flags_t mask);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_context_flags_restore

Restore selected context flags from a saved value.

```c
void ppf_context_flags_restore(
    ppf_context_t *ctx,
    ppf_flags_t flags,
    ppf_flags_t mask
);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_context_flags_save

Copy the current context flags for later restoration.

```c
ppf_flags_t ppf_context_flags_save(const ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

### ppf_context_flags_set

Set selected exception flags in a context.

```c
void ppf_context_flags_set(ppf_context_t *ctx, ppf_flags_t mask);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_context_init

Initialize an explicit context with default precision, range, rounding, flags, allocator, and scratch arena.

```c
int ppf_context_init(ppf_context_t *ctx);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** The context destination must be non-null writable unused storage. This call initializes it; do not overwrite a live context.

### ppf_context_init_custom

Initialize a context that uses caller-supplied allocation callbacks.

```c
int ppf_context_init_custom(
    ppf_context_t *ctx,
    const ppf_allocator_t *allocator
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** The context destination must be non-null writable unused storage. This call initializes it; do not overwrite a live context.

### ppf_context_set_default_precision

Change the precision used for new default-precision objects in a context.

```c
int ppf_context_set_default_precision(
    ppf_context_t *ctx,
    ppf_prec_t precision
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_context_set_range

Set the minimum and maximum binary exponents for a context.

```c
int ppf_context_set_range(
    ppf_context_t *ctx,
    ppf_exp_t emin,
    ppf_exp_t emax
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_context_set_rounding

Set the context's default rounding mode.

```c
int ppf_context_set_rounding(ppf_context_t *ctx, ppf_rnd_t rnd);
```

**Result.** Returns 0 when the concrete rounding mode is accepted and -1 otherwise; `PPF_RND_DEFAULT` is rejected. Failure indicator: `-1`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. select a concrete mode from PPF_RNDN through PPF_RNDF; PPF_RND_DEFAULT is rejected rather than resolved.

### ppf_default_allocator

Return the library's standard allocator callbacks.

```c
ppf_allocator_t ppf_default_allocator(void);
```

**Result.** The value described above, with the declared C return type.

**Ownership.** returns allocator callbacks by value.

### ppf_default_context

Return the calling thread's default context.

```c
ppf_context_t *ppf_default_context(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** pointer remains owned by the calling thread and is invalid after thread cleanup.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_default_context_cleanup

Release the calling thread's default-context resources.

```c
void ppf_default_context_cleanup(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_allocflag

Set the allocation-failure flag in the calling thread default context.

```c
void ppf_set_allocflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_thread_cleanup

Release heap-backed state owned by the calling thread. Safe to call repeatedly.

```c
void ppf_thread_cleanup(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

## Exponent range and exception flags

### ppf_check_range

Apply the active exponent range to an already rounded result and update flags and ternary direction.

```c
int ppf_check_range(
    ppf_t *x,
    int ternary,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_clear_divby0

Clear the divide-by-zero flag in the thread-local default context.

```c
void ppf_clear_divby0(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_clear_erangeflag

Clear the range-error flag in the thread-local default context.

```c
void ppf_clear_erangeflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_clear_flags

Clear all exception flags in the thread-local default context.

```c
void ppf_clear_flags(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_clear_inexflag

Clear the inexact flag in the thread-local default context.

```c
void ppf_clear_inexflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_clear_nanflag

Clear the NaN flag in the thread-local default context.

```c
void ppf_clear_nanflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_clear_overflow

Clear the overflow flag in the thread-local default context.

```c
void ppf_clear_overflow(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_clear_underflow

Clear the underflow flag in the thread-local default context.

```c
void ppf_clear_underflow(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_divby0_p

Test the divide-by-zero flag in the thread-local default context.

```c
int ppf_divby0_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_erangeflag_p

Test the range-error flag in the thread-local default context.

```c
int ppf_erangeflag_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_flags_clear

Clear selected flags in the thread-local default context.

```c
void ppf_flags_clear(ppf_flags_t mask);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_flags_restore

Restore selected flags in the thread-local default context.

```c
void ppf_flags_restore(ppf_flags_t flags, ppf_flags_t mask);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_flags_save

Save all flags from the thread-local default context.

```c
ppf_flags_t ppf_flags_save(void);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_flags_set

Set selected flags in the thread-local default context.

```c
void ppf_flags_set(ppf_flags_t mask);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_flags_test

Return the selected flags currently set in the thread-local default context.

```c
ppf_flags_t ppf_flags_test(ppf_flags_t mask);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_get_emax

Read the maximum exponent of the thread-local default context.

```c
ppf_exp_t ppf_get_emax(void);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_get_emax_max

Return the largest supported maximum exponent setting.

```c
ppf_exp_t ppf_get_emax_max(void);
```

**Result.** The value described above, with the declared C return type.

### ppf_get_emax_min

Return the smallest supported maximum exponent setting.

```c
ppf_exp_t ppf_get_emax_min(void);
```

**Result.** The value described above, with the declared C return type.

### ppf_get_emin

Read the minimum exponent of the thread-local default context.

```c
ppf_exp_t ppf_get_emin(void);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_get_emin_max

Return the largest supported minimum exponent setting.

```c
ppf_exp_t ppf_get_emin_max(void);
```

**Result.** The value described above, with the declared C return type.

### ppf_get_emin_min

Return the smallest supported minimum exponent setting.

```c
ppf_exp_t ppf_get_emin_min(void);
```

**Result.** The value described above, with the declared C return type.

### ppf_inexflag_p

Test the inexact flag in the thread-local default context.

```c
int ppf_inexflag_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_nanflag_p

Test the NaN flag in the thread-local default context.

```c
int ppf_nanflag_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_overflow_p

Test the overflow flag in the thread-local default context.

```c
int ppf_overflow_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_divby0

Set the divide-by-zero flag in the thread-local default context.

```c
void ppf_set_divby0(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_emax

Set the default context maximum exponent.

```c
int ppf_set_emax(ppf_exp_t exponent);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_emin

Set the default context minimum exponent.

```c
int ppf_set_emin(ppf_exp_t exponent);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_erangeflag

Set the range-error flag in the thread-local default context.

```c
void ppf_set_erangeflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_inexflag

Set the inexact flag in the thread-local default context.

```c
void ppf_set_inexflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_nanflag

Set the NaN flag in the thread-local default context.

```c
void ppf_set_nanflag(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_overflow

Set the overflow flag in the thread-local default context.

```c
void ppf_set_overflow(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_underflow

Set the underflow flag in the thread-local default context.

```c
void ppf_set_underflow(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_subnormalize

Emulate subnormal or fixed-point rounding at the active minimum exponent.

```c
int ppf_subnormalize(
    ppf_t *x,
    int ternary,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_underflow_p

Test the underflow flag in the thread-local default context.

```c
int ppf_underflow_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

## Initialization and precision

### ppf_clear

Release memory owned by one number.

```c
void ppf_clear(ppf_t *x);
```

**Result.** No return value.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_clears

Clear several numbers; terminate the variadic list with `(ppf_t *)0`.

```c
void ppf_clears(ppf_t *first, ...);
```

**Result.** No return value.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_get_default_prec

Return the thread-local default precision.

```c
ppf_prec_t ppf_get_default_prec(void);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_get_prec

Return an object's precision in bits.

```c
ppf_prec_t ppf_get_prec(const ppf_t *x);
```

**Result.** The value described above, with the declared C return type.

### ppf_init

Initialize a number at the thread-local default precision.

```c
int ppf_init(ppf_t *x);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** On a valid initialization request, project-allocation failure leaves a safe zero-state object. Invalid arguments can be rejected before zeroing; follow the initializer's preconditions and cleanup lifecycle.

### ppf_init2

Initialize a number at an explicit precision.

```c
int ppf_init2(ppf_t *x, ppf_prec_t precision);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** On a valid initialization request, project-allocation failure leaves a safe zero-state object. Invalid arguments can be rejected before zeroing; follow the initializer's preconditions and cleanup lifecycle.

### ppf_init2_ctx

Initialize a number at an explicit precision using an explicit context allocator.

```c
int ppf_init2_ctx(ppf_t *x, ppf_prec_t precision, ppf_context_t *ctx);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** The supplied context must be non-null and initialized.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** On a valid initialization request, project-allocation failure leaves a safe zero-state object. Invalid arguments can be rejected before zeroing; follow the initializer's preconditions and cleanup lifecycle.

### ppf_inits

Initialize several numbers at the default precision; terminate with `(ppf_t *)0`.

```c
int ppf_inits(ppf_t *first, ...);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** On failure the initialized prefix is cleared and unvisited variadic destinations are untouched; every observed project allocation is tested; libc prefix-tracking realloc remains outside the recoverable project-allocator domain.

### ppf_inits2

Initialize several numbers at one precision; terminate the variadic list with `(ppf_t *)0`.

```c
int ppf_inits2(ppf_prec_t precision, ppf_t *first, ...);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** On failure the initialized prefix is cleared and unvisited variadic destinations are untouched; every observed project allocation is tested; libc prefix-tracking realloc remains outside the recoverable project-allocator domain.

### ppf_prec_round

Change precision while preserving and rounding the current value.

```c
int ppf_prec_round(
    ppf_t *x,
    ppf_prec_t precision,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_reserve

Ensure an object has storage for at least the requested precision.

```c
int ppf_reserve(ppf_t *x, ppf_prec_t precision);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_set_default_prec

Set the thread-local default precision used by `ppf_init`.

```c
int ppf_set_default_prec(ppf_prec_t precision);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_prec

Change object precision and leave a deterministic NaN rather than preserving the old value.

```c
int ppf_set_prec(ppf_t *x, ppf_prec_t precision);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_set_prec_raw

Change the exposed precision field for MPF-compatibility workflows; advanced use only.

```c
void ppf_set_prec_raw(ppf_t *x, ppf_prec_t precision);
```

**Result.** No return value.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_swap

Exchange two complete floating objects without numerical rounding.

```c
void ppf_swap(ppf_t *a, ppf_t *b);
```

**Result.** No return value.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

## Direct significand access (advanced)

### ppf_commit_regular

Validate and commit a directly written regular significand, sign, and exponent.

```c
int ppf_commit_regular(
    ppf_t *x,
    int sign,
    ppf_exp_t exponent,
    ppf_context_t *ctx
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_limb_count

Return the number of significand limbs currently used by an object.

```c
size_t ppf_limb_count(const ppf_t *x);
```

**Result.** The operation-specific count described above, rather than a numerical ternary. Failure indicator: `0 when the operation cannot produce a count`.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

### ppf_limbs_read

Return read-only access to normalized significand limbs.

```c
const mp_limb_t *ppf_limbs_read(const ppf_t *x);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** pointer is invalidated by mutation, reallocation, or clear.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

### ppf_limbs_write

Begin controlled writable access to significand limbs; the caller must later commit a valid regular value.

```c
mp_limb_t *ppf_limbs_write(ppf_t *x);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** pointer is writable internal storage and must be followed by a valid commit when representation fields change.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

## Special values, signs, and rounding control

### ppf_can_round

Ask whether an approximation with a stated error can be safely rounded to a target precision and mode.

```c
int ppf_can_round(
    const ppf_t *approximation,
    ppf_exp_t error_bits,
    ppf_rnd_t source_rounding,
    ppf_rnd_t target_rounding,
    ppf_prec_t target_precision
);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** source_rounding and target_rounding must both be concrete modes from PPF_RNDN through PPF_RNDF; PPF_RND_DEFAULT in either parameter is rejected with return 0, without consulting the calling thread default context.

### ppf_copysign

Copy the magnitude of one value and the sign of another.

```c
int ppf_copysign(
    ppf_t *rop,
    const ppf_t *op1,
    const ppf_t *op2,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_get_default_rounding_mode

Return the thread-local default rounding mode.

```c
ppf_rnd_t ppf_get_default_rounding_mode(void);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_inf_p

Test whether a value is infinite.

```c
int ppf_inf_p(const ppf_t *x);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_nan_p

Test whether a value is NaN.

```c
int ppf_nan_p(const ppf_t *x);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_number_p

Test whether a value is finite, including zero.

```c
int ppf_number_p(const ppf_t *x);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_print_rnd_mode

Return the short printable spelling of a rounding mode.

```c
const char *ppf_print_rnd_mode(ppf_rnd_t rnd);
```

**Result.** Returns a static name for a concrete rounding mode from `PPF_RNDN` through `PPF_RNDF`. `PPF_RND_DEFAULT` and invalid enumeration values return NULL. Failure indicator: `NULL`.

**Context and rounding.** the enumeration is inspected without resolving a context; PPF_RND_DEFAULT and invalid values return NULL.

**Ownership.** returned string is library-owned static storage.

### ppf_regular_p

Test whether a value is a finite nonzero regular number.

```c
int ppf_regular_p(const ppf_t *x);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_round_nearest_away_finish

Finish the extra-bit helper used to implement nearest rounding with ties away from zero.

```c
int ppf_round_nearest_away_finish(
    ppf_t *rop,
    const ppf_t *extended_result,
    int extended_ternary,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Overlap.** destination precision is exactly one bit below the extended source precision; Forbidden: destination and extended source are the same object.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_default_rounding_mode

Set the thread-local default rounding mode.

```c
int ppf_set_default_rounding_mode(ppf_rnd_t rnd);
```

**Result.** Returns 0 when the concrete rounding mode is accepted and -1 otherwise; `PPF_RND_DEFAULT` is rejected. Failure indicator: `-1`.

**Context and rounding.** select a concrete mode from PPF_RNDN through PPF_RNDF; PPF_RND_DEFAULT is rejected rather than resolved.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_set_inf

Set signed infinity.

```c
void ppf_set_inf(ppf_t *x, int sign);
```

**Result.** No return value.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_nan

Set a signed NaN.

```c
void ppf_set_nan(ppf_t *x, int sign);
```

**Result.** No return value.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_zero

Set signed zero.

```c
void ppf_set_zero(ppf_t *x, int sign);
```

**Result.** No return value.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_setsign

Copy a value while explicitly selecting its sign bit.

```c
int ppf_setsign(
    ppf_t *rop,
    const ppf_t *op,
    int signbit,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_sgn

Return the mathematical sign of a non-NaN value and update context state when needed.

```c
int ppf_sgn(const ppf_t *x, ppf_context_t *ctx);
```

**Result.** A sign result for the operation described above.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_signbit

Return the stored sign bit, including for zero, infinity, and NaN.

```c
int ppf_signbit(const ppf_t *x);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_zero_p

Test whether a value is zero.

```c
int ppf_zero_p(const ppf_t *x);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

## Assignment and parsing

### ppf_set

Copy and round one Polyphase number into another.

```c
int ppf_set(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_d

Round a double into an existing Polyphase number.

```c
int ppf_set_d(ppf_t *rop, double value, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_decimal128

Round a decimal128 value into an existing Polyphase number.

```c
int ppf_set_decimal128(
    ppf_t *rop,
    ppf_decimal128_t value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

**Availability.** This declaration depends on `PPF_HAVE_DECIMAL_FLOAT`; test the installed header's capability macro.

### ppf_set_decimal64

Round a decimal64 value into an existing Polyphase number.

```c
int ppf_set_decimal64(
    ppf_t *rop,
    ppf_decimal64_t value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

**Availability.** This declaration depends on `PPF_HAVE_DECIMAL_FLOAT`; test the installed header's capability macro.

### ppf_set_f

Round a GMP float (`mpf_t`) into an existing Polyphase number.

```c
int ppf_set_f(
    ppf_t *rop,
    mpf_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_float128

Round a binary128 value into an existing Polyphase number.

```c
int ppf_set_float128(
    ppf_t *rop,
    ppf_float128_t value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

**Availability.** This declaration depends on `PPF_HAVE_FLOAT128`; test the installed header's capability macro.

### ppf_set_flt

Round a float into an existing Polyphase number.

```c
int ppf_set_flt(ppf_t *rop, float value, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_ld

Round a long double into an existing Polyphase number.

```c
int ppf_set_ld(
    ppf_t *rop,
    long double value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_q

Round a GMP rational (`mpq_t`) into an existing Polyphase number.

```c
int ppf_set_q(
    ppf_t *rop,
    mpq_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_si

Round a signed long into an existing Polyphase number.

```c
int ppf_set_si(ppf_t *rop, long value, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_si_2exp

Assign the exact value (signed long) times a binary power, then round once.

```c
int ppf_set_si_2exp(
    ppf_t *rop,
    long value,
    ppf_exp_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_sj

Round an intmax_t into an existing Polyphase number.

```c
int ppf_set_sj(
    ppf_t *rop,
    intmax_t value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_sj_2exp

Assign the exact value (intmax_t) times a binary power, then round once.

```c
int ppf_set_sj_2exp(
    ppf_t *rop,
    intmax_t value,
    ppf_exp_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_str

Parse an entire string. Return `0` only when the complete non-whitespace input is consumed; return `-1` on parse failure or trailing text.

```c
int ppf_set_str(
    ppf_t *rop,
    const char *text,
    int base,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** absolute parsed exponent magnitudes above 2,000,000 are constructed only when the result is already unambiguously outside the configured binary range. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_set_ui

Round an unsigned long into an existing Polyphase number.

```c
int ppf_set_ui(
    ppf_t *rop,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_ui_2exp

Assign the exact value (unsigned long) times a binary power, then round once.

```c
int ppf_set_ui_2exp(
    ppf_t *rop,
    unsigned long value,
    ppf_exp_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_uj

Round an uintmax_t into an existing Polyphase number.

```c
int ppf_set_uj(
    ppf_t *rop,
    uintmax_t value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_uj_2exp

Assign the exact value (uintmax_t) times a binary power, then round once.

```c
int ppf_set_uj_2exp(
    ppf_t *rop,
    uintmax_t value,
    ppf_exp_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_z

Round a GMP integer (`mpz_t`) into an existing Polyphase number.

```c
int ppf_set_z(
    ppf_t *rop,
    mpz_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_z_2exp

Assign the exact value (GMP integer (`mpz_t`)) times a binary power, then round once.

```c
int ppf_set_z_2exp(
    ppf_t *rop,
    mpz_srcptr value,
    ppf_exp_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_strtofr

Parse as much of a string as possible, return the ternary rounding relation, and report the first unparsed character.

```c
int ppf_strtofr(
    ppf_t *rop,
    const char *text,
    char **endptr,
    int base,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** absolute parsed exponent magnitudes above 2,000,000 are constructed only when the result is already unambiguously outside the configured binary range. Consult the numerical-limits appendix for the complete cap inventory.

## Initialize-and-set shortcuts

### ppf_init_set

Initialize a number and copy-round another Polyphase number into it.

```c
int ppf_init_set(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination is uninitialized and distinct from the initialized source; Forbidden: destination and source are the same object.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_d

Initialize a number and assign from a double in one call.

```c
int ppf_init_set_d(
    ppf_t *rop,
    double value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_f

Initialize a number and assign from a GMP float (`mpf_t`) in one call.

```c
int ppf_init_set_f(
    ppf_t *rop,
    mpf_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_flt

Initialize a number and assign from a float in one call.

```c
int ppf_init_set_flt(
    ppf_t *rop,
    float value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_ld

Initialize a number and assign from a long double in one call.

```c
int ppf_init_set_ld(
    ppf_t *rop,
    long double value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_q

Initialize a number and assign from a GMP rational (`mpq_t`) in one call.

```c
int ppf_init_set_q(
    ppf_t *rop,
    mpq_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_si

Initialize a number and assign from a signed long in one call.

```c
int ppf_init_set_si(
    ppf_t *rop,
    long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_sj

Initialize a number and assign from an intmax_t in one call.

```c
int ppf_init_set_sj(
    ppf_t *rop,
    intmax_t value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_str

Initialize a number and assign from a str in one call.

```c
int ppf_init_set_str(
    ppf_t *rop,
    const char *text,
    int base,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

**Resource policy.** absolute parsed exponent magnitudes above 2,000,000 are constructed only when the result is already unambiguously outside the configured binary range. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_init_set_ui

Initialize a number and assign from an unsigned long in one call.

```c
int ppf_init_set_ui(
    ppf_t *rop,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_uj

Initialize a number and assign from an uintmax_t in one call.

```c
int ppf_init_set_uj(
    ppf_t *rop,
    uintmax_t value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

### ppf_init_set_z

Initialize a number and assign from a GMP integer (`mpz_t`) in one call.

```c
int ppf_init_set_z(
    ppf_t *rop,
    mpz_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** destination storage satisfies the function's initialization or destruction precondition; Forbidden: using an already initialized object as an initialization destination unless explicitly documented.

**Allocation and failure.** Every observed project-allocation failure is tested; initialization failure leaves a safe-zero object, setter failure leaves an initialized clearable object; caller must clear after any initialized outcome.

## Conversion to C and GMP types

### ppf_free_str

Release a string returned by `ppf_get_str` using the correct allocator.

```c
void ppf_free_str(char *text);
```

**Result.** No return value.

**Ownership.** releases a string allocated by ppf_get_str.

### ppf_frexp

Split a value into a Polyphase mantissa and binary exponent.

```c
int ppf_frexp(
    ppf_exp_t *exponent,
    ppf_t *mantissa,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_get_d

Convert a Polyphase value to double using the requested rounding mode.

```c
double ppf_get_d(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_d_2exp

Return a `double` mantissa and store a separate binary exponent.

```c
double ppf_get_d_2exp(
    long *exponent,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_decimal128

Convert a Polyphase value to decimal128 value using the requested rounding mode.

```c
ppf_decimal128_t ppf_get_decimal128(
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Availability.** This declaration depends on `PPF_HAVE_DECIMAL_FLOAT`; test the installed header's capability macro.

### ppf_get_decimal64

Convert a Polyphase value to decimal64 value using the requested rounding mode.

```c
ppf_decimal64_t ppf_get_decimal64(
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Availability.** This declaration depends on `PPF_HAVE_DECIMAL_FLOAT`; test the installed header's capability macro.

### ppf_get_f

Round a value into an initialized GMP float.

```c
int ppf_get_f(
    mpf_ptr value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** Returns the numerical ternary for conversion to the GMP floating-point destination. Test for `PPF_TERNARY_ERROR` before interpreting its sign. NaN maps to zero with ERANGE and return 0; infinity maps to a finite GMP value with ERANGE and returns +1 for negative infinity or -1 for positive infinity.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_float128

Convert a Polyphase value to binary128 value using the requested rounding mode.

```c
ppf_float128_t ppf_get_float128(
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Availability.** This declaration depends on `PPF_HAVE_FLOAT128`; test the installed header's capability macro.

### ppf_get_flt

Convert a Polyphase value to float using the requested rounding mode.

```c
float ppf_get_flt(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_ld

Convert a Polyphase value to long double using the requested rounding mode.

```c
long double ppf_get_ld(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_ld_2exp

Return a `long double` mantissa and store a separate binary exponent.

```c
long double ppf_get_ld_2exp(
    long *exponent,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_q

Export a finite value exactly as a GMP rational.

```c
void ppf_get_q(mpq_ptr value, const ppf_t *op, ppf_context_t *ctx);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_get_si

Convert a Polyphase value to signed long using the requested rounding mode.

```c
long ppf_get_si(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_sj

Convert a Polyphase value to intmax_t using the requested rounding mode.

```c
intmax_t ppf_get_sj(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_str

Format a significand into a supplied or library-allocated buffer and return its radix exponent separately.

```c
char *ppf_get_str(
    char *buffer,
    ppf_exp_t *exponent,
    int base,
    size_t digits,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A pointer with the ownership conditions below. Failure indicator: `NULL`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Ownership.** when the caller supplies no buffer, the returned string retains the resolved context allocator provenance and must be released only with ppf_free_str.

**Allocation and failure.** On unrecovered project-allocation failure, returns NULL and releases temporary project-owned storage; the source number is unchanged; recovered fallback allocation failures may still yield a valid string.

### ppf_get_str_ndigits

Return the number of base-`b` digits normally needed to print an object at its precision.

```c
size_t ppf_get_str_ndigits(int base, ppf_prec_t precision);
```

**Result.** The operation-specific count described above, rather than a numerical ternary. Failure indicator: `0 when the operation cannot produce a count`.

### ppf_get_ui

Convert a Polyphase value to unsigned long using the requested rounding mode.

```c
unsigned long ppf_get_ui(
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_uj

Convert a Polyphase value to uintmax_t using the requested rounding mode.

```c
uintmax_t ppf_get_uj(const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_z

Round a value to a GMP integer.

```c
int ppf_get_z(
    mpz_ptr value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** Returns the numerical ternary for the GMP integer written to rop. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

### ppf_get_z_2exp

Export the exact integer significand and binary exponent of a finite value.

```c
ppf_exp_t ppf_get_z_2exp(mpz_ptr value, const ppf_t *op);
```

**Result.** The value described above, with the declared C return type.

## Text I/O, PPFIF interchange, formatting, and fit tests

### ppf_asprintf

Allocate a formatted string with `malloc`; release the returned buffer with `free`.

```c
int ppf_asprintf(char **text, const char *format, ...);
```

**Result.** A formatting count; a negative result reports failure.

**Ownership.** on success *text is allocated with the process allocator and must be released with free.

**Allocation and failure.** Sets *text to NULL before formatting; failure releases temporary storage and leaves *text NULL, rather than preserving its prior pointer; %n writes already performed while formatting are not rolled back.

### ppf_fits_intmax_p

Test whether the value fits in intmax_t under the library fit rules.

```c
int ppf_fits_intmax_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fits_sint_p

Test whether the value fits in signed int under the library fit rules.

```c
int ppf_fits_sint_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fits_slong_p

Test whether the value fits in signed long under the library fit rules.

```c
int ppf_fits_slong_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fits_sshort_p

Test whether the value fits in signed short under the library fit rules.

```c
int ppf_fits_sshort_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fits_uint_p

Test whether the value fits in unsigned int under the library fit rules.

```c
int ppf_fits_uint_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fits_uintmax_p

Test whether the value fits in uintmax_t under the library fit rules.

```c
int ppf_fits_uintmax_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fits_ulong_p

Test whether the value fits in unsigned long under the library fit rules.

```c
int ppf_fits_ulong_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fits_ushort_p

Test whether the value fits in unsigned short under the library fit rules.

```c
int ppf_fits_ushort_p(const ppf_t *op, ppf_rnd_t rnd);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

**Context and rounding.** PPF_RND_DEFAULT resolves to PPF_RNDN through a newly initialized private context, independently of the calling thread default rounding mode.

### ppf_fpif_export

Write the endian-independent `PPFIF01` binary interchange representation.

```c
int ppf_fpif_export(FILE *stream, const ppf_t *op);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Allocation and failure.** The complete record is staged before its first stream write; project-allocation failure leaves the stream and source number unchanged, but a later stream write failure may leave a partial record.

### ppf_fpif_import

Transactionally import a `PPFIF01` record; malformed input leaves the destination unchanged.

```c
int ppf_fpif_import(ppf_t *op, FILE *stream, ppf_context_t *ctx);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Allocation and failure.** Unrecovered project-allocation failure preserves the numerical destination and releases temporary project-owned storage; bytes already consumed from the input stream are not restored.

**Resource policy.** default 64 MiB cap independently bounds imported payload and rounded significand storage (536870912 ordinary precision bits on GMP64); `PPF_FPIF_MAX_IMPORT_BYTES` may configure another positive cap; initial staging is at most 4096 bytes, growing only after reserved bytes arrive; range rejection returns -1, raises ERANGE and preserves the destination. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_fprintf

Formatted output to a stream with C, GMP, precision, and Polyphase conversions.

```c
int ppf_fprintf(FILE *stream, const char *format, ...);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before stream output; unrecovered project-allocation failure during formatting leaves the stream unchanged; %n writes already performed are not rolled back, and a later stream write failure may leave partial output.

### ppf_inp_str

Read one whitespace-delimited number from a stream; a null stream means standard input.

```c
size_t ppf_inp_str(
    ppf_t *rop,
    FILE *stream,
    int base,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The operation-specific count described above, rather than a numerical ternary. Failure indicator: `0 when the operation cannot produce a count`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Unrecovered project-allocation failure preserves the numerical destination and releases temporary project-owned storage; bytes already consumed from the input stream are not restored.

### ppf_out_str

Write a value in an arbitrary base to a stream; a null stream means standard output.

```c
size_t ppf_out_str(
    FILE *stream,
    int base,
    size_t digits,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** The operation-specific count described above, rather than a numerical ternary. Failure indicator: `0 when the operation cannot produce a count`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** Unrecovered project-allocation failure during text construction occurs before stream output; the source number is unchanged; a later stream write failure may leave a partial record and is not rolled back.

### ppf_printf

Formatted output to standard output.

```c
int ppf_printf(const char *format, ...);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before stream output; unrecovered project-allocation failure during formatting leaves the stream unchanged; %n writes already performed are not rolled back, and a later stream write failure may leave partial output.

### ppf_snprintf

Length-bounded formatted output.

```c
int ppf_snprintf(char *buffer, size_t size, const char *format, ...);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before the character buffer is copied; unrecovered project-allocation failure leaves that buffer unchanged; %n writes already performed while formatting are not rolled back.

### ppf_sprintf

Format into a caller-provided buffer without a length limit; use only when the buffer is known to be large enough.

```c
int ppf_sprintf(char *buffer, const char *format, ...);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before the character buffer is copied; unrecovered project-allocation failure leaves that buffer unchanged; %n writes already performed while formatting are not rolled back.

### ppf_vasprintf

`va_list` form of `ppf_asprintf`; release the returned buffer with `free`.

```c
int ppf_vasprintf(char **text, const char *format, va_list ap);
```

**Result.** A formatting count; a negative result reports failure.

**Ownership.** on success *text is allocated with the process allocator and must be released with free.

**Allocation and failure.** Sets *text to NULL before formatting; failure releases temporary storage and leaves *text NULL, rather than preserving its prior pointer; %n writes already performed while formatting are not rolled back.

### ppf_vfprintf

`va_list` form of `ppf_fprintf`.

```c
int ppf_vfprintf(FILE *stream, const char *format, va_list ap);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before stream output; unrecovered project-allocation failure during formatting leaves the stream unchanged; %n writes already performed are not rolled back, and a later stream write failure may leave partial output.

### ppf_vprintf

`va_list` form of `ppf_printf`.

```c
int ppf_vprintf(const char *format, va_list ap);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before stream output; unrecovered project-allocation failure during formatting leaves the stream unchanged; %n writes already performed are not rolled back, and a later stream write failure may leave partial output.

### ppf_vsnprintf

`va_list` form of `ppf_snprintf`.

```c
int ppf_vsnprintf(
    char *buffer,
    size_t size,
    const char *format,
    va_list ap
);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before the character buffer is copied; unrecovered project-allocation failure leaves that buffer unchanged; %n writes already performed while formatting are not rolled back.

### ppf_vsprintf

`va_list` form of `ppf_sprintf`.

```c
int ppf_vsprintf(char *buffer, const char *format, va_list ap);
```

**Result.** A formatting count; a negative result reports failure.

**Allocation and failure.** Formatting completes in temporary storage before the character buffer is copied; unrecovered project-allocation failure leaves that buffer unchanged; %n writes already performed while formatting are not rolled back.

## Comparison, ordering, sign, and absolute value

### ppf_abs

Take an absolute value with requested destination rounding.

```c
int ppf_abs(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_cmp_d

Compare with a double without rounding the Polyphase operand.

```c
int ppf_cmp_d(const ppf_t *op, double value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_f

Compare with a GMP float (`mpf_t`) without rounding the Polyphase operand.

```c
int ppf_cmp_f(const ppf_t *op, mpf_srcptr value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_ld

Compare with a long double without rounding the Polyphase operand.

```c
int ppf_cmp_ld(const ppf_t *op, long double value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_q

Compare with a GMP rational (`mpq_t`) without rounding the Polyphase operand.

```c
int ppf_cmp_q(const ppf_t *op, mpq_srcptr value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_si

Compare with a signed long without rounding the Polyphase operand.

```c
int ppf_cmp_si(const ppf_t *op, long value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_si_2exp

Compare with an exact signed long times a binary power.

```c
int ppf_cmp_si_2exp(
    const ppf_t *op,
    long value,
    ppf_exp_t exponent,
    ppf_context_t *ctx
);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_ui

Compare with a unsigned long without rounding the Polyphase operand.

```c
int ppf_cmp_ui(const ppf_t *op, unsigned long value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_ui_2exp

Compare with an exact unsigned long times a binary power.

```c
int ppf_cmp_ui_2exp(
    const ppf_t *op,
    unsigned long value,
    ppf_exp_t exponent,
    ppf_context_t *ctx
);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmp_z

Compare with a GMP integer (`mpz_t`) without rounding the Polyphase operand.

```c
int ppf_cmp_z(const ppf_t *op, mpz_srcptr value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmpabs

Compare absolute values without rounding either input.

```c
int ppf_cmpabs(const ppf_t *a, const ppf_t *b, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_cmpabs_ui

Compare a value's absolute magnitude with an unsigned long.

```c
int ppf_cmpabs_ui(const ppf_t *op, unsigned long value, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_greater_p

IEEE-style greater-than predicate.

```c
int ppf_greater_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_greaterequal_p

IEEE-style greater-than-or-equal predicate.

```c
int ppf_greaterequal_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_less_p

IEEE-style less-than predicate.

```c
int ppf_less_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_lessequal_p

IEEE-style less-than-or-equal predicate.

```c
int ppf_lessequal_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_lessgreater_p

Test whether two values are ordered and unequal.

```c
int ppf_lessgreater_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_neg

Negate a value with requested destination rounding.

```c
int ppf_neg(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_total_order_p

Apply a total ordering that includes signed zeros, infinities, and signed NaNs.

```c
int ppf_total_order_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_unordered_p

Test whether a comparison is unordered because at least one operand is NaN.

```c
int ppf_unordered_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

## Integer rounding, fractional parts, and remainders

### ppf_ceil

Round to an integral value toward positive infinity.

```c
int ppf_ceil(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. the function name selects the integer-rounding rule.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_floor

Round to an integral value toward negative infinity.

```c
int ppf_floor(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. the function name selects the integer-rounding rule.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_fmod

Compute the truncating-quotient remainder.

```c
int ppf_fmod(
    ppf_t *rop,
    const ppf_t *x,
    const ppf_t *y,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_fmod_ui

Compute the truncating-quotient remainder by an unsigned long divisor.

```c
int ppf_fmod_ui(
    ppf_t *rop,
    const ppf_t *x,
    unsigned long y,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fmodquo

Compute `fmod` and return selected low quotient bits.

```c
int ppf_fmodquo(
    ppf_t *rop,
    long *quotient_bits,
    const ppf_t *x,
    const ppf_t *y,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_frac

Return the signed fractional part.

```c
int ppf_frac(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_integer_p

Test whether a finite value is mathematically integral.

```c
int ppf_integer_p(const ppf_t *op);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_min_prec

Return the minimum precision needed to represent the current value exactly.

```c
ppf_prec_t ppf_min_prec(const ppf_t *op);
```

**Result.** The value described above, with the declared C return type.

### ppf_modf

Split one value into integral and fractional Polyphase outputs.

```c
int ppf_modf(
    ppf_t *integer_part,
    ppf_t *fractional_part,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** Test for `PPF_TERNARY_ERROR` before decoding. Otherwise the return is code(first) + 4 * code(second), where 0 means exact, 1 means above the exact result, and 2 means below it: the integral output first and the fractional output second. The sign of the packed return is not a numerical ternary. Failure indicator: `PPF_TERNARY_ERROR`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** either floating output may alias the floating input individually; Forbidden: the two floating outputs must not be the same object.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_remainder

Compute the IEEE nearest-integer-quotient remainder.

```c
int ppf_remainder(
    ppf_t *rop,
    const ppf_t *x,
    const ppf_t *y,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_remquo

Compute the IEEE remainder and selected low quotient bits.

```c
int ppf_remquo(
    ppf_t *rop,
    long *quotient_bits,
    const ppf_t *x,
    const ppf_t *y,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_rint

Round to an integral Polyphase value using a selected rounding mode.

```c
int ppf_rint(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_rint_ceil

`ppf_rint` convenience form using upward rounding.

```c
int ppf_rint_ceil(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_rint_floor

`ppf_rint` convenience form using downward rounding.

```c
int ppf_rint_floor(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_rint_round

`ppf_rint` convenience form using nearest with ties away from zero.

```c
int ppf_rint_round(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_rint_roundeven

`ppf_rint` convenience form using nearest with ties to even.

```c
int ppf_rint_roundeven(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_rint_trunc

`ppf_rint` convenience form using truncation.

```c
int ppf_rint_trunc(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_round

Round to nearest integer with halfway cases away from zero.

```c
int ppf_round(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. the function name selects the integer-rounding rule.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_roundeven

Round to nearest integer with halfway cases to even.

```c
int ppf_roundeven(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. the function name selects the integer-rounding rule.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_trunc

Round to an integral value toward zero.

```c
int ppf_trunc(ppf_t *rop, const ppf_t *op, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. the function name selects the integer-rounding rule.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

## Neighboring values, min/max, and exponent access

### ppf_dim

Compute the positive difference `max(a-b, 0)`.

```c
int ppf_dim(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_get_exp

Return the stored binary exponent of a regular value.

```c
ppf_exp_t ppf_get_exp(const ppf_t *x, ppf_context_t *ctx);
```

**Result.** The value described above, with the declared C return type.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_max

Return the selected maximum of two values with defined NaN and signed-zero behavior.

```c
int ppf_max(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_min

Return the selected minimum of two values with defined NaN and signed-zero behavior.

```c
int ppf_min(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_nextabove

Move one representable step toward positive infinity.

```c
void ppf_nextabove(ppf_t *x, ppf_context_t *ctx);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_nextbelow

Move one representable step toward negative infinity.

```c
void ppf_nextbelow(ppf_t *x, ppf_context_t *ctx);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_nexttoward

Move one representable step from one value toward another.

```c
void ppf_nexttoward(ppf_t *x, const ppf_t *y, ppf_context_t *ctx);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_set_exp

Replace the stored binary exponent after range validation.

```c
int ppf_set_exp(ppf_t *x, ppf_exp_t exponent, ppf_context_t *ctx);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Overlap.** the designated object is mutated in place.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

## Random values, build information, cache control, and custom storage

### ppf_buildopt_decimal_p

Report whether decimal64/decimal128 conversion support was compiled.

```c
int ppf_buildopt_decimal_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_buildopt_float128_p

Report whether binary128 conversion support was compiled.

```c
int ppf_buildopt_float128_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_buildopt_gmpinternals_p

Report whether optional GMP-internal integration is active.

```c
int ppf_buildopt_gmpinternals_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_buildopt_sharedcache_p

Report whether shared-cache support is active.

```c
int ppf_buildopt_sharedcache_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_buildopt_tls_p

Report whether thread-local storage support is active.

```c
int ppf_buildopt_tls_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_buildopt_tune_case

Return the build tuning-case description.

```c
const char *ppf_buildopt_tune_case(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_custom_get_exp

Return the external object exponent.

```c
ppf_exp_t ppf_custom_get_exp(const ppf_t *x);
```

**Result.** The value described above, with the declared C return type.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

### ppf_custom_get_kind

Return the public kind code of an externally stored value.

```c
int ppf_custom_get_kind(const ppf_t *x);
```

**Result.** The value described above, with the declared C return type.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

### ppf_custom_get_significand

Return the address of an external object's significand storage.

```c
void *ppf_custom_get_significand(const ppf_t *x);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returns the currently attached application-owned significand storage.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

### ppf_custom_get_size

Return the application-owned byte count required for an externally backed object.

```c
size_t ppf_custom_get_size(ppf_prec_t precision);
```

**Result.** The operation-specific count described above, rather than a numerical ternary. Failure indicator: `0 when the operation cannot produce a count`.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

### ppf_custom_init

Initialize raw external storage for a chosen precision.

```c
void ppf_custom_init(void *significand, ppf_prec_t precision);
```

**Result.** No return value.

**Ownership.** initializes caller-provided storage without taking ownership.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

### ppf_custom_init_set

Attach a `ppf_t` view to application-owned storage and set its initial kind, exponent, and precision.

```c
int ppf_custom_init_set(
    ppf_t *x,
    int kind,
    ppf_exp_t exponent,
    ppf_prec_t precision,
    void *significand
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Ownership.** attaches caller-provided storage without taking ownership.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_custom_move

Move an existing value into application-owned storage.

```c
void ppf_custom_move(ppf_t *x, void *new_position);
```

**Result.** No return value.

**Ownership.** moves the object view to replacement caller-owned storage.

**Overlap.** overlap is governed by the advanced direct-storage preconditions; Forbidden: overlap that violates the documented storage-size, commit, or ownership preconditions.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_div_2exp

Divide by an unsigned binary power for MPF-style compatibility.

```c
int ppf_div_2exp(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_eq

Test equality to a requested number of leading bits for MPF-style compatibility.

```c
int ppf_eq(const ppf_t *op1, const ppf_t *op2, unsigned long bits);
```

**Result.** Returns nonzero when the operands agree to the requested number of leading bits, and 0 otherwise.

### ppf_erandom

Generate an exponentially distributed random value with certified rounding.

```c
int ppf_erandom(
    ppf_t *rop,
    gmp_randstate_t state,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** adaptive random-distribution rounding uses at most 12 precision-escalation attempts. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** Application locking is required for shared mutable use.

### ppf_free_cache

Release the calling thread's local caches.

```c
void ppf_free_cache(void);
```

**Result.** No return value.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_free_cache2

Release selected local and/or global caches.

```c
void ppf_free_cache2(ppf_free_cache_t way);
```

**Result.** No return value.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_free_pool

Release reusable memory-pool resources.

```c
void ppf_free_pool(void);
```

**Result.** No return value.

**Concurrency.** This operation concerns the calling thread's state; it is not a mechanism for sharing a mutable context.

### ppf_get_patches

Return the patch-level description string.

```c
const char *ppf_get_patches(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_get_version

Return the library version string.

```c
const char *ppf_get_version(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_grandom

Generate two normally distributed values as a pair.

```c
int ppf_grandom(
    ppf_t *rop1,
    ppf_t *rop2,
    gmp_randstate_t state,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** Test for `PPF_TERNARY_ERROR` before decoding. Otherwise the return is code(first) + 4 * code(second), where 0 means exact, 1 means above the exact result, and 2 means below it: rop1 first and rop2 second; when rop2 is NULL its component code is 0. The sign of the packed return is not a numerical ternary. Failure indicator: `PPF_TERNARY_ERROR`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the optional second output may be NULL; Forbidden: the two floating outputs must not be the same object.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** adaptive random-distribution rounding uses at most 12 precision-escalation attempts. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** Application locking is required for shared mutable use.

### ppf_mp_memory_cleanup

Run the library's broad multiprecision memory cleanup.

```c
int ppf_mp_memory_cleanup(void);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

### ppf_mul_2exp

Multiply by an unsigned binary power for MPF-style compatibility.

```c
int ppf_mul_2exp(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** Reviewed valid initialized-storage paths do not call the project allocator; existing capacity, direct-storage and ownership preconditions apply; GMP allocation is outside this claim.

### ppf_nrandom

Generate a normally distributed random value with certified rounding.

```c
int ppf_nrandom(
    ppf_t *rop,
    gmp_randstate_t state,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** adaptive random-distribution rounding uses at most 12 precision-escalation attempts. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** Application locking is required for shared mutable use.

### ppf_reldiff

Compute a relative difference for MPF-style compatibility.

```c
void ppf_reldiff(
    ppf_t *rop,
    const ppf_t *op1,
    const ppf_t *op2,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** No return value.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_urandom

Generate a uniformly distributed floating value in the supported unit interval convention.

```c
int ppf_urandom(
    ppf_t *rop,
    gmp_randstate_t state,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** adaptive random-distribution rounding uses at most 12 precision-escalation attempts. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** Application locking is required for shared mutable use.

### ppf_urandomb

Generate a uniform random dyadic number from GMP random bits.

```c
int ppf_urandomb(ppf_t *rop, gmp_randstate_t state);
```

**Result.** Returns 0 on success and 1 on failure. The generated significand is exact at the destination precision; the return is not a numerical ternary. Failure indicator: `1`.

**Context and rounding.** the generated significand is exact at the destination precision.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Concurrency.** Application locking is required for shared mutable use.

## Build and ABI information

### ppf_abi_contract_sha256

Return the SHA-256 identifier of the normalized public ABI contract.

```c
const char *ppf_abi_contract_sha256(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_abi_epoch

Return the governed public ABI epoch.

```c
uint32_t ppf_abi_epoch(void);
```

**Result.** The value described above, with the declared C return type.

### ppf_abi_info_size

Return the size of the current ABI information structure.

```c
size_t ppf_abi_info_size(void);
```

**Result.** The operation-specific count described above, rather than a numerical ternary. Failure indicator: `0 when the operation cannot produce a count`.

### ppf_abi_revision

Return the additive revision within the current ABI epoch.

```c
uint32_t ppf_abi_revision(void);
```

**Result.** The value described above, with the declared C return type.

### ppf_abi_tag

Return the stable textual tag for the current ABI epoch.

```c
const char *ppf_abi_tag(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_buildopt_thread_cleanup_p

Report whether automatic POSIX thread-exit cleanup is available in this build.

```c
int ppf_buildopt_thread_cleanup_p(void);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_get_abi_info

Copy a size-bounded runtime description of the active public ABI into caller storage.

```c
int ppf_get_abi_info(ppf_abi_info_t *info, size_t info_size);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

## Arithmetic and product planning

### ppf_add

Add two Polyphase values with one final rounding.

```c
int ppf_add(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_add_d

Add a Polyphase operand and double with one final rounding.

```c
int ppf_add_d(
    ppf_t *rop,
    const ppf_t *op,
    double value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_add_q

Add a Polyphase operand and GMP rational (`mpq_t`) with one final rounding.

```c
int ppf_add_q(
    ppf_t *rop,
    const ppf_t *op,
    mpq_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_add_si

Add a Polyphase operand and signed long with one final rounding.

```c
int ppf_add_si(
    ppf_t *rop,
    const ppf_t *op,
    long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_add_ui

Add a Polyphase operand and unsigned long with one final rounding.

```c
int ppf_add_ui(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_add_z

Add a Polyphase operand and GMP integer (`mpz_t`) with one final rounding.

```c
int ppf_add_z(
    ppf_t *rop,
    const ppf_t *op,
    mpz_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_d_div

Compute double / Polyphase operand with one final rounding.

```c
int ppf_d_div(
    ppf_t *rop,
    double value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_d_sub

Compute double - Polyphase operand with one final rounding.

```c
int ppf_d_sub(
    ppf_t *rop,
    double value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div

Divide two Polyphase values with one final rounding.

```c
int ppf_div(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div_d

Compute Polyphase operand divided by double with one final rounding.

```c
int ppf_div_d(
    ppf_t *rop,
    const ppf_t *op,
    double value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div_q

Compute Polyphase operand divided by GMP rational (`mpq_t`) with one final rounding.

```c
int ppf_div_q(
    ppf_t *rop,
    const ppf_t *op,
    mpq_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div_si

Compute Polyphase operand divided by signed long with one final rounding.

```c
int ppf_div_si(
    ppf_t *rop,
    const ppf_t *op,
    long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div_ui

Compute Polyphase operand divided by unsigned long with one final rounding.

```c
int ppf_div_ui(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div_z

Compute Polyphase operand divided by GMP integer (`mpz_t`) with one final rounding.

```c
int ppf_div_z(
    ppf_t *rop,
    const ppf_t *op,
    mpz_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul

Multiply two Polyphase values with one final rounding.

```c
int ppf_mul(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_algorithm_threads

Multiply while explicitly forcing FULL, rank-3, rank-5, or window product selection.

```c
int ppf_mul_algorithm_threads(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_d

Multiply a Polyphase operand and double with one final rounding.

```c
int ppf_mul_d(
    ppf_t *rop,
    const ppf_t *op,
    double value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_q

Multiply a Polyphase operand and GMP rational (`mpq_t`) with one final rounding.

```c
int ppf_mul_q(
    ppf_t *rop,
    const ppf_t *op,
    mpq_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_si

Multiply a Polyphase operand and signed long with one final rounding.

```c
int ppf_mul_si(
    ppf_t *rop,
    const ppf_t *op,
    long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_threads

Multiply two values using AUTO product planning and the requested worker count.

```c
int ppf_mul_threads(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_ui

Multiply a Polyphase operand and unsigned long with one final rounding.

```c
int ppf_mul_ui(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_z

Multiply a Polyphase operand and GMP integer (`mpz_t`) with one final rounding.

```c
int ppf_mul_z(
    ppf_t *rop,
    const ppf_t *op,
    mpz_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_plan_product

Return the resolved algorithm, requested workers, effective workers, and profile-match status.

```c
int ppf_plan_product(
    ppf_product_plan_t *plan,
    const ppf_t *a,
    const ppf_t *b,
    ppf_product_operation_t operation,
    unsigned long workers
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

### ppf_plan_product_algorithm

Return the algorithm chosen by the portable or generated AUTO planner.

```c
ppf_product_algorithm_t ppf_plan_product_algorithm(
    const ppf_t *a,
    const ppf_t *b,
    int square,
    unsigned long workers
);
```

**Result.** The value described above, with the declared C return type.

### ppf_product_profile_name

Return the compiled product-profile name, such as `portable`.

```c
const char *ppf_product_profile_name(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_product_profile_sha256

Return the evidence hash of the compiled product profile.

```c
const char *ppf_product_profile_sha256(void);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_si_div

Compute signed long / Polyphase operand with one final rounding.

```c
int ppf_si_div(
    ppf_t *rop,
    long value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_si_sub

Compute signed long - Polyphase operand with one final rounding.

```c
int ppf_si_sub(
    ppf_t *rop,
    long value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sqr

Square one Polyphase value with one final rounding.

```c
int ppf_sqr(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sqr_algorithm_threads

Square while explicitly forcing a selected product algorithm.

```c
int ppf_sqr_algorithm_threads(
    ppf_t *rop,
    const ppf_t *a,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sqr_threads

Square a value using AUTO product planning and the requested worker count.

```c
int ppf_sqr_threads(
    ppf_t *rop,
    const ppf_t *a,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sub

Subtract two Polyphase values with one final rounding.

```c
int ppf_sub(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sub_d

Compute Polyphase operand minus double with one final rounding.

```c
int ppf_sub_d(
    ppf_t *rop,
    const ppf_t *op,
    double value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sub_q

Compute Polyphase operand minus GMP rational (`mpq_t`) with one final rounding.

```c
int ppf_sub_q(
    ppf_t *rop,
    const ppf_t *op,
    mpq_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sub_si

Compute Polyphase operand minus signed long with one final rounding.

```c
int ppf_sub_si(
    ppf_t *rop,
    const ppf_t *op,
    long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sub_ui

Compute Polyphase operand minus unsigned long with one final rounding.

```c
int ppf_sub_ui(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sub_z

Compute Polyphase operand minus GMP integer (`mpz_t`) with one final rounding.

```c
int ppf_sub_z(
    ppf_t *rop,
    const ppf_t *op,
    mpz_srcptr value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_ui_div

Compute unsigned long / Polyphase operand with one final rounding.

```c
int ppf_ui_div(
    ppf_t *rop,
    unsigned long value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_ui_sub

Compute unsigned long - Polyphase operand with one final rounding.

```c
int ppf_ui_sub(
    ppf_t *rop,
    unsigned long value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_z_sub

Compute GMP integer (`mpz_t`) - Polyphase operand with one final rounding.

```c
int ppf_z_sub(
    ppf_t *rop,
    mpz_srcptr value,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

## Roots and hypotenuse

### ppf_cbrt

Compute the real cube root.

```c
int ppf_cbrt(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Input contract.** defined for finite real inputs.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** nontrivial exact-input root interval construction uses `PPF_ROOT_ATTEMPTS` refinement attempts (default 10); exact and exceptional-value cases may return earlier. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_hypot

Compute `sqrt(a*a + b*b)` with one final rounding.

```c
int ppf_hypot(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** the GMP integer fallback uses root interval construction with `PPF_ROOT_ATTEMPTS` refinement attempts (default 10); the direct-limb path calls the square-root operation with its separate 24-attempt limit; these are path-specific limits, not one global attempt budget. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_rec_sqrt

Compute the reciprocal square root.

```c
int ppf_rec_sqrt(
    ppf_t *rop,
    const ppf_t *a,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** the Newton interval branch and the root-plus-quotient fallback each have a `PPF_ROOT_ATTEMPTS` refinement limit (default 10 per branch), not a shared global total of 10 attempts; exact and exceptional-value cases may return earlier. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_rec_sqrt_algorithm_threads

Compute reciprocal square root while forcing a selected product algorithm.

```c
int ppf_rec_sqrt_algorithm_threads(
    ppf_t *rop,
    const ppf_t *a,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** the Newton interval branch and the root-plus-quotient fallback each have a `PPF_ROOT_ATTEMPTS` refinement limit (default 10 per branch), not a shared global total of 10 attempts; exact and exceptional-value cases may return earlier. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_rec_sqrt_threads

Compute the reciprocal square root using an internal worker request.

```c
int ppf_rec_sqrt_threads(
    ppf_t *rop,
    const ppf_t *a,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** the Newton interval branch and the root-plus-quotient fallback each have a `PPF_ROOT_ATTEMPTS` refinement limit (default 10 per branch), not a shared global total of 10 attempts; exact and exceptional-value cases may return earlier. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_root

Legacy unsigned-degree root interface with its documented signed-zero convention.

```c
int ppf_root(
    ppf_t *rop,
    const ppf_t *a,
    unsigned long degree,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** degree must be nonzero; even degrees require nonnegative finite input.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** nontrivial exact-input root interval construction uses `PPF_ROOT_ATTEMPTS` refinement attempts (default 10); exact and exceptional-value cases may return earlier. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_rootn_si

Compute an integer-degree root; a negative degree requests a reciprocal root.

```c
int ppf_rootn_si(
    ppf_t *rop,
    const ppf_t *a,
    long degree,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** degree must be nonzero; even degrees require nonnegative finite input; negative degrees additionally exclude zero.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** nontrivial exact-input root interval construction uses `PPF_ROOT_ATTEMPTS` refinement attempts (default 10); exact and exceptional-value cases may return earlier. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_rootn_ui

Compute an integer-degree root; unsigned degree.

```c
int ppf_rootn_ui(
    ppf_t *rop,
    const ppf_t *a,
    unsigned long degree,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** degree must be nonzero; even degrees require nonnegative finite input.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** nontrivial exact-input root interval construction uses `PPF_ROOT_ATTEMPTS` refinement attempts (default 10); exact and exceptional-value cases may return earlier. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sqrt

Compute the square root.

```c
int ppf_sqrt(ppf_t *rop, const ppf_t *a, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Input contract.** finite regular inputs require x >= 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** square-root interval refinement is limited to 24 attempts; exact and exceptional-value cases may return earlier; ppf_sqrt_ui delegates to the same square-root operation. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sqrt_ui

Compute the square root of an unsigned long.

```c
int ppf_sqrt_ui(
    ppf_t *rop,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** defined for every unsigned input.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** square-root interval refinement is limited to 24 attempts; exact and exceptional-value cases may return earlier; ppf_sqrt_ui delegates to the same square-root operation. Consult the numerical-limits appendix for the complete cap inventory.

## Reusable division, fused operations, powers of two, sums, and dot products

### ppf_div_2si

Divide by `2^shift` with a signed shift.

```c
int ppf_div_2si(
    ppf_t *rop,
    const ppf_t *a,
    long shift,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div_2ui

Divide by `2^shift` with an unsigned shift.

```c
int ppf_div_2ui(
    ppf_t *rop,
    const ppf_t *a,
    unsigned long shift,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_div_with_plan

Divide one numerator by a planned denominator.

```c
int ppf_div_with_plan(
    ppf_t *rop,
    const ppf_t *numerator,
    const ppf_divisor_plan_t *plan,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** plan must be complete and numerator valid.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Concurrency.** Share the plan only under its documented immutable-plan and lifetime rules.

### ppf_div_with_plan_many

Divide many independent numerators by one immutable plan, optionally in parallel.

```c
int ppf_div_with_plan_many(
    ppf_t *outputs[],
    const ppf_t *const numerators[],
    size_t length,
    const ppf_divisor_plan_t *plan,
    ppf_rnd_t rnd,
    unsigned long workers,
    int ternaries[],
    ppf_context_t *ctx
);
```

**Input contract.** plan and every output/numerator entry must be valid.

**Result.** Returns 0 on success and -1 on failure. When supplied, the ternaries array receives the individual numerical ternary results; the function return is not a numerical ternary. Failure indicator: `-1`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** outputs[i] may equal numerators[i] for each index; Forbidden: duplicate outputs and cross-index output/input overlap.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Concurrency.** Share the plan only under its documented immutable-plan and lifetime rules.

### ppf_divisor_plan_applications

Return how many successful applications the plan has recorded.

```c
unsigned long ppf_divisor_plan_applications(
    const ppf_divisor_plan_t *plan
);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** Share the plan only under its documented immutable-plan and lifetime rules.

### ppf_divisor_plan_create

Create an immutable plan for repeatedly dividing into one destination precision.

```c
int ppf_divisor_plan_create(
    ppf_divisor_plan_t **out,
    const ppf_t *denominator,
    ppf_prec_t destination_precision,
    ppf_context_t *ctx
);
```

**Input contract.** denominator must be finite and nonzero; destination precision must be valid.

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Ownership.** successful creation returns a plan released by ppf_divisor_plan_destroy.

**Concurrency.** Application locking is required for shared mutable use.

### ppf_divisor_plan_create_ex

Create a reusable divisor plan with an expected application count for planning.

```c
int ppf_divisor_plan_create_ex(
    ppf_divisor_plan_t **out,
    const ppf_t *denominator,
    ppf_prec_t destination_precision,
    unsigned long expected_applications,
    ppf_context_t *ctx
);
```

**Input contract.** denominator must be finite and nonzero; destination precision must be valid.

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

**Ownership.** successful creation returns a plan released by ppf_divisor_plan_destroy.

**Allocation and failure.** After initial argument validation, *out is set to NULL; project-allocation failure releases the incomplete plan and leaves *out NULL and the denominator unchanged; this does not preserve an existing pointer passed through out.

**Concurrency.** Application locking is required for shared mutable use.

### ppf_divisor_plan_destroy

Destroy a reusable divisor plan.

```c
void ppf_divisor_plan_destroy(ppf_divisor_plan_t *plan);
```

**Result.** No return value.

**Ownership.** releases a completed or partially constructed divisor plan.

**Concurrency.** Application locking is required for shared mutable use.

### ppf_divisor_plan_precision

Return the destination precision stored in a divisor plan.

```c
ppf_prec_t ppf_divisor_plan_precision( const ppf_divisor_plan_t *plan);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** Share the plan only under its documented immutable-plan and lifetime rules.

### ppf_dot

Compute a dot product with exact products and one final rounding.

```c
int ppf_dot(
    ppf_t *rop,
    const ppf_t *const a[],
    const ppf_t *const b[],
    size_t length,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_dot_threads

Compute a dot product with parallel product leaves and one final rounding.

```c
int ppf_dot_threads(
    ppf_t *rop,
    const ppf_t *const a[],
    const ppf_t *const b[],
    size_t length,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fac_ui

Compute an unsigned integer factorial.

```c
int ppf_fac_ui(
    ppf_t *rop,
    unsigned long value,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fma

Compute `a*b + c` with one final rounding.

```c
int ppf_fma(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fma_algorithm_threads

Threaded FMA while forcing a selected exact product graph.

```c
int ppf_fma_algorithm_threads(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fma_threads

Threaded FMA using AUTO product selection.

```c
int ppf_fma_threads(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fmma

Compute `a*b + c*d` with one final rounding.

```c
int ppf_fmma(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    const ppf_t *d,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fmms

Compute `a*b - c*d` with one final rounding.

```c
int ppf_fmms(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    const ppf_t *d,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fms

Compute `a*b - c` with one final rounding.

```c
int ppf_fms(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fms_algorithm_threads

Threaded fused multiply-subtract while forcing a product graph.

```c
int ppf_fms_algorithm_threads(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_product_algorithm_t algorithm,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_fms_threads

Threaded fused multiply-subtract using AUTO product selection.

```c
int ppf_fms_threads(
    ppf_t *rop,
    const ppf_t *a,
    const ppf_t *b,
    const ppf_t *c,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_2si

Multiply by `2^shift` with a signed shift.

```c
int ppf_mul_2si(
    ppf_t *rop,
    const ppf_t *a,
    long shift,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_mul_2ui

Multiply by `2^shift` with an unsigned shift.

```c
int ppf_mul_2ui(
    ppf_t *rop,
    const ppf_t *a,
    unsigned long shift,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

### ppf_sum

Sum an array exactly before one final rounding.

```c
int ppf_sum(
    ppf_t *rop,
    const ppf_t *const values[],
    size_t length,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

## Logarithms, exponentials, and powers

### ppf_compound_si

Compute the compound-interest form `(1 + op)^exponent` with signed integer exponent.

```c
int ppf_compound_si(
    ppf_t *rop,
    const ppf_t *op,
    long exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_exp

Compute the natural exponential with certified final rounding.

```c
int ppf_exp(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_exp10

Compute the base-10 exponential with certified final rounding.

```c
int ppf_exp10(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_exp10m1

Compute the 10^x-1 with certified final rounding.

```c
int ppf_exp10m1(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_exp2

Compute the base-2 exponential with certified final rounding.

```c
int ppf_exp2(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_exp2m1

Compute the 2^x-1 with certified final rounding.

```c
int ppf_exp2m1(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_expm1

Compute the exp(x)-1 with certified final rounding.

```c
int ppf_expm1(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_log

Compute the natural logarithm with certified final rounding.

```c
int ppf_log(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Input contract.** finite regular inputs require x > 0; zero and negative inputs follow documented exceptional-value semantics.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_log10

Compute the base-10 logarithm with certified final rounding.

```c
int ppf_log10(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_log10p1

Compute the base-10 logarithm of 1+x with certified final rounding.

```c
int ppf_log10p1(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > -1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_log1p

Compute the natural logarithm of 1+x with certified final rounding.

```c
int ppf_log1p(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > -1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_log2

Compute the base-2 logarithm with certified final rounding.

```c
int ppf_log2(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_log2p1

Compute the base-2 logarithm of 1+x with certified final rounding.

```c
int ppf_log2p1(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require x > -1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_log_ui

Compute the natural logarithm of an unsigned integer with certified final rounding.

```c
int ppf_log_ui(
    ppf_t *rop,
    unsigned long op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_pow

Compute the general real power with certified final rounding.

```c
int ppf_pow(
    ppf_t *rop,
    const ppf_t *base,
    const ppf_t *exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_pow_si

Compute the power with a signed long exponent with certified final rounding.

```c
int ppf_pow_si(
    ppf_t *rop,
    const ppf_t *base,
    long exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_pow_sj

Compute the power with an intmax_t exponent with certified final rounding.

```c
int ppf_pow_sj(
    ppf_t *rop,
    const ppf_t *base,
    intmax_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_pow_ui

Compute the power with an unsigned long exponent with certified final rounding.

```c
int ppf_pow_ui(
    ppf_t *rop,
    const ppf_t *base,
    unsigned long exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_pow_uj

Compute the power with a uintmax_t exponent with certified final rounding.

```c
int ppf_pow_uj(
    ppf_t *rop,
    const ppf_t *base,
    uintmax_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_pow_z

Compute the power with a GMP integer exponent with certified final rounding.

```c
int ppf_pow_z(
    ppf_t *rop,
    const ppf_t *base,
    mpz_srcptr exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_pown

Compute the power with a signed integer exponent using the `pown` entry with certified final rounding.

```c
int ppf_pown(
    ppf_t *rop,
    const ppf_t *base,
    intmax_t exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_powr

Compute the real power with the restricted nonnegative-base `powr` domain with certified final rounding.

```c
int ppf_powr(
    ppf_t *rop,
    const ppf_t *base,
    const ppf_t *exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** real-valued power function requiring a nonnegative base, with zero cases handled separately.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_ui_pow

Compute the unsigned-long base to Polyphase exponent with certified final rounding.

```c
int ppf_ui_pow(
    ppf_t *rop,
    unsigned long base,
    const ppf_t *exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_ui_pow_ui

Compute the unsigned-long base to unsigned-long exponent with certified final rounding.

```c
int ppf_ui_pow_ui(
    ppf_t *rop,
    unsigned long base,
    unsigned long exponent,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** certification uses bounded escalation and at most 10,000,000 series or reduction steps in the general backend; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits by default, also bounding the endpoint-to-one exponent gap before exponential-minus-one subtraction; integer-power shortcuts use a precision-dependent budget; unusable certificates retry or reject. Consult the numerical-limits appendix for the complete cap inventory.

## Trigonometric and hyperbolic functions

### ppf_acos

Compute inverse cosine in radians with certified final rounding.

```c
int ppf_acos(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require -1 <= x <= 1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_acosh

Compute inverse hyperbolic cosine with certified final rounding.

```c
int ppf_acosh(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite real inputs require x >= 1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_acospi

Compute inverse cosine divided by pi with certified final rounding.

```c
int ppf_acospi(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require -1 <= x <= 1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_acosu

Compute the inverse cos scaled into units of `u/(2*pi)`.

```c
int ppf_acosu(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long u,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require -1 <= x <= 1 and u != 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_asin

Compute inverse sine in radians with certified final rounding.

```c
int ppf_asin(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require -1 <= x <= 1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_asinh

Compute inverse hyperbolic sine with certified final rounding.

```c
int ppf_asinh(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_asinpi

Compute inverse sine divided by pi with certified final rounding.

```c
int ppf_asinpi(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require -1 <= x <= 1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_asinu

Compute the inverse sin scaled into units of `u/(2*pi)`.

```c
int ppf_asinu(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long u,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite regular inputs require -1 <= x <= 1 and u != 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_atan

Compute inverse tangent in radians with certified final rounding.

```c
int ppf_atan(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_atan2

Compute quadrant-aware inverse tangent in radians with certified final rounding.

```c
int ppf_atan2(
    ppf_t *rop,
    const ppf_t *y,
    const ppf_t *x,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_atan2pi

Compute quadrant-aware inverse tangent divided by pi with certified final rounding.

```c
int ppf_atan2pi(
    ppf_t *rop,
    const ppf_t *y,
    const ppf_t *x,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_atan2u

Compute quadrant-aware atan2 scaled into units of `u/(2*pi)`.

```c
int ppf_atan2u(
    ppf_t *rop,
    const ppf_t *y,
    const ppf_t *x,
    unsigned long u,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_atanh

Compute inverse hyperbolic tangent with certified final rounding.

```c
int ppf_atanh(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite real result requires -1 < x < 1; endpoints and exterior values follow exceptional semantics.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_atanpi

Compute inverse tangent divided by pi with certified final rounding.

```c
int ppf_atanpi(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_atanu

Compute the inverse tan scaled into units of `u/(2*pi)`.

```c
int ppf_atanu(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long u,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** u must be nonzero.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_cos

Compute cosine in radians with certified final rounding.

```c
int ppf_cos(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_cosh

Compute hyperbolic cosine with certified final rounding.

```c
int ppf_cosh(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_cospi

Compute cos(pi*x) with certified final rounding.

```c
int ppf_cospi(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_cosu

Compute cos(2*pi*x/u), where `u` is an unsigned turn-unit denominator.

```c
int ppf_cosu(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long u,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_cot

Compute cotangent with certified final rounding.

```c
int ppf_cot(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_coth

Compute hyperbolic cotangent with certified final rounding.

```c
int ppf_coth(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_csc

Compute cosecant with certified final rounding.

```c
int ppf_csc(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_csch

Compute hyperbolic cosecant with certified final rounding.

```c
int ppf_csch(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_get_trig_threads

Read the current trigonometric worker request.

```c
unsigned long ppf_get_trig_threads(void);
```

**Result.** The value described above, with the declared C return type.

### ppf_sec

Compute secant with certified final rounding.

```c
int ppf_sec(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sech

Compute hyperbolic secant with certified final rounding.

```c
int ppf_sech(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_set_trig_threads

Set the default internal worker request for trigonometric calculations.

```c
void ppf_set_trig_threads(unsigned long workers);
```

**Result.** No return value.

### ppf_sin

Compute sine in radians with certified final rounding.

```c
int ppf_sin(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sin_cos

Compute sine and cosine into two outputs.

```c
int ppf_sin_cos(
    ppf_t *sop,
    ppf_t *cop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** Test for `PPF_TERNARY_ERROR` before decoding. Otherwise the return is code(first) + 4 * code(second), where 0 means exact, 1 means above the exact result, and 2 means below it: the sine output first and the cosine output second. The sign of the packed return is not a numerical ternary. Failure indicator: `PPF_TERNARY_ERROR`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** either floating output may alias the floating input individually; Forbidden: the two floating outputs must not be the same object.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sinh

Compute hyperbolic sine with certified final rounding.

```c
int ppf_sinh(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sinh_cosh

Compute hyperbolic sine and cosine into two outputs.

```c
int ppf_sinh_cosh(
    ppf_t *sop,
    ppf_t *cop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** Test for `PPF_TERNARY_ERROR` before decoding. Otherwise the return is code(first) + 4 * code(second), where 0 means exact, 1 means above the exact result, and 2 means below it: the hyperbolic sine output first and the hyperbolic cosine output second. The sign of the packed return is not a numerical ternary. Failure indicator: `PPF_TERNARY_ERROR`.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** either floating output may alias the floating input individually; Forbidden: the two floating outputs must not be the same object.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sinpi

Compute sin(pi*x) with certified final rounding.

```c
int ppf_sinpi(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_sinu

Compute sin(2*pi*x/u), where `u` is an unsigned turn-unit denominator.

```c
int ppf_sinu(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long u,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_tan

Compute tangent in radians with certified final rounding.

```c
int ppf_tan(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_tanh

Compute hyperbolic tangent with certified final rounding.

```c
int ppf_tanh(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_tanpi

Compute tan(pi*x) with certified final rounding.

```c
int ppf_tanpi(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_tanu

Compute tan(2*pi*x/u), where `u` is an unsigned turn-unit denominator.

```c
int ppf_tanu(
    ppf_t *rop,
    const ppf_t *op,
    unsigned long u,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** general trigonometric certification uses bounded escalation, bounded reduction growth, and at most 10,000,000 steps. Consult the numerical-limits appendix for the complete cap inventory.

## Special functions

### ppf_agm

Compute the arithmetic-geometric mean with certified final rounding.

```c
int ppf_agm(
    ppf_t *rop,
    const ppf_t *op1,
    const ppf_t *op2,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_ai

Compute the Airy Ai function with certified final rounding.

```c
int ppf_ai(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Input contract.** implemented real-domain path is restricted to |x| <= 500; infinities are handled separately.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates; the current Airy Ai path additionally rejects finite |x| > 500. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_beta

Compute the Beta function with certified final rounding.

```c
int ppf_beta(
    ppf_t *rop,
    const ppf_t *op1,
    const ppf_t *op2,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** real arguments where the Gamma quotient is finite; singular combinations follow exceptional semantics.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_digamma

Compute the digamma function with certified final rounding.

```c
int ppf_digamma(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** real input excluding nonpositive integer poles.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_eint

Compute the real exponential integral Ei with certified final rounding.

```c
int ppf_eint(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_erf

Compute the error function with certified final rounding.

```c
int ppf_erf(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_erfc

Compute the complementary error function with certified final rounding.

```c
int ppf_erfc(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_gamma

Compute the Gamma function with certified final rounding.

```c
int ppf_gamma(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** real input excluding nonpositive integer poles.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_gamma_inc

Compute the upper incomplete gamma function for two real arguments.

```c
int ppf_gamma_inc(
    ppf_t *rop,
    const ppf_t *op,
    const ppf_t *op2,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_j0

Compute the Bessel J of order 0 with certified final rounding.

```c
int ppf_j0(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_j1

Compute the Bessel J of order 1 with certified final rounding.

```c
int ppf_j1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_jn

Compute the Bessel J of integer order n with certified final rounding.

```c
int ppf_jn(
    ppf_t *rop,
    long n,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_lgamma

Compute `log(abs(Gamma(x)))` and return the sign of Gamma separately.

```c
int ppf_lgamma(
    ppf_t *rop,
    int *signp,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** real input; sign output records the sign of Gamma away from poles.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_li2

Compute the real dilogarithm Li2 with certified final rounding.

```c
int ppf_li2(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_lngamma

Compute the real logarithm of Gamma where Gamma is positive.

```c
int ppf_lngamma(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** real input excluding nonpositive integer poles when a finite real logarithm is required.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_y0

Compute the Bessel Y of order 0 with certified final rounding.

```c
int ppf_y0(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Input contract.** finite real series path requires x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_y1

Compute the Bessel Y of order 1 with certified final rounding.

```c
int ppf_y1(ppf_t *rop, const ppf_t *op, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Input contract.** finite real series path requires x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_yn

Compute the Bessel Y of integer order n with certified final rounding.

```c
int ppf_yn(
    ppf_t *rop,
    long n,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** finite real series path requires x > 0.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_zeta

Compute the Riemann zeta function with certified final rounding.

```c
int ppf_zeta(
    ppf_t *rop,
    const ppf_t *op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Input contract.** real input with a pole at x = 1.

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Overlap.** the floating destination may alias each floating source position exercised by the aliasing matrix.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

### ppf_zeta_ui

Compute the Riemann zeta at an unsigned integer with certified final rounding.

```c
int ppf_zeta_ui(
    ppf_t *rop,
    unsigned long op,
    ppf_rnd_t rnd,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** special-function certification uses bounded precision escalation; several series and recurrences are capped at 1,000,000 iterations; dyadic interval construction permits at most 1,000,000 extra endpoint-alignment bits and retries or rejects unusable certificates. Consult the numerical-limits appendix for the complete cap inventory.

## Certified constants and constant caches

### ppf_clear_constant_cache

Clear the cache for one selected constant.

```c
void ppf_clear_constant_cache(ppf_constant_id_t id);
```

**Result.** No return value.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_catalan

Compute Catalan's constant through the cached default-worker certified constant path.

```c
int ppf_const_catalan(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_catalan_threads

Compute Catalan's constant through the cached explicit-worker certified constant path.

```c
int ppf_const_catalan_threads(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_catalan_uncached

Compute Catalan's constant through the uncached explicit-worker certified constant path.

```c
int ppf_const_catalan_uncached(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_euler

Compute Euler's constant through the cached default-worker certified constant path.

```c
int ppf_const_euler(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_euler_threads

Compute Euler's constant through the cached explicit-worker certified constant path.

```c
int ppf_const_euler_threads(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_euler_uncached

Compute Euler's constant through the uncached explicit-worker certified constant path.

```c
int ppf_const_euler_uncached(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_log2

Compute log(2) through the cached default-worker certified constant path.

```c
int ppf_const_log2(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_log2_threads

Compute log(2) through the cached explicit-worker certified constant path.

```c
int ppf_const_log2_threads(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_log2_uncached

Compute log(2) through the uncached explicit-worker certified constant path.

```c
int ppf_const_log2_uncached(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_pi

Compute pi through the cached default-worker certified constant path.

```c
int ppf_const_pi(ppf_t *rop, ppf_rnd_t rnd, ppf_context_t *ctx);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_pi_threads

Compute pi through the cached explicit-worker certified constant path.

```c
int ppf_const_pi_threads(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_const_pi_uncached

Compute pi through the uncached explicit-worker certified constant path.

```c
int ppf_const_pi_uncached(
    ppf_t *rop,
    ppf_rnd_t rnd,
    unsigned long workers,
    ppf_context_t *ctx
);
```

**Result.** A numerical ternary result. Test for `PPF_TERNARY_ERROR` before interpreting its sign.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_constant_cache_precision

Return the precision currently retained for a constant cache.

```c
ppf_prec_t ppf_constant_cache_precision(ppf_constant_id_t id);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_free_constant_caches

Release all certified constant caches.

```c
void ppf_free_constant_caches(void);
```

**Result.** No return value.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_get_constant_interval

Export a certified dyadic interval around a supported constant.

```c
int ppf_get_constant_interval(
    ppf_constant_id_t id,
    mpz_ptr lower,
    mpz_ptr upper,
    ppf_exp_t *exponent,
    ppf_prec_t precision,
    unsigned long workers
);
```

**Result.** Returns 0 on success and -1 on failure. The certified dyadic interval is written to lower, upper, and exponent. Failure indicator: `-1`.

**Overlap.** lower and upper may refer to distinct caller-owned integers; Forbidden: lower and upper must not be the same mpz object.

**Allocation and failure.** Unrecovered project-allocation failure before publication preserves lower, upper, and exponent and releases temporary project-owned storage; GMP allocation while copying successful outputs is outside this recoverable allocation contract.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_get_constant_threads

Read the default worker request for constant construction.

```c
unsigned long ppf_get_constant_threads(void);
```

**Result.** The value described above, with the declared C return type.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_get_last_constant_stats

Read statistics from the most recent constant computation in the calling thread.

```c
int ppf_get_last_constant_stats(ppf_constant_stats_t *out);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_precompute_catalan

Precompute and cache catalan to a requested precision.

```c
int ppf_precompute_catalan(ppf_prec_t precision, unsigned long workers);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Allocation and failure.** Unrecovered project-allocation failure releases the incomplete interval without publishing it to the constant cache; an existing cached interval remains valid; GMP allocation is outside this recoverable allocation contract.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_precompute_constant

Precompute and cache constant to a requested precision.

```c
int ppf_precompute_constant(
    ppf_constant_id_t id,
    ppf_prec_t precision,
    unsigned long workers
);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Allocation and failure.** Unrecovered project-allocation failure releases the incomplete interval without publishing it to the constant cache; an existing cached interval remains valid; GMP allocation is outside this recoverable allocation contract.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_precompute_euler

Precompute and cache euler to a requested precision.

```c
int ppf_precompute_euler(ppf_prec_t precision, unsigned long workers);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Allocation and failure.** Unrecovered project-allocation failure releases the incomplete interval without publishing it to the constant cache; an existing cached interval remains valid; GMP allocation is outside this recoverable allocation contract.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_precompute_log2

Precompute and cache log(2) to a requested precision.

```c
int ppf_precompute_log2(ppf_prec_t precision, unsigned long workers);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Allocation and failure.** Unrecovered project-allocation failure releases the incomplete interval without publishing it to the constant cache; an existing cached interval remains valid; GMP allocation is outside this recoverable allocation contract.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_precompute_pi

Precompute and cache pi to a requested precision.

```c
int ppf_precompute_pi(ppf_prec_t precision, unsigned long workers);
```

**Result.** A status code. Check the operation's success convention; this is not a numerical ternary. Failure indicator: `function-specific nonzero failure status`.

**Allocation and failure.** Unrecovered project-allocation failure releases the incomplete interval without publishing it to the constant cache; an existing cached interval remains valid; GMP allocation is outside this recoverable allocation contract.

**Resource policy.** constant certification uses at most 16 precision-escalation attempts per request. Consult the numerical-limits appendix for the complete cap inventory.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

### ppf_set_constant_threads

Set the default worker request for constant construction.

```c
void ppf_set_constant_threads(unsigned long workers);
```

**Result.** No return value.

**Concurrency.** The library synchronizes its global state. Caller-owned mutable objects still require exclusive access.

## Certified interval assignment

### ppf_set_dyadic_interval

Round a certified dyadic interval into a destination, reporting decided, undecided, or error.

```c
ppf_interval_status_t ppf_set_dyadic_interval(
    ppf_t *rop,
    mpz_srcptr lower,
    mpz_srcptr upper,
    ppf_exp_t exponent,
    ppf_rnd_t rnd,
    int *ternary,
    ppf_context_t *ctx
);
```

**Input contract.** lower and upper are exact integers with lower <= upper and share the supplied binary exponent.

**Result.** `PPF_INTERVAL_DECIDED`, `PPF_INTERVAL_UNDECIDED`, or `PPF_INTERVAL_ERROR`. The numerical ternary is a separate output.

**Context and rounding.** A null `ctx` selects the calling thread's default context. PPF_RND_DEFAULT resolves through the supplied context or the calling thread default context.

**Allocation and failure.** An unrecovered project-allocation failure preserves the prior numerical output. Sticky flags may change, and an exact fallback may succeed while retaining the allocation flag. Numerical rejection is a separate case and may commit NaN.

## Core comparison, validation, and diagnostics

### ppf_cmp

Compare two Polyphase values.

```c
int ppf_cmp(const ppf_t *a, const ppf_t *b, ppf_context_t *ctx);
```

**Result.** An ordering result. NaNs and unordered comparisons require the operation's documented handling.

**Context and rounding.** A null `ctx` selects the calling thread's default context.

### ppf_dump

Write an internal diagnostic representation to standard output.

```c
void ppf_dump(const ppf_t *x);
```

**Result.** No return value.

### ppf_dump_file

Write an internal diagnostic representation to a selected stream.

```c
void ppf_dump_file(FILE *stream, const ppf_t *x);
```

**Result.** No return value.

### ppf_equal_p

Test exact equality of represented values.

```c
int ppf_equal_p(const ppf_t *a, const ppf_t *b);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

### ppf_kind_name

Return a readable name for a value kind.

```c
const char *ppf_kind_name(ppf_kind_t kind);
```

**Result.** A borrowed pointer. Do not free the returned storage. Failure indicator: `NULL where documented`.

**Ownership.** returned string is library-owned static storage.

### ppf_rounding_name

Return a readable name for a rounding mode.

```c
const char *ppf_rounding_name(ppf_rnd_t rnd);
```

**Result.** Returns a static name for the supplied enumeration value, including `PPF_RND_DEFAULT`. An invalid enumeration value returns the static string `PPF_RND_INVALID`; this function does not return NULL.

**Context and rounding.** the enumeration is inspected without selecting or resolving a rounding policy; PPF_RND_DEFAULT has its own literal name.

**Ownership.** returned string is library-owned static storage.

### ppf_validate

Check internal representation invariants.

```c
int ppf_validate(const ppf_t *x);
```

**Result.** A predicate result: zero means false and a nonzero value means true.

# Alphabetical function index

Every entry below links to its core API reference.

## A

| Function | Function |
|---|---|
| [`ppf_abi_contract_sha256`](#ppf_abi_contract_sha256) | [`ppf_allocflag_p`](#ppf_allocflag_p) |
| [`ppf_abi_epoch`](#ppf_abi_epoch) | [`ppf_arena_alloc`](#ppf_arena_alloc) |
| [`ppf_abi_info_size`](#ppf_abi_info_size) | [`ppf_arena_mark`](#ppf_arena_mark) |
| [`ppf_abi_revision`](#ppf_abi_revision) | [`ppf_arena_release`](#ppf_arena_release) |
| [`ppf_abi_tag`](#ppf_abi_tag) | [`ppf_arena_reset`](#ppf_arena_reset) |
| [`ppf_abs`](#ppf_abs) | [`ppf_asin`](#ppf_asin) |
| [`ppf_acos`](#ppf_acos) | [`ppf_asinh`](#ppf_asinh) |
| [`ppf_acosh`](#ppf_acosh) | [`ppf_asinpi`](#ppf_asinpi) |
| [`ppf_acospi`](#ppf_acospi) | [`ppf_asinu`](#ppf_asinu) |
| [`ppf_acosu`](#ppf_acosu) | [`ppf_asprintf`](#ppf_asprintf) |
| [`ppf_add`](#ppf_add) | [`ppf_atan`](#ppf_atan) |
| [`ppf_add_d`](#ppf_add_d) | [`ppf_atan2`](#ppf_atan2) |
| [`ppf_add_q`](#ppf_add_q) | [`ppf_atan2pi`](#ppf_atan2pi) |
| [`ppf_add_si`](#ppf_add_si) | [`ppf_atan2u`](#ppf_atan2u) |
| [`ppf_add_ui`](#ppf_add_ui) | [`ppf_atanh`](#ppf_atanh) |
| [`ppf_add_z`](#ppf_add_z) | [`ppf_atanpi`](#ppf_atanpi) |
| [`ppf_agm`](#ppf_agm) | [`ppf_atanu`](#ppf_atanu) |
| [`ppf_ai`](#ppf_ai) |  |

## B

| Function | Function |
|---|---|
| [`ppf_beta`](#ppf_beta) | [`ppf_buildopt_sharedcache_p`](#ppf_buildopt_sharedcache_p) |
| [`ppf_buildopt_decimal_p`](#ppf_buildopt_decimal_p) | [`ppf_buildopt_thread_cleanup_p`](#ppf_buildopt_thread_cleanup_p) |
| [`ppf_buildopt_float128_p`](#ppf_buildopt_float128_p) | [`ppf_buildopt_tls_p`](#ppf_buildopt_tls_p) |
| [`ppf_buildopt_gmpinternals_p`](#ppf_buildopt_gmpinternals_p) | [`ppf_buildopt_tune_case`](#ppf_buildopt_tune_case) |

## C

| Function | Function |
|---|---|
| [`ppf_can_round`](#ppf_can_round) | [`ppf_const_log2`](#ppf_const_log2) |
| [`ppf_cbrt`](#ppf_cbrt) | [`ppf_const_log2_threads`](#ppf_const_log2_threads) |
| [`ppf_ceil`](#ppf_ceil) | [`ppf_const_log2_uncached`](#ppf_const_log2_uncached) |
| [`ppf_check_range`](#ppf_check_range) | [`ppf_const_pi`](#ppf_const_pi) |
| [`ppf_clear`](#ppf_clear) | [`ppf_const_pi_threads`](#ppf_const_pi_threads) |
| [`ppf_clear_allocflag`](#ppf_clear_allocflag) | [`ppf_const_pi_uncached`](#ppf_const_pi_uncached) |
| [`ppf_clear_constant_cache`](#ppf_clear_constant_cache) | [`ppf_constant_cache_precision`](#ppf_constant_cache_precision) |
| [`ppf_clear_divby0`](#ppf_clear_divby0) | [`ppf_context_clear`](#ppf_context_clear) |
| [`ppf_clear_erangeflag`](#ppf_clear_erangeflag) | [`ppf_context_clone`](#ppf_context_clone) |
| [`ppf_clear_flags`](#ppf_clear_flags) | [`ppf_context_flags`](#ppf_context_flags) |
| [`ppf_clear_inexflag`](#ppf_clear_inexflag) | [`ppf_context_flags_clear`](#ppf_context_flags_clear) |
| [`ppf_clear_nanflag`](#ppf_clear_nanflag) | [`ppf_context_flags_restore`](#ppf_context_flags_restore) |
| [`ppf_clear_overflow`](#ppf_clear_overflow) | [`ppf_context_flags_save`](#ppf_context_flags_save) |
| [`ppf_clear_underflow`](#ppf_clear_underflow) | [`ppf_context_flags_set`](#ppf_context_flags_set) |
| [`ppf_clears`](#ppf_clears) | [`ppf_context_init`](#ppf_context_init) |
| [`ppf_cmp`](#ppf_cmp) | [`ppf_context_init_custom`](#ppf_context_init_custom) |
| [`ppf_cmp_d`](#ppf_cmp_d) | [`ppf_context_set_default_precision`](#ppf_context_set_default_precision) |
| [`ppf_cmp_f`](#ppf_cmp_f) | [`ppf_context_set_range`](#ppf_context_set_range) |
| [`ppf_cmp_ld`](#ppf_cmp_ld) | [`ppf_context_set_rounding`](#ppf_context_set_rounding) |
| [`ppf_cmp_q`](#ppf_cmp_q) | [`ppf_copysign`](#ppf_copysign) |
| [`ppf_cmp_si`](#ppf_cmp_si) | [`ppf_cos`](#ppf_cos) |
| [`ppf_cmp_si_2exp`](#ppf_cmp_si_2exp) | [`ppf_cosh`](#ppf_cosh) |
| [`ppf_cmp_ui`](#ppf_cmp_ui) | [`ppf_cospi`](#ppf_cospi) |
| [`ppf_cmp_ui_2exp`](#ppf_cmp_ui_2exp) | [`ppf_cosu`](#ppf_cosu) |
| [`ppf_cmp_z`](#ppf_cmp_z) | [`ppf_cot`](#ppf_cot) |
| [`ppf_cmpabs`](#ppf_cmpabs) | [`ppf_coth`](#ppf_coth) |
| [`ppf_cmpabs_ui`](#ppf_cmpabs_ui) | [`ppf_csc`](#ppf_csc) |
| [`ppf_commit_regular`](#ppf_commit_regular) | [`ppf_csch`](#ppf_csch) |
| [`ppf_compound_si`](#ppf_compound_si) | [`ppf_custom_get_exp`](#ppf_custom_get_exp) |
| [`ppf_const_catalan`](#ppf_const_catalan) | [`ppf_custom_get_kind`](#ppf_custom_get_kind) |
| [`ppf_const_catalan_threads`](#ppf_const_catalan_threads) | [`ppf_custom_get_significand`](#ppf_custom_get_significand) |
| [`ppf_const_catalan_uncached`](#ppf_const_catalan_uncached) | [`ppf_custom_get_size`](#ppf_custom_get_size) |
| [`ppf_const_euler`](#ppf_const_euler) | [`ppf_custom_init`](#ppf_custom_init) |
| [`ppf_const_euler_threads`](#ppf_const_euler_threads) | [`ppf_custom_init_set`](#ppf_custom_init_set) |
| [`ppf_const_euler_uncached`](#ppf_const_euler_uncached) | [`ppf_custom_move`](#ppf_custom_move) |

## D

| Function | Function |
|---|---|
| [`ppf_d_div`](#ppf_d_div) | [`ppf_div_ui`](#ppf_div_ui) |
| [`ppf_d_sub`](#ppf_d_sub) | [`ppf_div_with_plan`](#ppf_div_with_plan) |
| [`ppf_default_allocator`](#ppf_default_allocator) | [`ppf_div_with_plan_many`](#ppf_div_with_plan_many) |
| [`ppf_default_context`](#ppf_default_context) | [`ppf_div_z`](#ppf_div_z) |
| [`ppf_default_context_cleanup`](#ppf_default_context_cleanup) | [`ppf_divby0_p`](#ppf_divby0_p) |
| [`ppf_digamma`](#ppf_digamma) | [`ppf_divisor_plan_applications`](#ppf_divisor_plan_applications) |
| [`ppf_dim`](#ppf_dim) | [`ppf_divisor_plan_create`](#ppf_divisor_plan_create) |
| [`ppf_div`](#ppf_div) | [`ppf_divisor_plan_create_ex`](#ppf_divisor_plan_create_ex) |
| [`ppf_div_2exp`](#ppf_div_2exp) | [`ppf_divisor_plan_destroy`](#ppf_divisor_plan_destroy) |
| [`ppf_div_2si`](#ppf_div_2si) | [`ppf_divisor_plan_precision`](#ppf_divisor_plan_precision) |
| [`ppf_div_2ui`](#ppf_div_2ui) | [`ppf_dot`](#ppf_dot) |
| [`ppf_div_d`](#ppf_div_d) | [`ppf_dot_threads`](#ppf_dot_threads) |
| [`ppf_div_q`](#ppf_div_q) | [`ppf_dump`](#ppf_dump) |
| [`ppf_div_si`](#ppf_div_si) | [`ppf_dump_file`](#ppf_dump_file) |

## E

| Function | Function |
|---|---|
| [`ppf_eint`](#ppf_eint) | [`ppf_exp`](#ppf_exp) |
| [`ppf_eq`](#ppf_eq) | [`ppf_exp10`](#ppf_exp10) |
| [`ppf_equal_p`](#ppf_equal_p) | [`ppf_exp10m1`](#ppf_exp10m1) |
| [`ppf_erandom`](#ppf_erandom) | [`ppf_exp2`](#ppf_exp2) |
| [`ppf_erangeflag_p`](#ppf_erangeflag_p) | [`ppf_exp2m1`](#ppf_exp2m1) |
| [`ppf_erf`](#ppf_erf) | [`ppf_expm1`](#ppf_expm1) |
| [`ppf_erfc`](#ppf_erfc) |  |

## F

| Function | Function |
|---|---|
| [`ppf_fac_ui`](#ppf_fac_ui) | [`ppf_fmma`](#ppf_fmma) |
| [`ppf_fits_intmax_p`](#ppf_fits_intmax_p) | [`ppf_fmms`](#ppf_fmms) |
| [`ppf_fits_sint_p`](#ppf_fits_sint_p) | [`ppf_fmod`](#ppf_fmod) |
| [`ppf_fits_slong_p`](#ppf_fits_slong_p) | [`ppf_fmod_ui`](#ppf_fmod_ui) |
| [`ppf_fits_sshort_p`](#ppf_fits_sshort_p) | [`ppf_fmodquo`](#ppf_fmodquo) |
| [`ppf_fits_uint_p`](#ppf_fits_uint_p) | [`ppf_fms`](#ppf_fms) |
| [`ppf_fits_uintmax_p`](#ppf_fits_uintmax_p) | [`ppf_fms_algorithm_threads`](#ppf_fms_algorithm_threads) |
| [`ppf_fits_ulong_p`](#ppf_fits_ulong_p) | [`ppf_fms_threads`](#ppf_fms_threads) |
| [`ppf_fits_ushort_p`](#ppf_fits_ushort_p) | [`ppf_fpif_export`](#ppf_fpif_export) |
| [`ppf_flags_clear`](#ppf_flags_clear) | [`ppf_fpif_import`](#ppf_fpif_import) |
| [`ppf_flags_restore`](#ppf_flags_restore) | [`ppf_fprintf`](#ppf_fprintf) |
| [`ppf_flags_save`](#ppf_flags_save) | [`ppf_frac`](#ppf_frac) |
| [`ppf_flags_set`](#ppf_flags_set) | [`ppf_free_cache`](#ppf_free_cache) |
| [`ppf_flags_test`](#ppf_flags_test) | [`ppf_free_cache2`](#ppf_free_cache2) |
| [`ppf_floor`](#ppf_floor) | [`ppf_free_constant_caches`](#ppf_free_constant_caches) |
| [`ppf_fma`](#ppf_fma) | [`ppf_free_pool`](#ppf_free_pool) |
| [`ppf_fma_algorithm_threads`](#ppf_fma_algorithm_threads) | [`ppf_free_str`](#ppf_free_str) |
| [`ppf_fma_threads`](#ppf_fma_threads) | [`ppf_frexp`](#ppf_frexp) |

## G

| Function | Function |
|---|---|
| [`ppf_gamma`](#ppf_gamma) | [`ppf_get_flt`](#ppf_get_flt) |
| [`ppf_gamma_inc`](#ppf_gamma_inc) | [`ppf_get_last_constant_stats`](#ppf_get_last_constant_stats) |
| [`ppf_get_abi_info`](#ppf_get_abi_info) | [`ppf_get_ld`](#ppf_get_ld) |
| [`ppf_get_constant_interval`](#ppf_get_constant_interval) | [`ppf_get_ld_2exp`](#ppf_get_ld_2exp) |
| [`ppf_get_constant_threads`](#ppf_get_constant_threads) | [`ppf_get_patches`](#ppf_get_patches) |
| [`ppf_get_d`](#ppf_get_d) | [`ppf_get_prec`](#ppf_get_prec) |
| [`ppf_get_d_2exp`](#ppf_get_d_2exp) | [`ppf_get_q`](#ppf_get_q) |
| [`ppf_get_decimal128`](#ppf_get_decimal128) | [`ppf_get_si`](#ppf_get_si) |
| [`ppf_get_decimal64`](#ppf_get_decimal64) | [`ppf_get_sj`](#ppf_get_sj) |
| [`ppf_get_default_prec`](#ppf_get_default_prec) | [`ppf_get_str`](#ppf_get_str) |
| [`ppf_get_default_rounding_mode`](#ppf_get_default_rounding_mode) | [`ppf_get_str_ndigits`](#ppf_get_str_ndigits) |
| [`ppf_get_emax`](#ppf_get_emax) | [`ppf_get_trig_threads`](#ppf_get_trig_threads) |
| [`ppf_get_emax_max`](#ppf_get_emax_max) | [`ppf_get_ui`](#ppf_get_ui) |
| [`ppf_get_emax_min`](#ppf_get_emax_min) | [`ppf_get_uj`](#ppf_get_uj) |
| [`ppf_get_emin`](#ppf_get_emin) | [`ppf_get_version`](#ppf_get_version) |
| [`ppf_get_emin_max`](#ppf_get_emin_max) | [`ppf_get_z`](#ppf_get_z) |
| [`ppf_get_emin_min`](#ppf_get_emin_min) | [`ppf_get_z_2exp`](#ppf_get_z_2exp) |
| [`ppf_get_exp`](#ppf_get_exp) | [`ppf_grandom`](#ppf_grandom) |
| [`ppf_get_f`](#ppf_get_f) | [`ppf_greater_p`](#ppf_greater_p) |
| [`ppf_get_float128`](#ppf_get_float128) | [`ppf_greaterequal_p`](#ppf_greaterequal_p) |

## H

| Function | Function |
|---|---|
| [`ppf_hypot`](#ppf_hypot) |  |

## I

| Function | Function |
|---|---|
| [`ppf_inexflag_p`](#ppf_inexflag_p) | [`ppf_init_set_si`](#ppf_init_set_si) |
| [`ppf_inf_p`](#ppf_inf_p) | [`ppf_init_set_sj`](#ppf_init_set_sj) |
| [`ppf_init`](#ppf_init) | [`ppf_init_set_str`](#ppf_init_set_str) |
| [`ppf_init2`](#ppf_init2) | [`ppf_init_set_ui`](#ppf_init_set_ui) |
| [`ppf_init2_ctx`](#ppf_init2_ctx) | [`ppf_init_set_uj`](#ppf_init_set_uj) |
| [`ppf_init_set`](#ppf_init_set) | [`ppf_init_set_z`](#ppf_init_set_z) |
| [`ppf_init_set_d`](#ppf_init_set_d) | [`ppf_inits`](#ppf_inits) |
| [`ppf_init_set_f`](#ppf_init_set_f) | [`ppf_inits2`](#ppf_inits2) |
| [`ppf_init_set_flt`](#ppf_init_set_flt) | [`ppf_inp_str`](#ppf_inp_str) |
| [`ppf_init_set_ld`](#ppf_init_set_ld) | [`ppf_integer_p`](#ppf_integer_p) |
| [`ppf_init_set_q`](#ppf_init_set_q) |  |

## J

| Function | Function |
|---|---|
| [`ppf_j0`](#ppf_j0) | [`ppf_jn`](#ppf_jn) |
| [`ppf_j1`](#ppf_j1) |  |

## K

| Function | Function |
|---|---|
| [`ppf_kind_name`](#ppf_kind_name) |  |

## L

| Function | Function |
|---|---|
| [`ppf_less_p`](#ppf_less_p) | [`ppf_lngamma`](#ppf_lngamma) |
| [`ppf_lessequal_p`](#ppf_lessequal_p) | [`ppf_log`](#ppf_log) |
| [`ppf_lessgreater_p`](#ppf_lessgreater_p) | [`ppf_log10`](#ppf_log10) |
| [`ppf_lgamma`](#ppf_lgamma) | [`ppf_log10p1`](#ppf_log10p1) |
| [`ppf_li2`](#ppf_li2) | [`ppf_log1p`](#ppf_log1p) |
| [`ppf_limb_count`](#ppf_limb_count) | [`ppf_log2`](#ppf_log2) |
| [`ppf_limbs_read`](#ppf_limbs_read) | [`ppf_log2p1`](#ppf_log2p1) |
| [`ppf_limbs_write`](#ppf_limbs_write) | [`ppf_log_ui`](#ppf_log_ui) |

## M

| Function | Function |
|---|---|
| [`ppf_max`](#ppf_max) | [`ppf_mul_2ui`](#ppf_mul_2ui) |
| [`ppf_min`](#ppf_min) | [`ppf_mul_algorithm_threads`](#ppf_mul_algorithm_threads) |
| [`ppf_min_prec`](#ppf_min_prec) | [`ppf_mul_d`](#ppf_mul_d) |
| [`ppf_modf`](#ppf_modf) | [`ppf_mul_q`](#ppf_mul_q) |
| [`ppf_mp_memory_cleanup`](#ppf_mp_memory_cleanup) | [`ppf_mul_si`](#ppf_mul_si) |
| [`ppf_mul`](#ppf_mul) | [`ppf_mul_threads`](#ppf_mul_threads) |
| [`ppf_mul_2exp`](#ppf_mul_2exp) | [`ppf_mul_ui`](#ppf_mul_ui) |
| [`ppf_mul_2si`](#ppf_mul_2si) | [`ppf_mul_z`](#ppf_mul_z) |

## N

| Function | Function |
|---|---|
| [`ppf_nan_p`](#ppf_nan_p) | [`ppf_nextbelow`](#ppf_nextbelow) |
| [`ppf_nanflag_p`](#ppf_nanflag_p) | [`ppf_nexttoward`](#ppf_nexttoward) |
| [`ppf_neg`](#ppf_neg) | [`ppf_nrandom`](#ppf_nrandom) |
| [`ppf_nextabove`](#ppf_nextabove) | [`ppf_number_p`](#ppf_number_p) |

## O

| Function | Function |
|---|---|
| [`ppf_out_str`](#ppf_out_str) | [`ppf_overflow_p`](#ppf_overflow_p) |

## P

| Function | Function |
|---|---|
| [`ppf_plan_product`](#ppf_plan_product) | [`ppf_prec_round`](#ppf_prec_round) |
| [`ppf_plan_product_algorithm`](#ppf_plan_product_algorithm) | [`ppf_precompute_catalan`](#ppf_precompute_catalan) |
| [`ppf_pow`](#ppf_pow) | [`ppf_precompute_constant`](#ppf_precompute_constant) |
| [`ppf_pow_si`](#ppf_pow_si) | [`ppf_precompute_euler`](#ppf_precompute_euler) |
| [`ppf_pow_sj`](#ppf_pow_sj) | [`ppf_precompute_log2`](#ppf_precompute_log2) |
| [`ppf_pow_ui`](#ppf_pow_ui) | [`ppf_precompute_pi`](#ppf_precompute_pi) |
| [`ppf_pow_uj`](#ppf_pow_uj) | [`ppf_print_rnd_mode`](#ppf_print_rnd_mode) |
| [`ppf_pow_z`](#ppf_pow_z) | [`ppf_printf`](#ppf_printf) |
| [`ppf_pown`](#ppf_pown) | [`ppf_product_profile_name`](#ppf_product_profile_name) |
| [`ppf_powr`](#ppf_powr) | [`ppf_product_profile_sha256`](#ppf_product_profile_sha256) |

## R

| Function | Function |
|---|---|
| [`ppf_rec_sqrt`](#ppf_rec_sqrt) | [`ppf_rint_round`](#ppf_rint_round) |
| [`ppf_rec_sqrt_algorithm_threads`](#ppf_rec_sqrt_algorithm_threads) | [`ppf_rint_roundeven`](#ppf_rint_roundeven) |
| [`ppf_rec_sqrt_threads`](#ppf_rec_sqrt_threads) | [`ppf_rint_trunc`](#ppf_rint_trunc) |
| [`ppf_regular_p`](#ppf_regular_p) | [`ppf_root`](#ppf_root) |
| [`ppf_reldiff`](#ppf_reldiff) | [`ppf_rootn_si`](#ppf_rootn_si) |
| [`ppf_remainder`](#ppf_remainder) | [`ppf_rootn_ui`](#ppf_rootn_ui) |
| [`ppf_remquo`](#ppf_remquo) | [`ppf_round`](#ppf_round) |
| [`ppf_reserve`](#ppf_reserve) | [`ppf_round_nearest_away_finish`](#ppf_round_nearest_away_finish) |
| [`ppf_rint`](#ppf_rint) | [`ppf_roundeven`](#ppf_roundeven) |
| [`ppf_rint_ceil`](#ppf_rint_ceil) | [`ppf_rounding_name`](#ppf_rounding_name) |
| [`ppf_rint_floor`](#ppf_rint_floor) |  |

## S

| Function | Function |
|---|---|
| [`ppf_sec`](#ppf_sec) | [`ppf_set_ui_2exp`](#ppf_set_ui_2exp) |
| [`ppf_sech`](#ppf_sech) | [`ppf_set_uj`](#ppf_set_uj) |
| [`ppf_set`](#ppf_set) | [`ppf_set_uj_2exp`](#ppf_set_uj_2exp) |
| [`ppf_set_allocflag`](#ppf_set_allocflag) | [`ppf_set_underflow`](#ppf_set_underflow) |
| [`ppf_set_constant_threads`](#ppf_set_constant_threads) | [`ppf_set_z`](#ppf_set_z) |
| [`ppf_set_d`](#ppf_set_d) | [`ppf_set_z_2exp`](#ppf_set_z_2exp) |
| [`ppf_set_decimal128`](#ppf_set_decimal128) | [`ppf_set_zero`](#ppf_set_zero) |
| [`ppf_set_decimal64`](#ppf_set_decimal64) | [`ppf_setsign`](#ppf_setsign) |
| [`ppf_set_default_prec`](#ppf_set_default_prec) | [`ppf_sgn`](#ppf_sgn) |
| [`ppf_set_default_rounding_mode`](#ppf_set_default_rounding_mode) | [`ppf_si_div`](#ppf_si_div) |
| [`ppf_set_divby0`](#ppf_set_divby0) | [`ppf_si_sub`](#ppf_si_sub) |
| [`ppf_set_dyadic_interval`](#ppf_set_dyadic_interval) | [`ppf_signbit`](#ppf_signbit) |
| [`ppf_set_emax`](#ppf_set_emax) | [`ppf_sin`](#ppf_sin) |
| [`ppf_set_emin`](#ppf_set_emin) | [`ppf_sin_cos`](#ppf_sin_cos) |
| [`ppf_set_erangeflag`](#ppf_set_erangeflag) | [`ppf_sinh`](#ppf_sinh) |
| [`ppf_set_exp`](#ppf_set_exp) | [`ppf_sinh_cosh`](#ppf_sinh_cosh) |
| [`ppf_set_f`](#ppf_set_f) | [`ppf_sinpi`](#ppf_sinpi) |
| [`ppf_set_float128`](#ppf_set_float128) | [`ppf_sinu`](#ppf_sinu) |
| [`ppf_set_flt`](#ppf_set_flt) | [`ppf_snprintf`](#ppf_snprintf) |
| [`ppf_set_inexflag`](#ppf_set_inexflag) | [`ppf_sprintf`](#ppf_sprintf) |
| [`ppf_set_inf`](#ppf_set_inf) | [`ppf_sqr`](#ppf_sqr) |
| [`ppf_set_ld`](#ppf_set_ld) | [`ppf_sqr_algorithm_threads`](#ppf_sqr_algorithm_threads) |
| [`ppf_set_nan`](#ppf_set_nan) | [`ppf_sqr_threads`](#ppf_sqr_threads) |
| [`ppf_set_nanflag`](#ppf_set_nanflag) | [`ppf_sqrt`](#ppf_sqrt) |
| [`ppf_set_overflow`](#ppf_set_overflow) | [`ppf_sqrt_ui`](#ppf_sqrt_ui) |
| [`ppf_set_prec`](#ppf_set_prec) | [`ppf_strtofr`](#ppf_strtofr) |
| [`ppf_set_prec_raw`](#ppf_set_prec_raw) | [`ppf_sub`](#ppf_sub) |
| [`ppf_set_q`](#ppf_set_q) | [`ppf_sub_d`](#ppf_sub_d) |
| [`ppf_set_si`](#ppf_set_si) | [`ppf_sub_q`](#ppf_sub_q) |
| [`ppf_set_si_2exp`](#ppf_set_si_2exp) | [`ppf_sub_si`](#ppf_sub_si) |
| [`ppf_set_sj`](#ppf_set_sj) | [`ppf_sub_ui`](#ppf_sub_ui) |
| [`ppf_set_sj_2exp`](#ppf_set_sj_2exp) | [`ppf_sub_z`](#ppf_sub_z) |
| [`ppf_set_str`](#ppf_set_str) | [`ppf_subnormalize`](#ppf_subnormalize) |
| [`ppf_set_trig_threads`](#ppf_set_trig_threads) | [`ppf_sum`](#ppf_sum) |
| [`ppf_set_ui`](#ppf_set_ui) | [`ppf_swap`](#ppf_swap) |

## T

| Function | Function |
|---|---|
| [`ppf_tan`](#ppf_tan) | [`ppf_thread_cleanup`](#ppf_thread_cleanup) |
| [`ppf_tanh`](#ppf_tanh) | [`ppf_total_order_p`](#ppf_total_order_p) |
| [`ppf_tanpi`](#ppf_tanpi) | [`ppf_trunc`](#ppf_trunc) |
| [`ppf_tanu`](#ppf_tanu) |  |

## U

| Function | Function |
|---|---|
| [`ppf_ui_div`](#ppf_ui_div) | [`ppf_underflow_p`](#ppf_underflow_p) |
| [`ppf_ui_pow`](#ppf_ui_pow) | [`ppf_unordered_p`](#ppf_unordered_p) |
| [`ppf_ui_pow_ui`](#ppf_ui_pow_ui) | [`ppf_urandom`](#ppf_urandom) |
| [`ppf_ui_sub`](#ppf_ui_sub) | [`ppf_urandomb`](#ppf_urandomb) |

## V

| Function | Function |
|---|---|
| [`ppf_validate`](#ppf_validate) | [`ppf_vprintf`](#ppf_vprintf) |
| [`ppf_vasprintf`](#ppf_vasprintf) | [`ppf_vsnprintf`](#ppf_vsnprintf) |
| [`ppf_vfprintf`](#ppf_vfprintf) | [`ppf_vsprintf`](#ppf_vsprintf) |

## Y

| Function | Function |
|---|---|
| [`ppf_y0`](#ppf_y0) | [`ppf_yn`](#ppf_yn) |
| [`ppf_y1`](#ppf_y1) |  |

## Z

| Function | Function |
|---|---|
| [`ppf_z_sub`](#ppf_z_sub) | [`ppf_zeta`](#ppf_zeta) |
| [`ppf_zero_p`](#ppf_zero_p) | [`ppf_zeta_ui`](#ppf_zeta_ui) |
