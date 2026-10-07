# Failed and Conditional Trigonometric Speed Experiments

The following variants were implemented and tested. They are retained as boundaries rather than presented as successes.

## Dense exact binary splitting for the reduced series

A dense binary-splitting tree was tested for sine and arctangent. At the measured 5,000- and 20,000-bit cases, evaluation, tree construction, and exact reconstruction cost more than the retained blocked fixed-point recurrence.

**Decision:** not selected by the production planner.

## Universal large arctangent blocks

Blocks of 8, 16, 32, and 64 terms were tested across the full precision range. Large blocks improved high-precision calls but increased power construction and memory traffic at smaller precision.

**Decision:** retain an adaptive 4/16/32 policy rather than one universal block size.

## Fixed-point inverse hyperbolic rewrite

Direct fixed-point forms based on `atanh`, square roots, and quotient enclosures were correct in focused cases, but they became substantially slower at larger precision because each call still required expensive root and division work before logarithmic certification.

**Decision:** do not dispatch the public inverse hyperbolic functions through this prototype. The existing independent log/root path remains the production route.

## Full endpoint double-angle reconstruction

Propagating lower and upper sine/cosine endpoints independently required four full-size products per double-angle level.

**Decision:** replaced by midpoint-radius reconstruction requiring two dominant products per level and an explicit radius bound.

## Two-endpoint half-angle roots

The first arctangent half-angle implementation squared and rooted both enclosure endpoints separately.

**Decision:** replaced by a midpoint-radius transform with one full square and one integer square root.

## Exact rational phase reduction for `sinpi` and related calls

Converting an exact binary input into a fully canonical GMP rational and then multiplying floating interval objects by `pi` was correct but dominated the scaled-angle runtime.

**Decision:** use direct integer-dyadic phase reduction and one common-scale multiplication by the certified `pi` interval.

## Fixed paired hyperbolic evaluation for arbitrary huge arguments

The fixed-scale paired kernel can require an impractically wide integer scale when the input exponent is large.

**Decision:** gate the fixed kernel to ordinary magnitudes and retain the existing exact range/saturation shortcuts for huge inputs.

## Maximum halving or maximum task count everywhere

Aggressive halving and maximum parallel task exposure produced nonmonotone results because GMP changes multiplication algorithms at its own size thresholds.

**Decision:** keep measured, conservative thresholds and leave target-specific tuning separate from the portable defaults.
