# Hot-path experiments

- `benchmark_root_vs_rootrem.c` measures the retained floor-root optimization.
- `benchmark_reciprocal_root_order.c` measures the retained root-first
  reciprocal enclosure.
- `benchmark_sqrt_candidate.c` records a rejected GMP floating square-root
  candidate.
- `benchmark_recsqrt_direct.c` records a rejected direct floating
  reciprocal-square-root candidate.
- `rejected_newton_roots.c` records the rejected high-level Newton family.

The failed and conditional results are summarized in
`../FAILED_HOTPATH_EXPERIMENTS.md`.

## Deep reciprocal-root and radix experiments

The retained reciprocal-root implementation is in `src/roots.c`. The experiments in this directory record the paths tested before it was selected:

- full-product fixed-point Newton iteration
- truncated residual Newton variants
- three-channel and five-product multiplication graphs
- direct and root-first reciprocal-root orderings
- short-radix component measurements

The `guard_variants/` subdirectory contains alternative proof margins used to establish a conservative portable setting. These files are not linked into the production library.
