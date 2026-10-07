# Polyphase Float 0.14.0 completion status

The conservative version 1.0 readiness estimate is **approximately 64%**, with about **±5 percentage points** of uncertainty.

Completed high-weight areas now include the public function inventory, release packaging, reproducible evidence capture, product-planner governance, optional OpenMP, parser and interchange fuzzing, deterministic allocation-failure testing for covered paths, thread-lifecycle hardening, ABI epoch 1, the generated semantic contract, and the systematic aliasing contract.

The largest remaining version 1.0 blocks are:

1. close the 232 recorded allocation-failure atomicity gaps;
2. audit and justify or remove the 112 bounded implementation paths;
3. expand hard-to-round, narrow-range, flag, and special-value matrices;
4. complete native Windows, macOS, ARM64, and broader architecture validation;
5. extend sustained fuzzing and independent external replication;
6. finish proof-to-source traceability and publication-grade performance replication.
