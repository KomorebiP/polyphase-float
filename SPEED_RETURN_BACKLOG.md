# Speed Work to Resume After the Coverage Milestone

The special-function block was completed before returning to performance work, as requested. This file is the explicit reminder and work order for that return.

## Previously deferred trigonometric work

The following items remain active from the inverse-angle and dense-phase speed branches:

1. purpose-built dense sine/cosine and tangent kernels;
2. cached high-precision angle tables and sparse corrections;
3. high-product or middle-product operations for reconstruction;
4. fused square-root/half-angle refinement for `atan` and `atan2`;
5. improved pi-scaled reduction and dense phase evaluation;
6. shared inverse-hyperbolic kernels.

## Special-function speed priorities

### Shared fixed-point interval engine

Replace repeated full `ppf_t` interval operations in the special-function loops with common-scale integer endpoints, following the successful logarithm/exponential and trigonometric speed branches.

### Gamma, log-gamma, and digamma

- cache Bernoulli coefficients and planner thresholds;
- reduce full-object recurrence traffic;
- compare direct Stirling, Spouge, and Lanczos regions;
- fuse reflection and sign handling;
- create shared `gamma`/`lgamma` evaluation where both are needed.

### Upper incomplete gamma

Use a region planner:

- power series for small `x`;
- continued fraction for large `x`;
- uniform or Temme-type asymptotics near `x≈a`;
- recurrence only when it lowers total work.

### Zeta

- binary-split Euler--Maclaurin corrections;
- cached Bernoulli blocks;
- faster globally convergent sums near the critical strip;
- shared functional-equation constants;
- exact negative-integer fast paths without rebuilding Bernoulli tables.

### erf and erfc

- a true paired kernel;
- asymptotic `erfc` for large positive inputs;
- scaled complements that avoid cancellation;
- fixed-point series blocks near zero.

### Bessel functions

- Miller backward recurrence for suitable order/argument regions;
- asymptotic oscillatory forms for large arguments;
- shared `J0/J1` and `Y0/Y1` calculations;
- phase-reduced sine/cosine reuse;
- order planners instead of universal forward recurrence.

### Airy Ai

- asymptotic positive-argument decay;
- oscillatory negative-argument asymptotics;
- overlap checks between power-series and asymptotic regions;
- remove the present `abs(x)<=500` practical limit only after certification.

### Constants and caches

- reduce cached-copy overhead;
- precompute and reuse Bernoulli data;
- fixed-point Euler and Catalan builders;
- matched-worker cache and cold-computation benchmarks.

## Benchmark rules

Every future speed claim must report separately:

- one worker versus one worker;
- internally threaded single-call latency versus ordinary MPFR;
- equal-worker outer-batch throughput;
- cache-cold and cache-warm cases;
- exact value and ternary checks before timing rows are retained.
