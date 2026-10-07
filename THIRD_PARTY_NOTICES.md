# Third-party notices

Polyphase Float does not vendor the implementation source of GMP or MPFR.
They are external dependencies installed by the user or build environment.

## GNU Multiple Precision Arithmetic Library (GMP)

GMP is required by the independent production library for multiprecision
integer and limb arithmetic. Its headers are part of the public build
interface because `polyphase_float.h` exposes GMP types in selected APIs.
GMP retains its own copyright and license terms.

## GNU MPFR

MPFR is optional. It is used only by:

- the separate interoperability adapter;
- differential correctness tests;
- matched comparison benchmarks.

The production `libpolyphase_float` target does not link to or import MPFR.
The project includes `compat/mpfr_minimal.h` solely as a test-build fallback
when the MPFR shared library is present but its development header is absent.
Public and release builds should install and use the official MPFR development
header.

## OpenMP and system libraries

The currently validated implementation requires a C11 compiler, POSIX threads,
GMP, and the platform math library. OpenMP is optional and is used only when
enabled or discovered by the build system. These components retain their own
licenses.

## Contribution rule

Do not add copied code, generated tables, datasets, or documentation from an
external source unless its provenance and redistribution terms are recorded
here and are compatible with `LGPL-3.0-or-later`.
