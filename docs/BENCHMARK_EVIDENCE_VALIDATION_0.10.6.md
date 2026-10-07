# Benchmark evidence validation for 0.10.6

Version 0.10.6 adds counterbalanced campaign scheduling, deterministic
condition-order randomization, paired binding/order analysis, and a performance
regression comparator.

Validation requirements for the final source and benchmark artifacts are:

- strict Python syntax and release audits;
- deterministic schedule generation with stable extension;
- equal `close-first` and `spread-first` counts for every complete two-round
  block;
- exact resume without duplicate evidence;
- matched close/spread condition-order signatures within each process round;
- synthetic recovery of known placement and order effects;
- confirmed-regression detection only when the configured independent-process
  threshold is met;
- preservation of all 35 production source files from 0.10.5.

The detailed artifact-level results are shipped with the complete 0.10.6
bundle.
