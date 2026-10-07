# Failed and conditional transcendental speed experiments

The speed branch retained unsuccessful ideas so their boundaries remain
reproducible.

## AGM logarithm

A directed arithmetic-geometric mean logarithm was implemented from

\[
\log s \approx \frac{\pi}{2\operatorname{AGM}(1,4/s)}.
\]

Both full floating intervals and a midpoint-radius fixed-point AGM were tested.
The method has attractive logarithmic iteration count, but the available
independent square-root and interval-division kernels made it slower than the
root-reduced atanh path over the measured range. It remains an experiment, not
the automatic backend.

## Newton logarithm

A Newton solve for \(e^y=x\) was correct but slower because every iteration
required a complete certified exponential plus full-width correction work.
It did not become competitive before the exponential kernel was already much
faster than the logarithm it was intended to replace.

## Pure bit-burst logarithm at moderate precision

Bit-burst reduction reduced the series degree but added decomposition,
reconstruction, and certificate overhead. It lost below the high-precision
crossover. It is retained only above the measured threshold, where it begins
to outperform the ordinary root/Horner path.

## Binary-splitting atanh prototype

An exact binary-splitting atanh prototype increased temporary integer sizes and
performed more reconstruction work than the fixed-point Horner loop. It was
not retained in production.

## Universal block size for atanh

Block sizes of 4, 8, 16, and larger were tested. No one block size won across
precision. Small precision retains a short blocked path; the large-precision
backend uses Horner or bit-burst according to the measured region.

## Universal bit-burst exponential

Bit-burst exponential was excellent for sparse dyadic arguments such as 0.125,
but slower on dense reduced arguments created by multiplication with log(2) or
log(10). The production planner therefore uses a density test and switches to
blocked Taylor below the dense crossover.

## Direct exp2 reconstruction

A direct base-two reconstruction avoided one conceptual natural-exponential
layer but did not reduce the certified fixed-point work enough to beat the
shared exp kernel. The generic scaled-constant path remains in production.

## AGM fallback in the floating interval layer

A second AGM implementation was tested in the old `ppf_t` interval evaluator.
It improved that evaluator at some small precisions but remained slower than
the fixed-point backend and complicated the fallback path. It was removed from
the production source.

## Main conclusion

The winning improvement was architectural rather than a single classical
formula:

\[
\text{full floating endpoints per step}
\longrightarrow
\text{one fixed binary scale with integer endpoint arithmetic}.
\]

Algorithm selection then matters by operand shape. Sparse and dense
transcendental reductions should not be forced through the same schedule.
