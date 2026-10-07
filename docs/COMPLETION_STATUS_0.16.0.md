# Polyphase Float 0.16.0 completion status

The conservative version 1.0 readiness estimate is **approximately 68%**, with
about **±5 percentage points** of uncertainty.

Version 0.16.0 closes project-controlled allocation atomicity for the certified
constant family. It adds destination staging, exact-cache swap commits,
secondary rounded-cache staging, allocator-provenance work buffers, serial
worker-scratch preallocation, nested-context failure propagation, and a
deterministic failure matrix for all public constant routes.

The generated semantic ledger records:

```text
437 public core functions
437 direct test-source references
216 functions with incomplete allocation-failure atomicity evidence
112 functions with a bounded implementation path
6 conditional binary128 or decimal interfaces
```

The remaining high-weight version 1.0 blocks are:

1. close allocation atomicity for remaining transcendental and special-function work arrays and worker metadata
2. audit, justify, or remove the 112 bounded implementation paths
3. expand hard-to-round, narrow-range, flag, and special-value matrices
4. validate native Windows, macOS, ARM64, big-endian, and additional data models
5. conduct sustained fuzzing and independent external replication
6. finish proof-to-source traceability and publication-grade performance replication
