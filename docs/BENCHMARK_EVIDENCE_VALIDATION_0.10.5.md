# Benchmark evidence validation for 0.10.5

## Scope

This validation covers the benchmark-evidence and canonical-baseline block. It
checks the source release, benchmark JSON schema, raw-sample retention,
statistical analysis, deterministic packaging, and preservation of production
numerical source. It does not claim a new numerical algorithm or a new
performance result.

## Source validation

The source tree was configured with project warnings treated as errors and with
benchmarks enabled. The local environment provided the MPFR runtime library but
not the official development header, so the repository compatibility
declarations were used for local MPFR validation.

Results:

```text
Strict CMake build:  PASS
Fast tests:          31 / 31 passed
Extended tests:       7 / 7 passed
Smoke tests:         12 / 12 passed
Public release audit: PASS
```

The smoke inventory includes `benchmark_json_schema_smoke`, which executes the
public multiplication and cold-pi benchmark programs and verifies:

- schema version 2;
- operation identity;
- successful numerical correctness gate;
- positive finite timing samples;
- sample-count agreement;
- a reported median equal to the retained sample vector.

## Numerical-source preservation

A byte-level comparison was performed between `polyphase-float-0.10.4/src` and
`polyphase-float-0.10.5/src`.

```text
Old production files: 35
New production files: 35
Unchanged files:      35
Changed files:         0
Added files:           0
Removed files:         0
```

The per-file SHA-256 record is stored in
`docs/PRODUCTION_SOURCE_PRESERVATION_0.10.5.json` and is checked by the public
release audit.

## Evidence-pipeline behavior

The revised benchmark programs retain acquisition-order samples and refuse to
emit a verified record until their numerical comparison succeeds. The campaign
runner validates each record, attaches stable condition and campaign
identifiers, records executable and environment fingerprints, and prevents an
incompatible resume from mixing data in one output directory.

The analyzer gives each independently launched process equal weight. Its point
estimate is the median of process medians. Its intervals use a deterministic
hierarchical nonparametric bootstrap over process launches and their retained
samples. A one-process interval is labeled as within-process evidence only.

## Remaining boundary

- The official MPFR development header was not present in this local validation
  image. The existing official-header CI and the user's WSL validation remain
  the direct checks for that dependency path.
- The completed legacy ten-core campaigns retain condition medians but not all
  in-process samples. Their canonical report therefore does not invent
  confidence intervals.
- Independent external replication remains outstanding.
