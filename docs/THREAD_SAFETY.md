# Thread-safety and cache-lifecycle contract

This document defines the supported concurrency model for Polyphase Float
0.11.0 on the POSIX-thread baseline. It distinguishes objects that may be
shared for reading from mutable state that requires exclusive ownership.

## Summary table

| Resource | Concurrent use |
|---|---|
| Distinct `ppf_t` destinations with distinct contexts | Supported |
| One `ppf_t` used only as a read-only input | Supported |
| One `ppf_t` mutated by more than one thread | Not supported without external locking |
| Thread-default context | Private to the calling thread |
| One explicit `ppf_context_t` shared by threads | Not supported without external locking |
| Immutable divisor plan applied by several threads | Supported |
| Global constant and transcendental caches | Internally synchronized |
| Global cache purge during active calculations | Supported; purge may block |
| One GMP random state shared by threads | Not supported without external locking |
| Library unload while Polyphase threads are active | Not supported |

## Thread-default context

`ppf_default_context()` returns a `_Thread_local` context. Its precision,
rounding mode, exponent range, flags, allocator, and scratch arena are isolated
from every other thread.

The first use of heap-backed thread state registers a POSIX thread-specific
destructor. When registration succeeds, normal thread termination releases:

- the calling thread's default-context arena;
- thread-local product-planner and Toom workspaces;
- other heap-backed state owned by that thread-local execution layer.

`ppf_buildopt_thread_cleanup_p()` reports whether the process created the
thread-specific cleanup key. `ppf_thread_cleanup()` performs the same cleanup
explicitly and may be called repeatedly. `ppf_default_context_cleanup()` is a
compatibility alias.

Applications should call `ppf_thread_cleanup()` before unloading the shared
library and may call it on long-lived worker threads after a large temporary
workload. The process main thread should use explicit cleanup when leak-clean
shutdown evidence is required, because process termination semantics are not a
substitute for application-controlled resource release.

Automatic thread cleanup does not clear user-owned `ppf_t` objects, divisor
plans, random states, or explicit contexts. Those resources remain the
application's responsibility.

## Explicit contexts

An explicit `ppf_context_t` is mutable execution state. Its flags and scratch
arena can change during an operation. It therefore has one-thread-at-a-time
ownership unless the application supplies external synchronization.

Separate contexts may use the same allocator callbacks concurrently only when
the allocator implementation and its opaque state are themselves thread-safe.

`ppf_context_clear()` may be called directly on the current thread-default
context. Version 0.11.0 recognizes that case and makes the next
`ppf_default_context()` call recreate the documented defaults.

## Floating-point objects

A fully initialized `ppf_t` may be read by several threads at the same time if
no thread can mutate, clear, resize, swap, or reinitialize it. This supports
shared constants and operands with thread-private outputs.

Any operation that can modify an object requires exclusive ownership of that
object for the duration of the call. This includes operations where an output
aliases an input. An object must not be cleared while another thread is
reading it.

## Divisor plans

A completed `ppf_divisor_plan_t` is immutable except for its atomic application
counter. It may be applied concurrently by several threads when each call uses
its own mutable destination and context. The plan must not be destroyed until
all applications have completed.

## Global caches

Constant caches and fixed-point transcendental caches retain their mutexes and
initialized GMP objects for the process lifetime. A purge clears cached values
and oversized storage but does not destroy synchronization objects.

The following operations are supported while other threads calculate
constants or logarithms:

- `ppf_clear_constant_cache()`;
- `ppf_free_constant_caches()`;
- `ppf_free_cache2(PPF_FREE_GLOBAL_CACHE)`.

A purge acquires the corresponding cache mutexes and can block until an active
builder releases them. A calculation that starts after a purge may rebuild the
entry.

`ppf_free_cache2(PPF_FREE_LOCAL_CACHE)` affects only the calling thread's
scratch pool and thread-local product workspaces. It does not release state
owned by another live thread.

## Internal OpenMP

The optional OpenMP layer is independent of the application-level ownership
rules. Several application threads may invoke Polyphase operations
concurrently when their destinations and contexts are disjoint and their
shared inputs are read-only. The library may cap effective worker counts or
select a serial path when already inside an OpenMP region.

The ThreadSanitizer validation build deliberately uses `PPF_OPENMP=OFF` so
race reports describe the Polyphase POSIX-thread layer rather than an
uninstrumented OpenMP runtime.

## Unsupported patterns

The following require application-level serialization or are outside the
0.11.0 contract:

- mutating the same `ppf_t` from multiple threads;
- sharing one explicit context without a lock;
- sharing one `gmp_randstate_t` without a lock;
- destroying an object or plan while another thread uses it;
- changing allocator state while dependent objects are live;
- calling `dlclose` while another thread may enter Polyphase Float;
- assuming thread cleanup frees application-owned objects.

## Validation

The focused concurrency suite covers:

- independent default-context precision, range, rounding, flags, and arenas;
- explicit and automatic thread cleanup;
- leak-accounted thread-exit destruction;
- shared read-only operands;
- concurrent immutable divisor-plan application and atomic accounting;
- concurrent constant and logarithm evaluation during global cache purge;
- post-purge reconstruction of Euler and Catalan constants;
- arena alignments through 128 bytes;
- repeated stress under ThreadSanitizer;
- AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection.

See `THREAD_SAFETY_VALIDATION_0.11.0.md` for the release-specific evidence and
remaining limitations.
