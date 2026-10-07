# Canonical baseline policy

## Baseline identity

A canonical Polyphase Float benchmark baseline consists of one immutable set of:

1. source archive and SHA-256 digest;
2. benchmark artifact and SHA-256 digest;
3. benchmark version and record-schema version;
4. machine and software fingerprints;
5. exact build flags and executable digests;
6. run configuration;
7. correctness-gated raw records;
8. retained timing samples where available;
9. deterministic analysis files and their digests;
10. a written claim boundary.

A baseline is identified by content hashes, not by a mutable directory name.

## Historical and current evidence

Historical benchmark campaigns remain immutable and are stored under a named
reference directory. They are never silently rewritten into a newer record
schema. A current root report may consolidate historical campaigns, but it must
state when the underlying records contain only condition medians rather than
complete sample vectors.

The completed ten-core close and spread campaigns preceding 0.10.5 are retained
as a legacy reference baseline. Their records were correctness-gated, but they
do not retain every in-process timing sample. Therefore they support descriptive
medians, cross-campaign ranges, and planner observations, but not reconstructed
sample-level confidence intervals.

Version 0.10.5 introduces raw-sample retention and independent-process indexing
for new campaigns. It does not retroactively manufacture missing samples for
legacy campaigns.

## Baseline freeze rules

After a baseline manifest is published:

- source files, raw records, and environment records are immutable;
- derived reports may be regenerated only by a versioned analysis program;
- regenerated files must identify the analysis version and input hashes;
- corrections create a new baseline revision rather than replacing old bytes;
- old reports remain available and are labeled historical or superseded;
- benchmark claims cite the exact baseline identifier and file location.

## Claim boundary

A canonical baseline may support claims about:

- exact API and export counts measured in that build;
- correctness agreement for the tested conditions;
- operation-specific timing on the recorded machine;
- observed scaling and planner behavior;
- reproducibility of archive generation and analysis.

It does not, without additional evidence, support claims of:

- universal superiority over MPFR;
- correctness over every input;
- stable public ABI;
- cross-machine performance generality;
- independent replication;
- causal affinity effects from sequential close and spread campaigns.

## Production-source preservation

A benchmark-infrastructure-only release must compare the complete `src/`
directory against the preceding numerical baseline. Any changed production
source file must be named and reviewed separately. A statement that numerical
kernels are unchanged is permitted only after this byte-level comparison.
