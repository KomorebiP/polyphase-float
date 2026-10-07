# Product-planner profiles

## Purpose

Polyphase product crossovers are not universal constants. Rank 3 and rank 5
split the same exact product into different child sizes, and those child sizes
can cross GMP's architecture-specific multiplication thresholds. The fastest
choice can therefore change with operation, precision, requested worker count,
compiler, GMP build, processor topology, and thread placement.

Version 0.10.7 keeps the conservative portable planner as the default and adds
an opt-in build-time profile layer. A profile never changes arithmetic
semantics. It selects an existing exact product implementation before the
ordinary certified final rounding step.

## Public introspection

```c
ppf_product_plan_t plan;
ppf_plan_product(&plan, &a, &b, PPF_PRODUCT_OP_MULTIPLY, workers);
```

The plan reports:

- the resolved algorithm;
- the requested worker count;
- the effective worker count after the algorithm-specific cap;
- whether a generated profile rule matched.

`ppf_product_profile_name()` and `ppf_product_profile_sha256()` identify the
profile compiled into the library. The distributable default reports
`portable`.

## Portable fallback

The portable fallback remains deliberately conservative:

- full multiplication for small or unbalanced products;
- rank 3 for sufficiently large balanced multiplication, square, and FMA;
- rank 5 for sufficiently large squares with at least five requested workers;
- rank 3 for sufficiently large reciprocal-square-root refinement.

Explicit `PPF_PRODUCT_FULL`, `PPF_PRODUCT_KARATSUBA3`,
`PPF_PRODUCT_TOOM3`, and `PPF_PRODUCT_WINDOW` requests bypass profile lookup.
Only `PPF_PRODUCT_AUTO` uses a profile.

## Evidence gates

`tools/generate_product_profile.py` accepts rank-planner CSV evidence and emits
a C header only after all configured gates pass. By default, a rule requires:

1. four independent benchmark processes per binding;
2. both close and spread binding campaigns;
3. the same winner under every binding;
4. at least a five percent median speed advantage;
5. a 95 percent bootstrap interval wholly on the winning side of parity;
6. the winner in at least 75 percent of independent processes;
7. one machine fingerprint and one software fingerprint;
8. a result different from the portable fallback.

A condition that fails any gate receives no rule. The portable planner remains
active for that condition.

## Generate a profile

After building `benchmark_product_planner`, run:

```sh
python3 scripts/calibrate_product_profile.py \
  --root . \
  --binary build/planner-calibration/benchmark_product_planner \
  --output results/product_profile_calibration \
  --bits 1000000,5000000,10000000 \
  --workers 3,4,5,6,8,9,10 \
  --bindings close,spread \
  --process-repetitions 4 \
  --repetitions 9 \
  --fresh
```

The calibration runner launches each condition in an independent benchmark
process, counterbalances binding order by process round, randomizes the
condition order deterministically, retains every rank measurement, and writes:

```text
results/product_profile_calibration/
  close/rank_raw.csv
  spread/rank_raw.csv
  generated/ppf_product_profile_build.h
  generated/product_profile_report.json
  CALIBRATION_REPORT.md
  environment.json
  execution_schedule.json
  run_config.json
```


A rerun with the same arguments and without `--fresh` reuses the persisted
campaign ID, verifies the source binary and normalized machine/software
fingerprints, skips every completed record, and regenerates the combined profile
from the unchanged evidence. A changed grid or environment is rejected rather
than silently mixed into the campaign.

## Build with a generated profile

```sh
cmake -S . -B build/tuned \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_BENCHMARKS=ON \
  -DPPF_PRODUCT_PROFILE_HEADER="$PWD/results/product_profile_calibration/generated/ppf_product_profile_build.h"
cmake --build build/tuned -j
```

The header is copied into the build tree. It is not installed as public API and
does not modify the source-tree portable profile.

For the Make research harness:

```sh
make clean
make -j PPF_PRODUCT_PROFILE_HEADER=/absolute/path/ppf_product_profile_build.h
```

## Validate a generated profile

```sh
python3 scripts/validate_product_profile.py \
  --profile-report results/product_profile_calibration/generated/product_profile_report.json \
  --inspector build/tuned/inspect_product_plan \
  --benchmark build/tuned/benchmark_product_planner \
  --output results/product_profile_calibration/validation
```

Validation checks every compiled rule through the public planner-introspection
API, reruns correctness-gated AUTO and forced-rank measurements under close and
spread binding, and rejects excessive AUTO dispatch overhead.

## Portability boundary

A generated profile is not a portable release default. Its report binds it to
one machine fingerprint, one software fingerprint, the exact raw evidence
hashes, and the stated statistical gates. Recalibration is required after a
material CPU, compiler, GMP, OpenMP runtime, or topology change.
