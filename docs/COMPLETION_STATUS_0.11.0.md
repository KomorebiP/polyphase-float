# Polyphase Float 0.11.0 completion status

## Engineering estimate

The conservative readiness estimate for a defensible version 1.0 is:

```text
approximately 54 percent complete
uncertainty: about plus or minus 5 percentage points
```

This is an engineering assessment, not a statistic. The implemented numerical
surface is substantially more complete than 54 percent. Remaining weight is
concentrated in production obligations rather than function count.

## Block closed in 0.11.0

- supported POSIX-thread ownership contract
- thread-local default-context isolation
- automatic thread-exit cleanup for heap-backed Polyphase TLS state
- deterministic cache-lifecycle stress
- concurrent global cache purge and reuse
- GCC ThreadSanitizer validation of the serial core path
- historical race reproduction and correction
- ASan and UBSan validation of the concurrency harness
- alignment-safe scratch-arena block placement under custom and sanitizer allocators

## Completed foundations

- 349 of 349 direct or conditional lowercase inventory counterparts
- 431 public `ppf_*` declarations and matching configured shared-library
  exports on the validated GCC baseline
- independent GMP-based production core with no MPFR imports
- exact arithmetic, roots, constants, elementary functions, special functions,
  I/O, random support, contexts, custom storage, and an optional MPFR adapter
- deterministic allocation-failure injection for the documented core paths
- optional OpenMP and a verified serial path
- parser and PPFIF fuzzing harnesses
- retained-sample and counterbalanced benchmark evidence
- evidence-gated product-planner profiles
- reproducible packaging and release auditing

## Highest-weight remaining work

1. all-function allocation-failure coverage beyond the current core subset
2. stable public ABI decision and automated ABI comparison
3. native Windows, macOS, ARM64, and additional architecture validation
4. sustained fuzzing and permanent regression-corpus governance
5. complete function-by-function semantic and aliasing matrix
6. hard-to-round adversarial corpus expansion
7. proof-to-source traceability and external numerical review
8. independent correctness and performance replication
9. performance refinement for logarithms, reciprocal square root, inverse
   angles, hyperbolic functions, and special-function families
10. explicit cancellation, fork, plugin-unload, and long-running service policy

## Version 1.0 boundary

Version 1.0 still requires a frozen public contract, supported-platform
evidence, sustained safety testing, published practical limits, independent
review, and reproducible external validation. API inventory completion alone is
not a version 1.0 criterion.
