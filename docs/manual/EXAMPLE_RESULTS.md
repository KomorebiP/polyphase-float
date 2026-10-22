# Executable example results

The complete programs in this manual were compiled as strict C11 with warnings treated as errors. Each program returned success and produced the expected output shown below.

Reproduce these checks from the source directory:

```sh
python3 tools/check_manual_examples.py --output ../manual-check --cc gcc clang
```

The full run requires GMP and the official MPFR development headers and libraries. Add `--core-only` to explicitly omit the MPFR bridge. The output directory must be new and outside the source tree.

Compilers used:

- gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- Ubuntu clang version 18.1.3 (1ubuntu1)

These examples validate their demonstrated cases. They supplement the library's broader qualification and numerical contracts.

## intro_first

Source: [intro_first.c](../../examples/manual/intro_first.c).

```text
2 * 3 = 6.00000e0
```

## intro_flags

Source: [intro_flags.c](../../examples/manual/intro_flags.c).

```text
1 / 3: INEXACT is set
exact assignment: INEXACT is still set
after clearing: no flags are set
1 / 0: positive infinity; DIVBY0 is set
0 / 0: NaN; NAN is set
```

## intro_precision

Source: [intro_precision.c](../../examples/manual/intro_precision.c).

```text
parsed at 8 bits     = 1.00097656250e-1
copied to 128 bits   = 1.00097656250e-1
parsed at 128 bits   = 1.00000000000e-1
changing the default leaves existing precisions unchanged
```

## intro_rounding

Source: [intro_rounding.c](../../examples/manual/intro_rounding.c).

```text
down    = 3.32031250e-1 (below)
nearest = 3.33984375e-1 (above)
up      = 3.33984375e-1 (above)
```

## numeric_constants

Source: [numeric_constants.c](../../examples/manual/numeric_constants.c).

```text
Pi: direct evaluation and exported interval agree at 80 bits.
The interval proves the value and its rounding direction.
```

## numeric_elementary

Source: [numeric_elementary.c](../../examples/manual/numeric_elementary.c).

```text
log of the rounded 1 + 2^-80 is zero at 53 bits.
log1p(2^-80) and expm1(2^-80) remain positive.
sinpi(1) is exactly zero; sin of rounded pi is not.
```

## numeric_import

Source: [numeric_import.c](../../examples/manual/numeric_import.c).

```text
Neither binary value is exactly 1/10.
The 256-bit decimal parse is closer to 1/10.
Widening the native import preserves its old value.
```

## numeric_ranges

Source: [numeric_ranges.c](../../examples/manual/numeric_ranges.c).

```text
Nearest quotient: +infinity; overflow and inexact.
Toward-zero quotient: 384; inexact only.
Exact subnormal emulation: underflow without inexact.
```

## numeric_refinement

Source: [numeric_refinement.c](../../examples/manual/numeric_refinement.c).

```text
scale 8: refine
scale 16: refine
scale 32: refine
scale 64: certified 53-bit sqrt(2), rounded upward
```

## numeric_special_domains

Source: [numeric_special_domains.c](../../examples/manual/numeric_special_domains.c).

```text
Gamma(5) = 24 exactly.
lgamma(-1/2) returns a finite log magnitude and sign -1.
lngamma(-1/2) reports a real-domain NaN.
Ai(501) exceeds the implementation domain: ERANGE, not ALLOC.
```

## systems_allocation

Source: [systems_allocation.c](../../examples/manual/systems_allocation.c).

```text
allocation failure preserved 37; retry produced 1.25
```

## systems_io

Source: [systems_io.c](../../examples/manual/systems_io.c).

```text
digits=12500 exponent=1
value=1.25
PPFIF round trip: value and precision preserved
```

## systems_mpfr_bridge

Source: [systems_mpfr_bridge.c](../../examples/manual/systems_mpfr_bridge.c).

```text
MPFR -> Polyphase -> MPFR: 10
```

## systems_threads

Source: [systems_threads.c](../../examples/manual/systems_threads.c).

```text
2 workers read one immutable input; both produced 49
```
