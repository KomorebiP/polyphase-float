# Product-planner experiments that did not become universal defaults

## Universal rank 5

Rank 5 was faster in several important rows, especially large squares and
selected five-worker multiplication and FMA cases. It also lost decisively in
neighboring rows. The crossover was nonmonotone because the five child products
move through GMP's internal multiplication regimes at different sizes.

Result: rank 5 remains a first-class explicit algorithm and a tuner choice, not
a universal portable default.

## Rank 5 with fewer workers treated as rank 3

An early implementation silently replaced forced rank 5 with rank 3 when fewer
than five workers were available. That made benchmark labels misleading.

Result: forced rank 5 now always evaluates the five-product graph and schedules
those tasks over the requested worker count.

## Automatic rank 5 inside reciprocal square root

Some rows favored rank 5, but one- and five-million-bit crossovers were unstable.
A clear five-worker gain appeared at ten million bits.

Result: portable automatic reciprocal root remains rank 3. The tuner may select
rank 5 for a measured machine and precision.

## Certified low-low omission as the default product

The convolution-window calculation is rigorous and can settle correct rounding
without forming the low-low product. Endpoint construction and certification
usually consumed the saved arithmetic on the tested system.

Result: retained as an explicit experiment and future high-product foundation,
not an automatic path.

## Equal-worker multiplication and FMA as broad wins

Internally threaded calls often finished much faster than one ordinary MPFR
call. When both libraries received the same cores through independent outer
jobs, multiplication and FMA were generally near parity and sometimes favored
MPFR.

Result: latency and equal-worker throughput are reported separately.

## Benchmark-wide unrelated setup

The matched-worker executable originally constructed eight multi-million-bit
dot-product inputs even when timing multiplication or FMA. This did not enter
the timed region, but it made tuning needlessly slow and could obscure stalled
runs.

Result: method-specific setup now initializes only the objects required by the
selected operation.
