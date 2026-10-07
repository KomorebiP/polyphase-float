# Failed and conditional hot-path experiments

These experiments are retained because they define useful boundaries.

## One-conversion radix certificate

A single reduced-precision radix conversion was followed by exact arithmetic
on the returned digit interval. The certificate was correct, but the small
base-power and boundary calculations made it slower than converting the two
directed endpoints.

At one million bits, the experiment took about 2.95 microseconds per
128-digit result, compared with about 2.41 microseconds for the retained
endpoint method.

Result: rejected.

## Approximate GMP floating square root

A GMP floating approximation was corrected by exact integer comparisons. It
was slower than GMP's tuned integer floor-root operation.

Result: rejected.

## Approximate GMP reciprocal square root

A direct floating reciprocal-square-root approximation followed by exact
correction was also slower than the retained root-first enclosure.

Result: rejected.

## High-level Newton root iteration

Precision-doubling Newton iterations built from complete rounded `ppf_t`
operations were numerically correct. They created too many full-width
objects, rounded at every step, and moved more memory than the refinement
saved.

Result: rejected at the high-level object layer. A fused low-level Newton
kernel remains a valid future direction.

## Root with remainder

The root interval only needs the floor root and whether the root is exact.
Constructing the complete integer remainder was unnecessary. Replacing
`mpz_rootrem` with `mpz_root` was retained and produced a substantial speedup.

## Small ordinary-root guard

A 48-bit initial margin usually worked, but a difficult five-million-bit
seventh-root input required a second complete root calculation. Increasing
the initial margin to 112 bits costs only 64 extra root bits and avoids the
multi-million-bit repeat.

Result: larger ordinary-root margin retained. Reciprocal roots keep a smaller
separate margin because their cost is dominated by the root and quotient.
