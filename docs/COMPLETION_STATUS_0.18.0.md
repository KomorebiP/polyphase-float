# Polyphase Float 0.18.0 completion status

The conservative version 1.0 readiness estimate is **approximately 81.0%**,
with about **±3.5 percentage points** of uncertainty.

This is a weighted engineering assessment, not a percentage of all possible
inputs or a universal correctness claim. Version 0.18.0 closes all 56 remaining
allocation-atomicity entries in the public arithmetic, fused/planned-operation,
and root families. These are high-frequency core operations, so the block is
weighted more heavily than its raw function count alone.

The generated semantic ledger records:

```text
437 public core functions
437 direct test-source references
81 functions with incomplete allocation-failure atomicity evidence
112 functions with a bounded implementation path
6 conditional binary128 or decimal interfaces
213 functions with verified transactional allocation behavior
246 entries without a currently recorded gap
```

The allocation-atomicity gap count falls from 137 to 81, a reduction of 56
entries or 40.9 percent of the previously unresolved allocation ledger. Across
versions 0.15.0 through 0.18.0, the ledger has fallen from 228 unresolved
allocation entries to 81.

The remaining high-weight version 1.0 blocks are:

1. close or explicitly delimit the remaining 81 allocation-atomicity entries, led by assignment, conversion, integer-rounding, remainder, random, runtime, and custom-storage surfaces
2. audit, justify, or remove the 112 bounded implementation paths
3. expand hard-to-round, narrow-range, flag, and special-value matrices
4. validate native Windows, macOS, ARM64, big-endian, and additional data models
5. conduct sustained fuzzing and independent external replication
6. finish proof-to-source traceability and publication-grade performance replication

The estimate does not reduce the weight assigned to portability, independent
replication, proof review, or long-duration fuzzing merely because the central
allocation transaction families are now closed.
