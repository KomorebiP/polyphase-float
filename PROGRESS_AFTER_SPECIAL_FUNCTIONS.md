# Progress After the Special-Function Block

The independent library now implements the complete special-function and constant family from the 349-symbol inventory.

## Coverage

Direct counterparts:

\[
265/349=75.93\%.
\]

Public `ppf_*` declarations and shared-library exports:

\[
338/338.
\]

## Families now complete

```text
Initialization and precision
Combined initialization and assignment
Comparison and classification
Logarithms, exponentials, and powers
Trigonometric functions
Hyperbolic functions
Special functions and constants
Integer rounding and remainder
Exponent range and exception flags
```

The portable assignment and conversion families remain complete apart from the six compiler-dependent binary128 and decimal interfaces.

## Remaining inventory

The library now lacks 84 direct counterparts. Most remaining work is no longer new mathematical transcendental theory. It is concentrated in:

- scalar arithmetic variants and two-product fused calls;
- stream and formatted I/O;
- random distributions;
- rounding-control helpers;
- build and version queries;
- memory and cache compatibility calls;
- MPF compatibility aliases;
- custom storage interfaces;
- conditional binary128 and decimal conversions.

## Engineering assessment

```text
Independent mathematical coverage:          about 85-90%
Literal MPFR-style public function surface: 75.93%
Mature, broadly tuned replacement effort:  about 45-55%
```

The mature-replacement estimate remains much lower than the literal symbol count because performance tuning, platform coverage, ABI stability, fuzzing, documentation, licensing, and external replication are still substantial projects.

## Explicit performance note

The special-function implementations are correctness-first reference kernels and are presently much slower than MPFR. The requested return to speed work is recorded in `SPEED_RETURN_BACKLOG.md`, including both the new special-function priorities and the previously deferred inverse-angle/dense-trigonometric work.

## Next coverage block

The next coherent completion block is the remaining integration surface:

1. missing scalar arithmetic variants and `fmma`/`fmms`;
2. rounding helpers such as `can_round`;
3. stream, raw, and formatted I/O;
4. random distributions and build queries;
5. memory/cache controls, MPF aliases, and custom storage.

That block would move the library from a broad mathematical implementation toward a more complete application-facing replacement surface.
