# Stream, Interchange, and Formatted Output

The independent library now provides the complete stream/raw I/O and formatted-output surface corresponding to the implementation inventory.

## Stream text I/O

```c
size_t ppf_out_str(FILE *stream, int base, size_t digits,
                   const ppf_t *value, ppf_rnd_t rounding,
                   ppf_context_t *context);

size_t ppf_inp_str(ppf_t *value, FILE *stream, int base,
                   ppf_rnd_t rounding, ppf_context_t *context);
```

A null stream means `stdout` for output and `stdin` for input. `ppf_out_str` uses the same arbitrary-base, correctly rounded digit generator as `ppf_get_str`. `ppf_inp_str` skips leading whitespace, reads one non-whitespace word, and applies the independent string parser.

## Portable interchange

```c
int ppf_fpif_export(FILE *stream, const ppf_t *value);
int ppf_fpif_import(ppf_t *value, FILE *stream,
                    ppf_context_t *context);
```

The independent interchange representation is named `PPFIF01`. It stores:

- an eight-byte format identifier
- value category and sign
- binary precision
- binary exponent
- the normalized significand in big-endian bytes
- a CRC-32 integrity check

The representation is endian-independent and preserves the sign of NaN. Import is transactional: malformed, truncated, or checksum-invalid input leaves the destination unchanged.

`PPFIF01` is intentionally an independent format. It is not claimed to be byte-compatible with GNU MPFR's experimental FPIF representation.

## Debug output

```c
void ppf_dump(const ppf_t *value);
void ppf_dump_file(FILE *stream, const ppf_t *value);
```

`ppf_dump` writes the independent object description to standard output. `ppf_dump_file` is the library-specific stream variant.

## Formatted output

The ten entry points are:

```c
ppf_fprintf   ppf_vfprintf
ppf_printf    ppf_vprintf
ppf_sprintf   ppf_vsprintf
ppf_snprintf  ppf_vsnprintf
ppf_asprintf  ppf_vasprintf
```

The parser accepts standard C conversions, GMP value types, the precision type `%P`, and independent floating values through `%R`.

### Independent floating conversions

```text
%Ra  %RA   hexadecimal floating output
%Rb        binary floating output
%Re  %RE   scientific decimal output
%Rf  %RF   fixed decimal output
%Rg  %RG   fixed or scientific decimal output
%Rn        store the number of bytes generated so far in a ppf_t
```

The `%R` type accepts these rounding selectors:

```text
N  nearest, ties to even
Z  toward zero
U  toward positive infinity
D  toward negative infinity
Y  away from zero
*  take a ppf_rnd_t argument immediately before the ppf_t argument
```

Width, precision, left/right alignment, zero padding, signs, alternate form, dynamic width and precision, special values, and exact `snprintf` return-length behavior are supported. The implementation preserves embedded null characters in allocated and stream output.

### Mixed argument lists

One call may combine ordinary C values, GMP values, precision values, and `ppf_t` values:

```c
ppf_printf("x=%+.12RNf  z=%#Zx  p=%Pu\n", &x, z, ppf_get_prec(&x));
```

Standard and GMP conversions are delegated to the platform C/GMP formatter after the format has been parsed. Independent floating conversions use the library's own correctly rounded digit and exact-rational conversion paths.

## Deliberate boundary

Positional `$` arguments are rejected. The baseline implementation supports the ordinary sequential variadic interface used by standard C, GMP, and MPFR-style formatting. Extremely large output requests that would exceed `INT_MAX` return `-1`, set the range-error flag, and set `errno` to `EOVERFLOW` when available.
