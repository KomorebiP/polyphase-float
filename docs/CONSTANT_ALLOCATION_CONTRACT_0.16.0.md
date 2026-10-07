# Polyphase Float 0.16.0 certified-constant allocation contract

## Scope

This contract covers project-controlled allocations reached by the public
certified-constant family for pi, log 2, Euler's constant, and Catalan's
constant. It governs the 12 evaluation functions, five precompute functions,
interval export, cache controls, thread controls, and last-statistics export.

The contract does not intercept allocations performed internally by GMP. It
also does not claim recovery from operating-system failure, process termination,
or an allocator that violates the callback contract.

## Evaluation transaction

A constant evaluation constructs the result in a temporary `ppf_t`. The caller
object is committed by `ppf_swap` only after interval construction, outward
rounding, range handling, and project-controlled allocation have completed.

For an injected unrecovered allocation failure:

1. the function returns `PPF_TERNARY_ERROR`
2. `PPF_FLAG_ALLOC` is sticky on the resolved caller context
3. an initialized destination remains byte-for-byte unchanged
4. the exact interval cache remains at its last valid state
5. rounded-cache entries remain at their last valid state
6. all project-owned temporary allocations are released
7. a later non-failing call remains usable, including when the allocation flag was already set

Destination preservation includes the limb pointer, capacity, used-limb count,
precision, exponent, sign, category, storage flags, and limb contents.

## Exact cache commit

A newly certified interval is built outside the live cache. Once the candidate
is complete and better than the retained interval, the GMP integers and
exponent are committed by swap under the constant mutex. No partially updated
interval becomes observable.

## Rounded cache commit

Rounded cache growth is best effort. A complete candidate object is built under
a private process allocator, then committed by swap. Failure to grow this
secondary cache does not convert an otherwise complete caller result into an
error and cannot consume the caller's custom-allocation budget.

## Builder allocation routing

Project C work arrays used by the four constant builders retain their allocator
provenance in a private prefix. The prefix stores the exact callbacks and byte
count required for later reallocation or release.

The log 2 packet builder allocates packet bounds, nodes, and every worker's
scratch array before entering the OpenMP region. Public custom allocators are
therefore not invoked concurrently by those workers.

The embedded pi engine routes its C work arrays through the active context. The
active context is published and cleared under the same mutex used by every
allocator callback, so OpenMP workers cannot race with a context transition.
The public constant path uses exact non-adic Chudnovsky binary splitting.
Optional p-adic execution paths in the standalone embedded engine are not part
of this 0.16.0 allocation contract.

Decimal staging returned by `mpz_get_str(NULL, ...)` remains GMP-owned. The
engine obtains GMP's active free callback with `mp_get_memory_functions` and
never routes those strings through the context-owned buffer prefix.

Euler and Catalan builders clone the caller context for widened arithmetic and
propagate allocation failure from that nested context to the caller.

## Sticky allocation state

A preexisting `PPF_FLAG_ALLOC` remains set across a successful constant call.
The implementation temporarily distinguishes old state from a new failure so a
sticky diagnostic bit does not prevent later constant evaluation.

An internal thread-local allocation-failure epoch records project-controlled
allocator failures even when a nested context catches or clears its local flag.
This epoch is diagnostic state only and is not part of the public ABI.

## Precompute and cache controls

Named and generic precompute calls build and commit an exact interval under the
default context and the same swap boundary. Cache query and lifecycle functions
remain valid after every injected failure. Cache purge releases completed cache
objects only.

## Remaining boundary

The following remain outside this block:

- allocations performed internally by GMP
- remaining transcendental work arrays
- remaining special-function work arrays outside the constant builders
- selected worker metadata outside the constant family
- optional p-adic paths in the embedded standalone pi engine
- failures after an external stream backend begins an irreversible operation
