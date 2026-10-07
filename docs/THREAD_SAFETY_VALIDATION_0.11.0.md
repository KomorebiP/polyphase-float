# Polyphase Float 0.11.0 thread-safety validation

## Objective

The 0.11.0 block defines and tests the supported POSIX-thread concurrency and
cache-lifecycle boundary without changing numerical formulas.

## Production changes

- one-time immutable initialization for Euler and Catalan constant descriptors
- automatic POSIX thread-exit cleanup for the default context and direct-limb
  Toom workspaces
- explicit idempotent `ppf_thread_cleanup`
- build query for automatic thread-cleanup support
- alignment-safe arena-block placement and preservation of the original
  allocator pointer and exact allocation size for deallocation
- a serial ThreadSanitizer build mode

## Deterministic concurrency test

`tests/test_thread_safety.c` covers:

1. independent default contexts in eight simultaneous threads
2. isolation of precision, range, rounding mode, flags, and arena state
3. explicit cleanup, repeated cleanup, direct default-context clearing, and
   later reinitialization
4. automatic thread-exit cleanup measured with a counting allocator
5. explicit-context ownership and cleanup of direct-limb Toom workspaces
6. concurrent first use of certified constant descriptors
7. simultaneous constant and logarithm computation during global cache purges
8. local-cache cleanup between operations on active worker threads
9. reuse of every cache after concurrent cleanup
10. shared read-only operands with thread-private destinations and contexts
11. concurrent immutable divisor-plan application and atomic accounting
12. arena alignment from 1 through 128 bytes

The test can be amplified with:

```sh
PPF_THREAD_STRESS_MULTIPLIER=8 ./test_thread_safety
```

## Sanitizer boundary

The race-detection configuration is deliberately serial:

```sh
cmake -S . -B build/tsan \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_ENABLE_THREAD_SANITIZER=ON
cmake --build build/tsan -j
ctest --test-dir build/tsan -L threading --output-on-failure
```

This instruments Polyphase code, the core tests, POSIX-thread cache access, and
thread-exit cleanup. It does not claim that an external OpenMP runtime is
ThreadSanitizer-clean.

## Defects found and corrected

### Mutable special-constant descriptors

The 0.10.9 Euler and Catalan accessors rewrote fields in static descriptors on
every lookup. The values were identical, but simultaneous non-atomic writes are
a data race under the C memory model. Version 0.11.0 initializes the caches and
descriptors inside one `pthread_once` callback and treats them as immutable
afterward. See `THREAD_SAFETY_BASELINE_RACE_0.10.9.md`.

### Arena-block alignment

The sanitizer build exposed that a custom or sanitizer allocator could return a
layout in which the arena block header was not correctly aligned after manual
placement. Version 0.11.0 allocates alignment slack, aligns the block metadata,
and retains the original pointer and exact byte count for deallocation. The
focused test verifies returned scratch addresses through 128-byte alignment.

## Completed development-environment validation

| Check | Result |
|---|---:|
| GCC OpenMP strict CTest inventory | 55 / 55 passed |
| GCC serial strict CTest inventory | 55 / 55 passed |
| Clang serial strict CTest inventory | 55 / 55 passed |
| ThreadSanitizer focused CTest | passed |
| ThreadSanitizer stress multiplier | 8, passed |
| AddressSanitizer, UndefinedBehaviorSanitizer, and leak checks | 5 / 5 focused tests passed |
| Installed OpenMP CMake consumer | passed |
| Installed serial CMake consumer | passed |
| Serial OpenMP-runtime dependency | none |
| Public declarations and GCC shared-library exports | 431 / 431 |
| Production MPFR imports | 0 |
| Deterministic counting-allocator thread-exit cleanup | zero live blocks and bytes |
| Post-purge constant reconstruction | passed |

The local environment had the MPFR runtime library but not the official
`mpfr.h` development header. Differential builds therefore used the repository
compatibility declarations. The target-machine runner requires the official
header because optional MPFR declarations depend on the compiler and installed
MPFR build.

## Explicit limitations

The result does not establish:

- safe simultaneous mutation of one context or destination object
- asynchronous-cancellation safety
- signal-handler safety
- fork-without-exec safety after multithreaded use
- ThreadSanitizer coverage of the OpenMP runtime
- native Windows, macOS, ARM64, or big-endian thread behavior
- safety of application allocators or GMP random states shared without locks
- safety of unloading the shared library while Polyphase threads are active
