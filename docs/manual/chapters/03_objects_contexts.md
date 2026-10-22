# Numbers, contexts, and ownership

A `ppf_t` is an owned numerical object, not a plain scalar that you can copy with C assignment. A `ppf_context_t` carries the policy and temporary resources used by operations. Keeping their roles separate makes a program easier to reason about: numbers retain values, while contexts govern how calculations are performed and record what happened.

This chapter uses explicit contexts throughout. That makes the source of precision policy, range limits, flags, and allocations visible at each call site. The thread-local convenience interface is useful too, but mixing the two styles casually is a common source of misleading diagnostics.

## Start with a complete lifetime

A normal object passes through four stages: declare its structure, initialize its storage, assign or calculate a value, and clear it. Here is the pattern in a small scope:

```c
ppf_context_t ctx;
ppf_t x = {0};
int status = 1;

if (ppf_context_init(&ctx) != 0) return 1;
if (ppf_init2_ctx(&x, 128, &ctx) != 0) goto cleanup;
if (ppf_set_ui(&x, 7, PPF_RNDN, &ctx) == PPF_TERNARY_ERROR)
    goto cleanup;
status = 0;

cleanup:
ppf_clear(&x);
ppf_context_clear(&ctx);
return status;
```

This is a function-body pattern, not a complete translation unit. The [first-programs chapter](05_first_programs.md) includes the header, `main`, checked output, and thread cleanup in complete programs.

`ppf_t x = {0}` zero-initializes the C structure so a common cleanup path can safely clear it even if initialization did not complete. It does **not** create a numerical zero or a valid initialized number. Only after successful `ppf_init2_ctx` may ordinary numerical operations use the object.

Successful initialization gives the object its storage and precision and leaves its value as NaN. Assign the intended value before using it as an operand. To make a numerical zero, call `ppf_set_zero(&x, 1)` on an initialized object; the positive sign argument selects positive zero.

If allocating significand storage fails, `ppf_init2_ctx` returns failure and leaves an all-zero structure. Starting from `{0}` also handles early argument validation failures safely. Check the return before proceeding. A nonzero initialization status is not a rounding direction.

The `goto cleanup` statements provide one release path. They do not jump back into a partially initialized calculation. For a C program that owns several resources, this structure keeps an early failure from bypassing cleanup. Clear each object you successfully initialized, and avoid reinitializing an object whose previous storage is still live.

## Read an operation declaration

Consider the multiplication interface:

```c
int ppf_mul(ppf_t *rop, const ppf_t *a, const ppf_t *b,
            ppf_rnd_t rnd, ppf_context_t *ctx);
```

`rop` is the result operand, or destination. The `const` qualification on `a` and `b` marks them as input pointers; any permitted overlap with the destination follows the aliasing rules discussed below. `rnd` selects rounding, and `ctx` supplies execution policy and flags. The returned integer is a ternary result, whose interpretation is explained in the next chapter.

In a call such as `ppf_mul(&product, &a, &b, PPF_RNDN, &ctx)`, each `&` supplies an address. The function writes through the destination pointer. There is no C expression `product = a * b` for these objects: C's built-in multiplication operator does not know the library's ownership and rounding rules.

Library functions differ in return conventions. Read the declaration and reference entry instead of treating every `int` as the same kind of answer. Initialization returns a status; multiplication returns a ternary; comparison returns an ordering result; a predicate answers a yes/no question. They can all use the C type `int` while meaning different things.

## Precision belongs to the object

`ppf_init2_ctx(&x, 128, &ctx)` explicitly creates a 128-bit destination. Use `ppf_get_prec(&x)` to inspect that precision. The minimum supported precision is two bits; the maximum practical request also depends on storage and implementation limits. Choose a precision for the numerical work, rather than allocating the largest value the type could express.

Different precision-changing functions have deliberately different purposes:

| Function | Effect |
|---|---|
| `ppf_reserve` | Ensures storage capacity without changing the declared precision or numerical value. |
| `ppf_set_prec` | Changes the precision and discards the previous value, leaving NaN on success. |
| `ppf_prec_round` | Changes precision while rounding the current value under the supplied mode and context. |
| `ppf_context_set_default_precision` | Changes a context's default policy; existing objects retain their precisions. |

Use `ppf_prec_round` when the existing value must be retained. Narrowing can lose information. Widening can preserve the stored value exactly when it remains within the applicable range, but cannot reconstruct digits lost before the widening. Always check the return value; a request for more precision can require allocation.

Initialization offers several precision and context choices. `ppf_init` uses the calling thread's default context and its default precision. `ppf_init2` uses an explicit precision with that thread's default context. `ppf_init2_ctx` uses the precision you pass and the explicit context you pass. Setting an explicit context's default precision does not redirect a later `ppf_init` to that context. For introductory code, a named precision and `ppf_init2_ctx` make the intended pairing unambiguous.

A higher-precision source does not resize a lower-precision destination during `ppf_set` or arithmetic. That behavior lets you design a calculation with high-precision intermediate work and a deliberately smaller final output. It also means that a mistakenly small temporary can lose information before the final output is computed.

