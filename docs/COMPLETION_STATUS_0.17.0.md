# Polyphase Float 0.17.0 completion status

The conservative version 1.0 readiness estimate is **approximately 75.0%**,
with about **±4.0 percentage points** of uncertainty.

This is a weighted engineering assessment, not a claim that 75 percent of all
possible inputs or proofs have been exhausted. Version 0.17.0 closes the
largest remaining coherent project-allocation block by making 79 public
transcendental and special-function endpoints transactional. It also moves
serial and OpenMP work arrays into the allocator-provenance layer and adds a
failure matrix that covers ordinary, aliased, paired, sign, external-storage,
high-precision, and sticky-flag cases.

The generated semantic ledger records:

```text
437 public core functions
437 direct test-source references
137 functions with incomplete allocation-failure atomicity evidence
112 functions with a bounded implementation path
6 conditional binary128 or decimal interfaces
157 functions with verified transactional allocation behavior
```

The allocation-atomicity gap count falls from 216 to 137, a reduction of 79
entries or 36.6 percent of the previously unresolved allocation ledger.

The remaining high-weight version 1.0 blocks are:

1. close or explicitly delimit the remaining 137 allocation-atomicity entries, led by arithmetic, assignment, rounding, fused operations, runtime, and root surfaces
2. audit, justify, or remove the 112 bounded implementation paths
3. expand hard-to-round, narrow-range, flag, and special-value matrices
4. validate native Windows, macOS, ARM64, big-endian, and additional data models
5. conduct sustained fuzzing and independent external replication
6. finish proof-to-source traceability and publication-grade performance replication

The readiness increase from the 0.16.0 estimate reflects the size and depth of
the closed family, including allocation routes with thousands of project
allocations. It does not reduce the weight assigned to portability, external
replication, or proof review.
