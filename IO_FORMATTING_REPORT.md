# Independent I/O and Formatting Block

The library now has a complete application-facing text and interchange layer.

## Added public functions

### Stream and raw I/O

```text
ppf_out_str
ppf_inp_str
ppf_fpif_export
ppf_fpif_import
ppf_dump
```

`ppf_dump_file` is an additional independent convenience function.

### Formatted output

```text
ppf_fprintf
ppf_vfprintf
ppf_printf
ppf_vprintf
ppf_sprintf
ppf_vsprintf
ppf_snprintf
ppf_vsnprintf
ppf_asprintf
ppf_vasprintf
```

## Architecture

All independent floating formatting ultimately uses the established correctly rounded conversion layer. There is no conversion through `mpfr_t` inside the production library.

The formatted-output engine has three paths:

1. Standard C arguments are rendered with the system formatter.
2. GMP arguments are rendered with GMP's formatter.
3. `%R` arguments are rendered from `ppf_t` through independent binary or decimal conversion.

A byte-oriented dynamic builder is used internally, so `%c` with value zero is retained correctly and return counts remain byte-accurate.

## Interchange format

The independent `PPFIF01` representation is canonical across endianness and limb width. It preserves precision, category, sign, exponent, and normalized significand. A CRC-32 detects corruption before the destination is committed.

This is a new independent interchange representation rather than a claim of byte compatibility with MPFR's experimental FPIF encoding.

## Validation summary

The focused block tests include:

- all ordinary stream-output rounding directions
- bases 2, 10, 16, and 37
- zero, signed zero, infinity, NaN, and regular values
- randomized stream-output comparisons
- partial stream input and byte-count behavior
- randomized interchange round trips from 2 through 257 bits
- corrupt-stream rejection with destination preservation
- 200 randomized finite binary values
- 35 independent floating format patterns per randomized value
- zero-precision hexadecimal normalization
- dynamic width, precision, and rounding arguments
- standard C, GMP, precision, and independent floating arguments in one format
- all ten formatted-output entry points
- `snprintf` truncation and required-length semantics
- standard and independent `%n`
- embedded null output

The complete inherited test suite also remains enabled.

## Performance boundary

The existing short-digit converter makes `out_str` competitive for fixed-size output from very high-precision inputs. Formatted output adds parsing, width, sign, and allocation overhead, so it is expected to remain somewhat slower than MPFR's mature integrated formatter in some rows.

The independent interchange format includes a checksum and a larger canonical header. Its round-trip timing should not be interpreted as a byte-for-byte comparison with MPFR FPIF.

## Final development benchmark

The final benchmark used nine repetitions at 10,000, 100,000, and 1,000,000 bits. Every text row was checked against MPFR before timing was retained.

| Bits | Operation | Polyphase | MPFR | MPFR / Polyphase |
|---:|---|---:|---:|---:|
| 10,000 | 128-digit `out_str` | 1.638 us | 1.235 us | 0.754x |
| 100,000 | 128-digit `out_str` | 1.856 us | 1.393 us | 0.751x |
| 1,000,000 | 128-digit `out_str` | 2.061 us | 1.595 us | 0.774x |
| 10,000 | 128-significant-digit `asprintf` | 1.760 us | 1.250 us | 0.710x |
| 100,000 | 128-significant-digit `asprintf` | 1.923 us | 1.475 us | 0.767x |
| 1,000,000 | 128-significant-digit `asprintf` | 2.083 us | 1.625 us | 0.780x |
| 1,000,000 | portable interchange round trip | 0.921 ms | 0.066 ms | 0.072x |

The two text operations are directly comparable. In these rows the independent implementation is roughly 29 to 41 percent slower than the installed MPFR formatter. The interchange row is not a byte-format comparison: `PPFIF01` adds a canonical header and CRC-32 and is intentionally not MPFR FPIF.

Direct limb serialization and table-driven CRC reduced the one-million-bit independent round trip from roughly 3.2 ms in the initial prototype to about 0.92 ms in the final measured build.