## Copy values without copying ownership

C structure assignment copies pointers. Consequently, `b = a` and `memcpy(&b, &a, sizeof b)` are not ways to duplicate an owned Polyphase Float number. They can make two structures refer to the same storage and later free that storage twice.

Instead, initialize `b` at its intended precision and call:

```c
int ternary = ppf_set(&b, &a, PPF_RNDN, &ctx);
```

This copies the numerical value into `b` with the destination's rounding and precision. The allocations remain independently owned. Check `PPF_TERNARY_ERROR` before interpreting the sign of `ternary`.

`ppf_swap(&a, &b)` exchanges the complete object states, including precision and ownership. It is useful when an algorithm wants to make a completed temporary become the current result. After a swap, each variable owns the state it received and should still be cleared exactly once. Do not assume that either variable retained its old precision.

Pass object pointers to your own helper functions too. A helper can receive `ppf_t *out` and `const ppf_t *input`; taking a `ppf_t` by value would copy its owning fields. Keep helpers' ownership contracts simple: either they operate on an already initialized destination, or they explicitly initialize a new one and document who clears it.

Aliasing means that two arguments refer to the same object or overlapping storage. Some operations support using an input as the destination; others have stricter rules, especially paired outputs and batch interfaces. The introductory programs use separate objects so their data flow is visible. Check the per-function aliasing contract before replacing those objects with an in-place optimization.

## Give each context a clear owner

A context holds default precision, default rounding, the minimum and maximum exponent, sticky flags, allocation callbacks, and reusable temporary storage. Initialize it with `ppf_context_init`, then adjust policy through the public setters if necessary. `ppf_context_set_rounding` accepts a concrete rounding mode; operations can subsequently request that policy using `PPF_RND_DEFAULT`.

A context is mutable even when the operation's mathematical inputs are read-only: allocation and flags can change it. In concurrent code, give each worker its own context or protect shared use with appropriate synchronization. A shared read-only operand is permitted while no thread mutates, clears, resizes, swaps, or reinitializes it. A destination requires exclusive access for the duration of an operation. OpenMP support inside the library does not remove these application ownership rules.

Use `ppf_context_clone` to create a separate context with copied policy and flags. It initializes independent temporary-storage state rather than sharing the source context's active scratch arena. The clone retains the allocator callbacks and opaque state, so a custom allocator's lifetime and thread-safety requirements still apply. The destination of cloning must be available for initialization; do not clone over a live context without first releasing it.

For ordinary code, clear numbers before clearing the context that created them. Each number actually retains the allocator callbacks needed to free its own allocation, so the context structure itself is not a hidden owner of every number. With custom allocators, however, the opaque allocator state must stay valid until all dependent objects, buffers, and contexts have been released. The straightforward object-first cleanup order makes that obligation easy to meet.

## Use the matching release function

| Resource | How it is released |
|---|---|
| Initialized, normally owned `ppf_t` | `ppf_clear` |
| Explicit `ppf_context_t` | `ppf_context_clear` |
| String allocated by `ppf_get_str(NULL, ...)` | `ppf_free_str` |
| Successful `ppf_asprintf` or `ppf_vasprintf` output | C `free` |
| Caller-supplied output buffer | Its existing application owner releases it |

Those string rules are different on purpose. Do not infer the deallocator merely from the return type `char *`. The API reference states ownership for each allocating function. The introductory examples use stream output so there is no returned string allocation to manage.

`ppf_thread_cleanup` releases the calling thread's default context and internal thread-local workspaces. It may be called repeatedly. It does not clear your user-owned numbers, explicit contexts, plans, or random states. Call it after releasing those resources at the end of a worker or a standalone program when deterministic thread cleanup is desired. Normal thread-exit cleanup is also available where the library successfully registers its POSIX cleanup key; explicit cleanup keeps the example's lifetime visible.

## Keep explicit and default diagnostics together

If a calculation receives `&ctx`, inspect it with `ppf_context_flags(&ctx)` and clear it with `ppf_context_flags_clear(&ctx, mask)`. The shorter `ppf_flags_*` functions refer to the calling thread's default context. They do not automatically inspect whichever explicit context your previous operation used.

Many numerical APIs resolve a null context to the thread-local default context, but lifecycle calls have their own preconditions. Do not generalize the null-context convention to every function. The explicit form used here avoids that ambiguity and works naturally when a program manages multiple numerical policies.

**Exercise.** An eight-bit object contains an approximation to `0.1`. Which operation lets you increase its precision while retaining its stored value: `ppf_set_prec` or `ppf_prec_round`?

**Answer.** `ppf_prec_round`, with appropriate range and rounding policy. The widened object still contains the earlier approximation. `ppf_set_prec` discards the value and leaves NaN on success. Parsing the original decimal string again into a wider object is a different operation and can recover more information from that original input.

**Exercise.** Can clearing a context replace clearing all numbers created through it?

**Answer.** No. Numbers own their significand allocations. Clear each owned number and then clear the context's own resources. The same distinction applies to thread cleanup.
