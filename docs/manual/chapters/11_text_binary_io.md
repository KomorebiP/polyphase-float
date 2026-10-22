# Text, formatting, and binary interchange

Choose an output representation according to what the next reader needs. A
person usually needs a short decimal result with a stated precision. A parser
needs an unambiguous token and a checked conversion. A saved numerical object
needs its value, precision, and special-value information preserved. These are
different tasks, and the library provides separate interfaces for them.

| Task | Interface | What to check |
|---|---|---|
| Read one complete string | `ppf_set_str` | Zero means success; a nonzero result means failure |
| Read a number at the beginning of a string | `ppf_strtofr` | Error sentinel, end pointer, and any flags of interest |
| Obtain radix digits and a separate exponent | `ppf_get_str` | Non-null returned pointer; release allocated output with `ppf_free_str` |
| Produce a presentation string | `ppf_snprintf` or `ppf_asprintf` | Negative result means failure; check truncation separately |
| Read or write one text token on a stream | `ppf_inp_str`, `ppf_out_str` | Nonzero byte count, then the stream's error state when relevant |
| Save or restore a numerical object | `ppf_fpif_export`, `ppf_fpif_import` | Zero means success |

## Reading numbers deliberately

Use `ppf_set_str` for a configuration field that must contain exactly one
number. It accepts leading and trailing white space, but rejects an unconsumed
suffix. For example, `"1.25"` is a complete decimal input, while `"1.25 volts"`
requires either application-level parsing or the prefix interface. An empty or
white-space-only string is not a successful complete conversion. The
destination must already be initialized.

```c
if (ppf_set_str(&value, "1.25", 10, PPF_RNDN, &ctx) != 0) {
    /* Report an invalid or unsuccessful conversion. */
}
```

The return value here is a status, not the ternary rounding relation returned
by arithmetic. Successful parsing can round the input to the destination's
precision. Inspect `PPF_FLAG_INEXACT` when the application needs to distinguish
exact conversion from rounding, clearing the relevant context flag before the
operation if earlier work must not affect that decision.

Use `ppf_strtofr` when a larger grammar follows the number. On return, compare
the end pointer with the original input pointer to detect no conversion, then
validate the remaining text. A successful prefix does not validate a unit,
delimiter, or trailing field. Check `PPF_TERNARY_ERROR` separately before using
the result. A pointer into the input remains valid only while that input
buffer remains valid.

Positive input bases range from 2 through 62. Base zero requests prefix
detection for binary and hexadecimal forms and otherwise selects decimal.
The parser recognizes `@` as a radix-exponent marker in every base, `e` or `E`
in bases at most 10, and `p` or `P` as a binary-exponent marker in bases 2 and
16. These distinctions matter: a hexadecimal digit may itself be the letter
`e`, so treating every `e` as an exponent marker would be ambiguous. Use an
explicit base for a persistent text protocol, and define its special-value
policy rather than accepting NaN or infinity accidentally.

Short input does not necessarily mean cheap input. A small exponent token can
request a large exact radix power. The parser has construction limits and
range checks, but an application accepting untrusted text should also bound
input length, requested precision, and its own permitted exponent range. A
resource rejection is a conversion failure to handle, not permission to use a
partially computed value.

## Digits and the separate exponent

`ppf_get_str` returns significand digits without a radix point. For an ordinary
finite value, interpret the result as a fraction beginning immediately before
the first digit, multiplied by the base raised to the returned exponent. The
sign, when present, precedes the digits. Thus the five decimal digits `12500`
and exponent `1` describe `0.12500 × 10`, or `1.25`. They do not describe the
integer 12,500.

The `digits` argument requests the number of significant radix digits. Zero
asks the library to choose a count from the object's binary precision;
`ppf_get_str_ndigits` exposes that sizing calculation. This choice is useful
for general output, but a protocol should still specify its base and rounding
policy. If exact binary text is required, base 2 with enough digits to retain
the stored significand avoids an additional decimal rounding step. For a
complete saved object, use the binary interchange interface below.

With `buffer == NULL`, the library allocates the string and retains the
allocator provenance needed to release it. Call `ppf_free_str` exactly once.
Do not pass that allocation to `free`. Conversely, a buffer supplied by the
caller remains caller-owned and must not be passed to `ppf_free_str` merely
because `ppf_get_str` wrote into it. The caller-buffer form has no capacity
argument; allocated output is usually the easier choice unless storage bounds
are already established.

Special values have token representations rather than an ordinary
significand/exponent pair. Handle their categories explicitly when consuming
the digits API. Do not interpret an exponent variable left from an earlier
call as the exponent of NaN or infinity.

## Formatting for people

The `ppf_*printf` family accepts ordinary C conversions, GMP conversions, and
Polyphase floating conversions beginning with `%R`. Pass the address of the
initialized `ppf_t`, not a copied structure or a C floating value. For example,
`"%.2RNf"` prints two digits after the decimal point, using nearest rounding.
The `N` belongs to the formatting conversion; it is not a change to the
object's stored precision or to the context's default rounding mode.

