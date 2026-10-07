# Polyphase Float 0.15.0 completion status

The conservative version 1.0 readiness estimate is **approximately 66%**, with
about **±5 percentage points** of uncertainty.

Version 0.15.0 closes the bounded public text and interchange allocation block.
It adds transactional numerical parsing, allocator-provenance strings, staged
formatting, pre-write stream commit boundaries, staged PPFIF export, and a
deterministic fail-at-every-project-allocation test for the governed paths.

The generated semantic ledger now records:

```text
437 public core functions
437 direct test-source references
228 functions with incomplete allocation-failure atomicity evidence
112 functions with a bounded implementation path
6 conditional binary128 or decimal interfaces
```

The remaining high-weight version 1.0 blocks are:

1. close allocation atomicity for certified constants, transcendental work
   arrays, special-function work arrays, worker metadata, and cache growth;
2. audit, justify, or remove the 112 bounded implementation paths;
3. expand hard-to-round, narrow-range, flag, and special-value matrices;
4. validate native Windows, macOS, ARM64, big-endian, and additional data
   models;
5. conduct sustained fuzzing and independent external replication;
6. finish proof-to-source traceability and publication-grade performance
   replication.
