# Context and memory

A context owns execution policy that is not part of an individual number:

- default precision
- minimum and maximum exponent
- default rounding mode
- exception flags
- allocation callbacks
- a reusable scratch arena

Explicit contexts are intended to be owned by one thread. The convenience
default context is thread-local, including its flags and scratch arena.

## Custom allocators

`ppf_context_init_custom` accepts malloc, realloc, and free callbacks plus an
opaque pointer. Objects initialized through a context retain the allocator
callbacks needed to release their own significand storage. The opaque allocator
state must remain valid until every dependent object and context has been
cleared.

Version 0.10.9 adds `PPF_FLAG_ALLOC`. Context-owned allocation helpers raise
this sticky flag when a configured allocator rejects a request.

## Transactional failures

The deterministic 0.10.9 hardening suite verifies that covered operations leave
an already initialized destination byte-for-byte unchanged when an allocation
failure cannot be recovered. It also verifies successful cleanup and reuse.

`ppf_init2_ctx` accepts a mutable context because initialization can now raise
the allocation flag. An initialization failure leaves an all-zero object that
can safely be cleared.

`ppf_reserve` and `ppf_set_prec` preserve the object when reallocation fails.
Their existing signatures do not accept a context, so they do not raise a
caller-selected context flag.

## Scratch arena

The scratch arena uses stable linked blocks. Growing the arena does not
invalidate pointers returned from earlier blocks. A mark can be restored after
a temporary calculation, and a reset keeps the first block for reuse.

Version 0.11.0 aligns arena metadata explicitly inside the original allocator
block and retains the original pointer and exact size for deallocation. This
makes the metadata valid even when a sanitizer or custom allocator returns a
pointer with less alignment than the internal block type requires. Returned
scratch addresses continue to honor the caller's power-of-two alignment.

When arena growth fails, the old block chain and active mark remain valid. The
allocation flag is raised and the request returns null.

See `ALLOCATION_FAILURE_CONTRACT.md` for the exact covered operations and the
remaining boundary.

## Thread lifecycle

The default context is thread-local. Version 0.11.0 registers a POSIX
thread-specific destructor on first use of heap-backed thread state. Normal
thread exit releases the default-context arena and thread-local product
workspaces when the key could be registered.

`ppf_thread_cleanup()` provides deterministic cleanup and may be called more
than once. Applications should use it before unloading the library and on the
main thread when leak-clean shutdown evidence is required. User-owned numbers,
explicit contexts, divisor plans, and random states are never destroyed
automatically.

Explicit contexts are mutable and should have one-thread-at-a-time ownership.
Read-only numbers and completed divisor plans may be shared under the ownership
rules in `THREAD_SAFETY.md`.
