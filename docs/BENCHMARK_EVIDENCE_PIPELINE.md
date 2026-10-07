# Benchmark evidence pipeline

## Purpose

The benchmark system is an evidence-producing subsystem, not only a collection
of timers. A retained result must be traceable to the source payload, compiled
executable, machine and software environment, command, process launch, raw
in-process samples, correctness gate, and deterministic analysis procedure.

Version 0.10.5 changes benchmark-recording and analysis infrastructure. It does
not change a production numerical kernel.

## Record schema

The core benchmark executables emit JSON schema version 2. Each record contains:

- operation, precision, worker count, and job count;
- warmup count and measured repetition count;
- `sample_statistic: "median"`;
- the reported median in `seconds`;
- `sample_count`;
- `samples_seconds`, preserving acquisition order;
- `verified: true` only after the operation-specific numerical gate passes.

The campaign runner rejects records with missing samples, nonpositive samples,
a failed verification gate, a mismatched method name, or a reported median
that does not agree with the retained samples.

## Campaign identity and resume behavior

A campaign has one stable identifier and one `run_config.json`. The condition
identifier is derived from binding, precision, method, worker count, job count,
and independent-process index. A resumed run skips only exact completed
conditions.

Resume is refused when the output directory was produced with a different:

- precision grid;
- worker grid;
- in-process sample count;
- rank-planner grid;
- binding policy;
- build flag set;
- source payload;
- visible physical-core count;
- machine fingerprint;
- software fingerprint.

The number of independent process repetitions may be increased on resume, but
not reduced. Benchmark data without a matching run configuration is rejected.

## Environment evidence

`environment.json` and `environment.txt` record the Linux or WSL execution
context. The JSON record includes:

- CPU model, affinity, visible physical-core topology, and frequency-policy
  fields;
- operating system, kernel, WSL detection, and distribution;
- compiler, linker, CMake, Make, Python, GMP, and MPFR information;
- MPFR build options where the linked library exposes them;
- selected OpenMP and build variables;
- privacy-conscious machine and software fingerprints.

The Windows launcher additionally writes `windows_environment.json` before the
Linux campaign begins. It omits usernames, raw computer names, and serial
numbers.

## Statistical unit and estimator

An independently launched process is the experimental unit. The point estimate
for a condition is the median of the per-process medians, giving each process
equal weight even if sample counts differ.

Confidence intervals use a deterministic hierarchical nonparametric bootstrap:

1. process records are sampled with replacement;
2. samples inside each selected process are sampled with replacement;
3. a median is computed for each selected process;
4. the median of those process medians is retained;
5. the 2.5th and 97.5th percentiles form the reported interval.

With one process, the interval measures only in-process variation. It is not an
independent-process reproducibility interval. Public performance campaigns
should use at least three process repetitions, with more repetitions preferred
for narrow effects.

## Generated evidence

A completed campaign contains:

- `raw.jsonl`: one correctness-gated record per condition and process launch;
- `raw_samples.csv`: every timing sample in tabular form;
- `process_statistics.csv`: one summary per process launch;
- `condition_statistics.csv`: process-weighted summaries and intervals;
- `internal_scaling.csv`;
- `equal_worker_throughput.csv`;
- `one_thread_vs_mpfr.csv`;
- `rank_raw.csv` and `rank_planner.csv`;
- environment and run-configuration records;
- `REPORT.md`;
- `analysis_manifest.json`, including hashes of generated evidence files.

## Interpretation limits

- Multiworker latency is not an equal-resource comparison with one-thread
  MPFR.
- Planned-division application timings do not include plan construction unless
  a benchmark explicitly says otherwise.
- A machine-specific result is not a universal performance result.
- A correctness-gated benchmark is evidence for the tested values and semantic
  checks, not a proof over all inputs.
- Close-versus-spread comparisons require randomized or alternating campaign
  order before placement can be treated as the cause of a difference.
