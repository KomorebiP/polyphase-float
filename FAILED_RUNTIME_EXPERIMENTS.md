# Runtime-Block Boundaries and Rejected Shortcuts

## MPFR arithmetic delegation

Rejected. The runtime block does not call MPFR to implement any production operation. MPFR remains only in the optional adapter and external differential tests.

## Hardware-only normal and exponential sampling

Rejected. A `double` Box-Muller or inverse-log sample would cap accuracy at hardware precision. The retained generators sample exact binary-prefix intervals and refine until the destination rounding is certified.

## Treating faithful `can_round` as ordinary rounding

Rejected. The faithful target has a distinct contract, including a directed-source exact-target exception. The implementation normalizes source directions, handles that exception, and then applies the target decision test. All six-by-six rounding combinations are differentially exercised.

## Library-owned custom significands

Rejected. Custom storage is explicitly application-owned. The library will not free or silently reallocate it.

## Unconditional decimal/binary128 ABI

Rejected. These C types are compiler-conditional. The API and build queries expose them only when the toolchain provides the corresponding formats.

## Claiming random bitstream identity for every distribution

Rejected. Uniform generators are tested for MPFR-compatible state/value behavior. Normal and exponential generators provide the same mathematical distributions with certified rounding, but intentionally do not claim MPFR bitstream identity.
