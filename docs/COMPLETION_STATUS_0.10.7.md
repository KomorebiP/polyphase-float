# Project completion status after 0.10.7

## Strict roadmap completion

The published improvement map contains 66 separately scoped blocks. The blocks
closed through 0.10.7 are:

1. canonical numerical baseline;
2. canonical benchmark report;
3. complete environment capture;
4. raw timing retention;
5. statistical analysis and uncertainty;
6. randomized and counterbalanced campaign order;
27. machine-profiled AUTO product planner;
63. automated performance-regression tracking.

That is:

```text
8 / 66 blocks = 12.1 percent
```

This percentage counts blocks equally. It is intentionally not an estimate of
remaining engineering effort because the blocks vary greatly in size.

## Conservative effort-weighted readiness estimate

A separate, non-normative readiness estimate places the project at roughly
**43 percent** of a production-grade version 1.0 program.

| Area | Estimated readiness | Main boundary |
|---|---:|---|
| API and function inventory | 100% | 349/349 counterparts and 426 exports |
| Evidence and release foundation | 75% | strong local infrastructure; external replication remains |
| Correctness and safety hardening | 35% | broad differential tests exist; fuzzing, failure injection, ThreadSanitizer, and proof traceability remain |
| Performance engineering | 15% | planner block closed; most kernel and special-function blocks remain |
| Portability and independent replication | 10% | Linux/WSL baseline exists; broader OS, architecture, and external evidence remain |
| ABI, documentation, publication, and 1.0 gate | 20% | public project foundation exists; stable ABI and publication artifact remain |

The 43 percent figure is an engineering estimate, not a measured test result.
It is deliberately conservative and should be revised only when additional
roadmap blocks close with reproducible evidence.

## Meaning of 0.10.7 completion

Block 27 is considered closed because 0.10.7 provides all of the following:

- one central AUTO planner for multiply, square, FMA/FMS, and reciprocal square root;
- operation-specific effective-worker caps;
- a conservative portable fallback;
- generated machine-specific profiles with independent-process, binding-agreement, effect-size, and bootstrap gates;
- public plan and profile introspection;
- a resumable calibration workflow;
- tuned-build validation against the explicitly selected algorithm;
- portable and fixture-profile regression tests;
- strict build, full differential, smoke, installation-consumer, and release-audit validation.

The portable release intentionally contains no machine-specific tuning rules.
A user-generated profile is an optional local build input and must be
recalibrated after material machine, compiler, GMP, OpenMP, or topology changes.
