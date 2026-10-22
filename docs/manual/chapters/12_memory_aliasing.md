# Memory ownership and in-place operations

A `ppf_t` owns or refers to significand storage; a context owns mutable
execution policy and reusable scratch space. Treat both as managed C objects
with explicit lifetimes. The most reliable application structure initializes
them once, checks each fallible call, and releases them through one cleanup
path. That structure is also the foundation for safe in-place operations and
for recovery from allocation failure.

## Establish ownership before computing

For local numbers, zero-initialize the structure before a sequence of
initialization calls. This allows a common cleanup path to call `ppf_clear`
on objects whose initialization was never attempted. Successfully initialize
the context before using it to initialize numbers. A failure to allocate a
significand during valid `ppf_init2_ctx` initialization leaves a safe-zero
object; it does not leave a usable numerical value.

```c
ppf_context_t ctx;
ppf_t x = {0}, y = {0};

if (ppf_context_init(&ctx) != 0) return 1;
if (ppf_init2_ctx(&x, 128, &ctx) != 0 ||
    ppf_init2_ctx(&y, 128, &ctx) != 0) {
    ppf_clear(&y);
    ppf_clear(&x);
    ppf_context_clear(&ctx);
    return 1;
}
/* Assign values and perform checked calculations here. */
ppf_clear(&y);
ppf_clear(&x);
ppf_context_clear(&ctx);
```

Do not initialize an already live object again to change its precision. That
can abandon its existing allocation. `ppf_set_prec` changes precision but
discards the old value on success, leaving NaN. Use `ppf_prec_round` when
retaining and rounding the old value is the intention. `ppf_reserve` reserves
capacity; it is not itself a request to
change the numerical precision or value.

Do not copy a live `ppf_t` with C structure assignment or `memcpy`. That copies
the storage pointer and ownership metadata, so two apparent objects can refer
to one allocation and later free it twice. Use `ppf_set` to assign a value into
an initialized destination, and `ppf_swap` when ownership of two complete
objects should be exchanged. Similarly, use `ppf_context_clone` to create a
context with copied policy and its own initially empty scratch state; a raw
copy can duplicate ownership of arena blocks.

## Supplying an allocator

`ppf_context_init_custom` accepts a `ppf_allocator_t` containing allocation,
reallocation, and release callbacks plus an opaque pointer. Supply all three
callbacks. The reallocation callback receives old and new byte sizes; it must
leave the old block valid when it returns null. The release callback receives
the size associated with that allocation. Respect these sizes when integrating
a pool, accounting system, or debugging allocator.

Objects retain the allocator needed to release their own significands. An
allocated `ppf_get_str` result likewise retains allocator provenance. This
does not extend the lifetime of the opaque application state. Keep that state,
and any supporting pool or callback code, alive until every dependent number,
string, arena, and other owned resource has been released. Clearing the
context is not a substitute for clearing the numbers created through it.

The complete
[systems_allocation.c](../../../examples/manual/systems_allocation.c) example
uses a small allocator wrapper that can reject its next request. It first
stores `37`, forces the temporary-object allocation in `ppf_set_str` to fail,
and checks that `37` remains stored. After clearing the allocation flag, a
retry reads `1.25` successfully. The program also checks that all blocks
allocated through the wrapper have been released. This is an example of the
project allocator contract, not a simulation of every possible system-wide
out-of-memory condition.

If several contexts share one allocator and opaque pointer, the allocator's
own synchronization must support the way those contexts are used. Separate
contexts do not make a shared allocation counter or pool thread-safe.

## What failure preserves

The library distinguishes initialization, assignment, and temporary resource
construction. Their failure outcomes are useful only when the destination was
valid and the function's preconditions were met.

| Operation class | Useful failure rule |
|---|---|
| Significand allocation during `ppf_init2_ctx` | The object is safe to clear; there is no previous value to preserve |
| Covered numerical transaction with an initialized destination | An unrecovered project-allocation failure preserves that destination |
| `ppf_reserve` or `ppf_set_prec` reallocation | Failure preserves the original object and allocation |
| Initializer followed by assignment | A later setter failure may leave an initialized object that still needs clearing |
| Stream input | The number can be preserved although input bytes were consumed |
| Stream output | Staging can fail before writing; an I/O failure after writing starts can leave a prefix |

