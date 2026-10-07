# Hot-path speed refinement

This branch concentrates on the operations that were still consuming the most
runtime after the independent object model, arithmetic, utilities, roots, and
certified constants were established.

The production rule remains unchanged:

```text
independent native arithmetic or explicit failure
```

The independent shared library imports no `mpfr_*` symbol. MPFR is used only
by the optional adapter and by test and benchmark executables as an external
correctness reference.

## Retained changes

### Exact integer assignment and extraction

`ppf_set_z_2exp` now has a direct normal-range limb path. Exact integer input
no longer requires an avoidable intermediate copy before the common rounding
engine.

`ppf_get_z` and related integer rounding paths use a read-only GMP view of the
normalized `ppf_t` significand. This avoids exporting the complete
significand into a second large integer merely to round or shift it.

### Short arbitrary-base output

Short output requests no longer scale with the full source precision. The
source is enclosed by directed reduced-precision endpoints. Both endpoints
are converted, and the result is accepted only when they produce the same
radix string and exponent. Ambiguous cases increase the working precision or
use the exact path.

The endpoint-to-GMP conversion now writes the reduced `ppf_t` significand
directly into the GMP floating layout. One GMP conversion object is reused for
both endpoints.

This reduced a one-million-bit, 128-digit decimal request from roughly 5 ms in
the preceding exact implementation to about 2.5 microseconds in the final
benchmark. The operation remains about 1.8 times slower than MPFR because two
independent endpoint conversions are deliberately retained for certification.

### Remainder operations

`fmod` and IEEE remainder now use read-only views of the source significands.
The quotient and exact remainder are still calculated natively, but the
inputs are no longer copied into fresh full-width GMP integers first.

### General roots

The root family now uses `mpz_root`, because the previous root-with-remainder
operation calculated a large remainder that the interval proof never used.
The initial certificate margin was increased from 48 to 112 bits for ordinary
roots. This small bit increase avoids an occasional second complete
multi-million-bit root calculation.

### Reciprocal roots

Reciprocal roots now calculate the integer root first and then form a rigorous
reciprocal enclosure. The floor quotient for `root + 1` is derived from the
quotient and remainder for `root`, avoiding a second full division. This is
about 1.5 times faster than the preceding reciprocal-root implementation.

## Internal improvement over the preceding implementation

These measurements used the same development environment and comparable
inputs. They measure removal of the library's own overhead, not a comparison
with MPFR.

| Operation | Approximate internal speedup |
|---|---:|
| Exact integer assignment | 8.0x |
| Exact integer extraction | 2.29x |
| Rational assignment | 1.24x |
| Short decimal output | over 600x |
| Round to integer | 1.21x |
| `fmod` | 2.74x |
| IEEE remainder | 2.37x |
| Comparison | 1.83x |
| Cube root | 1.83x |
| Seventh root | 1.91x |
| Reciprocal square root | 1.55x |

## Final one-thread comparison

Every timed output was checked against MPFR before its row was retained. A
ratio above one favors Polyphase.

### One million bits

| Operation | Paired median speedup |
|---|---:|
| Exact integer assignment | 1.968x |
| Exact integer extraction | 2.269x |
| 128-digit decimal output | 0.552x |
| `fmod` | 1.134x |
| IEEE remainder | 1.380x |
| Cube root | 0.983x |
| Seventh root | 0.995x |
| Reciprocal square root | 0.682x |
| `hypot` | 1.014x |
| Division | 1.973x |

### Five million bits

| Operation | Paired median speedup |
|---|---:|
| Exact integer assignment | 1.679x |
| Exact integer extraction | 2.243x |
| 128-digit decimal output | 0.541x |
| `fmod` | 1.210x |
| IEEE remainder | 1.562x |
| Cube root | 0.965x |
| Seventh root | 1.000x |
| Reciprocal square root | 0.651x |
| `hypot` | 0.985x |
| Division | 2.125x |

The complete medians, MADs, bootstrap intervals, win-loss-tie counts, and raw
rows are under `results/hotpath_speedups`.

## Remaining performance boundaries

Two calls remain clearly slower in this benchmark.

### Short text output

The certified path performs two reduced-precision radix conversions. A
one-conversion certificate was tested, but its exact boundary arithmetic cost
more than the second conversion. A future win requires a lower-level radix
converter that emits a rounding certificate together with the digits.

### Reciprocal square root

Profiling at one million bits divided the current cost approximately as
follows:

```text
integer square root:  about 6.6 ms
exact quotient:       about 8.2 ms
correction:           below 0.1 ms
```

Small allocation changes cannot close this gap. The next serious attempt must
be a low-level inverse-square-root iteration with precision doubling, high
products, reusable scratch, and one exact final correction.

## Benchmark contract

The supplied benchmark uses:

- the same destination precision
- nearest-even rounding
- one worker for each library
- randomized method order
- correctness checks before timing rows are retained
- seven paired repetitions by default
- checkpointed raw results and automatic resume

Process startup, input construction, and post-timing verification are outside
the timed section.