| Conversion | Typical purpose |
|---|---|
| `%Rf`, `%RF` | A fixed number of decimal places |
| `%Re`, `%RE` | Scientific decimal notation |
| `%Rg`, `%RG` | A compact decimal presentation |
| `%Ra`, `%RA` | Hexadecimal floating notation |
| `%Rb` | Binary floating notation |
| `%Pu` | A `ppf_prec_t` value as an unsigned quantity |

After `R`, the selectors `N`, `Z`, `U`, `D`, and `Y` request nearest, toward
zero, toward positive infinity, toward negative infinity, and away from zero,
respectively. A `*` selector consumes a rounding-mode argument immediately
before the floating-object pointer. Ordinary dynamic field width and precision
consume their own integer arguments in the usual order. Prefer a literal,
simple format until those argument positions are familiar.

Choose `ppf_snprintf` for fixed-capacity buffers. Its nonnegative result is the
length that would have been produced, excluding the terminating null byte. A
result greater than or equal to the supplied capacity means truncation; it is
not an allocation error. Test for a negative result before converting that
result to an unsigned type. Choose `ppf_asprintf` when the required length is
not known in advance. Its successful allocation must be released with ordinary
`free`, and failure leaves its output pointer null.

| Allocation source | Required release |
|---|---|
| `ppf_get_str(NULL, ...)` | `ppf_free_str(text)` |
| `ppf_asprintf` or `ppf_vasprintf` | `free(text)` |
| Caller-supplied array | No library release; follow the array's own lifetime |

Formatting interfaces without an explicit context use the calling thread's
default context for their internal diagnostics. Inspecting an unrelated
explicit context will not explain their failure. Keep format strings under
application control: pass user text through `"%s"`, never as the format itself.
Conversions such as `%n` and `%Rn` write through pointers. Their earlier side
effects are not rolled back if a later conversion fails. Positional `$`
arguments are rejected. Large field widths can demand substantial work even
when the final destination buffer is small, so bound user-selected widths and
precisions before formatting.

## Streams, locales, and commit boundaries

`ppf_inp_str` skips initial white space and reads a non-white-space token.
`ppf_out_str` writes a formatted numeric token. A null input stream selects
standard input; a null output stream selects standard output. Applications
should normally pass their stream explicitly so an accidental null pointer
does not redirect data to a terminal.

Do not confuse numerical rollback with stream rollback. Failed input may have
consumed bytes while leaving the destination unchanged. Output staging can
fail before any write, but once writing begins a short write or an I/O error
can leave a prefix in the stream. Use application-level record framing,
temporary files, and a checked file replacement when an entire saved document
must be committed atomically. Check flushing and closing too: buffered output
can report an error after a successful formatting call.

The text stream formatter consults the active locale's decimal point. The
number parser recognizes the library's numeric syntax, including a dot radix
point. Keep machine-readable text in the C locale with a defined grammar, or
use PPFIF. Avoid changing process locale while other threads format data.

## PPFIF records

The binary representation is called `PPFIF01`. It records value category,
sign, binary precision, binary exponent, significand bytes in a defined byte
order, and a CRC-32 check. It is independent of the in-memory limb layout and
of host byte order. It is not MPFR's experimental FPIF representation, and a
raw dump of `ppf_t` is neither a PPFIF record nor a portable file.

Import restores the encoded precision as well as the value. It does not round
the record into the destination's previous precision. If an application needs
a particular working precision or range, import into a temporary object,
validate application-specific limits, and perform a separate rounded
assignment into the working destination. Category policies also belong to the
application: a valid NaN record can still be inappropriate for a price or a
physical measurement.

The default importer bounds the encoded payload and the rounded-up
significand storage independently at 64 MiB each. This also prevents a special
record with no payload from requesting unlimited storage through a claimed
precision. The private build macro `PPF_FPIF_MAX_IMPORT_BYTES` can change that
positive construction bound. It is not a per-call memory budget, a limit on
the total file size, or a guarantee of peak process memory. Oversized resource
requests return `-1`, raise `PPF_FLAG_ERANGE`, and preserve the destination.
Malformed, truncated, or checksum-invalid records likewise do not publish a
partially decoded object. Not every malformed record necessarily raises the
same flag; always check the return value.

A CRC detects accidental corruption; it does not authenticate a sender or
protect against deliberate modification. Apply authentication and transport
limits at the surrounding protocol layer when they are needed.

The complete example
[systems_io.c](../../../examples/manual/systems_io.c) prints the digits and
formatted value of `1.25`, then exports and imports a PPFIF record through a
temporary stream. It deliberately initializes the receiving object at a
different precision and checks that import restores the stored precision.
Its cleanup shows the two different string-release rules in one place.
