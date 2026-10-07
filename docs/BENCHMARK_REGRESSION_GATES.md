# Performance regression gates

Version 0.10.6 adds `scripts/compare_benchmark_runs.py`.

The comparator matches conditions by precision, method, and worker count. It
uses the median of independent-process medians as the point estimator and a
deterministic hierarchical bootstrap for the candidate-to-baseline ratio.

## Classifications

- `confirmed_regression`: the complete confidence interval exceeds the selected
  regression threshold and both campaigns meet the minimum process count;
- `preliminary_regression`: the point estimate exceeds the threshold but the
  evidence is too weak for confirmation;
- `stable`: the threshold is not crossed;
- `confirmed_improvement` and `preliminary_improvement`: analogous improvement
  classifications;
- `descriptive_cross_machine_*`: the same statistical signal is reported for
  inspection, but it cannot trip an automated gate.

The default threshold is five percent and the default confirmation requirement
is three independent processes per condition. A legacy single-median baseline
can identify a preliminary concern, but cannot confirm a regression. A matching
machine fingerprint is required for gate eligibility by default.

The tool writes `regression_summary.csv`, `regression_summary.json`,
`REGRESSION_REPORT.md`, and `regression_manifest.json`. It can return a nonzero
status for confirmed regressions when used in a controlled same-machine gate.