`PPF_FLAG_ALLOC` is sticky. It says that a project-controlled allocation was
refused; it does not by itself say that the operation failed. Some operations
recover through an exact fallback and return a normal result with the flag
still set. Check the function's result first, then interpret the flag according
to the application's resource policy. For an isolated measurement, clear the
flag on the same context before the operation. The short convenience flag
functions inspect the thread-default context, which may be different from an
explicit context passed to the calculation.

The transactional rule covers the specified project-allocation paths. It is
not a general promise to preserve the old value for every numerical error.
Domain errors, unresolved numerical certification, invalid arguments, and
ordinary overflow have their own documented outcomes. Nor does it promise
rollback of all externally visible state: flags can change, input streams can
advance, and formatting `%n` conversions can already have written a count.

GMP allocations form a separate domain from `ppf_context_t`. A custom
Polyphase allocator does not intercept every temporary integer or rational
allocated by GMP. C-library stream buffering, some process allocations, and
operating-system resources are separate too. Plan application memory limits
with those domains in mind. Do not replace GMP's process-level allocation
functions casually inside an application that already has live GMP objects or
other GMP users.

## In-place arithmetic without accidental overlap

An in-place call such as `ppf_add(&x, &x, &y, ...)` means that one initialized
object appears in an allowed destination and input position. It does not mean
that unrelated objects may share arbitrary portions of their limb arrays.
Consult the function's aliasing classification when building a generic wrapper
or using an unfamiliar operation; similarity between function names is not a
storage contract.

Ordinary arithmetic families support their documented in-place cases. The
destination's precision still controls the rounded result, even when the
destination is also an input. If different precision is needed for the
result, create or prepare that destination explicitly rather than assuming an
input's precision will be inherited.

Multi-output operations need special attention. The two floating outputs of
`ppf_modf`, `ppf_sin_cos`, and `ppf_sinh_cosh` must be distinct, although either
may individually alias the input where the contract permits it.
`ppf_grandom` also requires distinct floating outputs when both are present;
its second output may instead be null. `ppf_get_constant_interval` requires
distinct lower and upper GMP integer objects. Two pointer variables pointing
to the same object are not distinct outputs.

For `ppf_div_with_plan_many`, a destination may coincide with its numerator
at the same index. Duplicate destinations and cross-index destination/input
overlap are rejected before the batch begins. This prevents a result written
early in the batch from silently replacing an operand needed later. Check
array relationships before submitting a batch, especially when arrays are
assembled from slices or views.

Lifecycle functions are not ordinary in-place assignments. An initializer
needs unused destination storage; initializing it from itself cannot preserve
a value that initialization destroys. Direct-storage functions likewise have
their own capacity, alignment, normalization, and overlap requirements.

## Scratch space and caller-owned limbs

The context arena is useful when an integration needs temporary storage with
a shared scope. Take a mark before allocations and release back to it after
the temporary work. Growing the arena uses linked blocks and does not move
earlier returned addresses. A failed growth request leaves the earlier block
chain and mark usable. Restoring a mark ends the lifetime of allocations made
after that mark; no saved pointer into those allocations may be used afterward.

`ppf_arena_reset` keeps reusable storage rather than necessarily returning
every byte to the operating system. `ppf_context_clear` releases the context's
arena. Do not use an arena mark as a permanent bookmark across context cleanup,
and do not mix marks from different contexts.

Advanced external-significand storage avoids ordinary ownership of limb
memory. Its backing allocation must have enough capacity and remain valid
while the object uses it. Clearing an externally backed object does not
transfer ownership of that backing allocation to the library. Growing,
relocating, or editing the buffer requires the appropriate direct-storage
procedure; bypassing it can break normalization and expose readers to
inconsistent data. Start with ordinary owned objects unless the integration
has a measured need for direct storage and can enforce these lifetimes.

At shutdown, release returned strings and user-owned numbers, destroy plans
and random states, clear explicit contexts, and then perform any desired
thread-local cache cleanup. That order keeps allocator state alive until its
dependents are gone and makes repeated computations easier to reason about.
