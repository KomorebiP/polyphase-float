# Product and fused-operation planner

The planner separates the exact product graph from final floating-point
rounding. Multiplication, square, FMA, FMS, and reciprocal-square-root
refinement can therefore share product implementations without rounding an
intermediate product.

Version 0.10.7 centralizes AUTO selection in `src/product_planner.c`. The
planner resolves both an algorithm and an effective worker count. Full
multiplication uses one worker, rank 3 uses at most three, rank 5 uses at most
five, and the window path uses at most two workers for square and three for a
general product. This prevents a caller's larger worker request from creating
teams that the selected graph cannot use.

The distributable build uses a conservative portable policy. An optional
machine-specific profile can override exact operation, precision, limb-count,
and worker-count conditions when independent close and spread campaigns clear
the documented evidence gates. Explicit algorithm requests always bypass the
profile.

See `docs/PRODUCT_PLANNER_PROFILES.md` for profile generation, build,
validation, provenance, and portability rules. See `PRODUCT_PLANNER_REPORT.md`
for the earlier derivation and rank-comparison experiments.
