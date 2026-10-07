# Polyphase Float 0.15.0 I/O allocation and commit contract

## Scope

This contract governs project-controlled allocation failure in the public text,
formatting, stream, and PPFIF interfaces. It does not claim control over an
allocation failure occurring inside GMP, the C library, or an operating-system
stream implementation.

The governed functions are:

```c
ppf_set_str
ppf_strtofr
ppf_init_set_str
ppf_get_str
ppf_free_str
ppf_out_str
ppf_inp_str
ppf_fpif_export
ppf_fpif_import
ppf_asprintf
ppf_vasprintf
ppf_snprintf
ppf_vsnprintf
ppf_sprintf
ppf_vsprintf
ppf_fprintf
ppf_vfprintf
ppf_printf
ppf_vprintf
```

## Numerical destinations

`ppf_set_str`, `ppf_strtofr`, `ppf_inp_str`, and `ppf_fpif_import` construct a
complete temporary value before replacing an initialized destination. If an
unrecovered project-controlled allocation failure occurs before commit:

1. the numerical destination remains byte-for-byte unchanged;
2. `PPF_FLAG_ALLOC` is raised in the resolved context;
3. every project-controlled temporary allocation is released;
4. the destination and context remain reusable.

`ppf_set_str` additionally validates that the full input, apart from trailing
white space, is a number before commit. `ppf_strtofr` preserves its prefix
parsing contract and reports the first unconsumed character through `endptr`.

`ppf_init_set_str` first creates a valid initialized object. If assignment then
fails, the object remains valid and clearable, but it does not preserve a
pre-existing numerical value because initialization created the destination.

## Returned strings

When `ppf_get_str` allocates its own output, the returned string retains a copy
of the allocator provenance used to create it. The caller must release the
string with:

```c
ppf_free_str(text);
```

The string may be released after the originating numerical context has been
cleared, provided the copied allocator callbacks and their opaque state remain
valid. Passing any pointer not returned by `ppf_get_str` to `ppf_free_str` is
outside the public contract.

The all-or-nothing rule is:

- success returns one complete string;
- failure returns `NULL` and transfers no partial string.

If the caller supplies the destination buffer to `ppf_get_str`, the caller is
responsible for providing sufficient storage as required by the public API.
The library does not know that buffer's capacity and cannot make an independent
bounds guarantee.

## Formatted output

All `ppf_*printf` functions first construct the complete formatted byte sequence
in a context-owned staging builder.

For `ppf_asprintf` and `ppf_vasprintf`:

- success assigns a complete process-allocated string to `*text`;
- failure leaves `*text == NULL`;
- the caller releases a successful output with `free`.

For `ppf_snprintf`, `ppf_vsnprintf`, `ppf_sprintf`, and `ppf_vsprintf`:

- an unrecovered staging allocation failure occurs before the caller buffer is
  modified;
- ordinary successful `snprintf` truncation remains distinct from allocation
  failure;
- `sprintf`-style functions retain the usual caller obligation to provide
  sufficient space.

The final process allocation used by `ppf_asprintf` and `ppf_vasprintf` is made
through `malloc`, because their established ownership contract requires release
with `free`. Failure of that final allocation raises `PPF_FLAG_ALLOC`.

The `%n` conversion is an externally visible side effect. A value already
stored by an earlier `%n` is not rolled back if a later conversion fails.

## Output streams

`ppf_out_str`, `ppf_fprintf`, `ppf_vfprintf`, `ppf_printf`, and `ppf_vprintf`
complete project-controlled staging before the first stream write.

Therefore an allocation failure before writing leaves the stream unchanged.
Once a write begins, the library does not promise rollback after a short write,
I/O error, signal, or failure inside the stream implementation.

## Input streams

`ppf_inp_str` preserves the numerical destination on a project-controlled
allocation failure. It may already have consumed bytes from the input stream.
The library does not promise to restore the original position of an arbitrary
stream.

## PPFIF interchange

`ppf_fpif_export` stages the complete PPFIF01 header, payload, and CRC before the
first write. Allocation failure before that point leaves the stream unchanged.
After writing begins, the general stream limitation above applies.

`ppf_fpif_import` validates and constructs a temporary value before swapping it
into the destination. Truncated, malformed, checksum-invalid, or allocation-
failed input does not expose a partially decoded numerical object. The stream
position may advance.

## Sticky recovery evidence

Some paths can recover from a failed preferred allocation by using an exact
fallback. A successful numerical or textual result may therefore coexist with
`PPF_FLAG_ALLOC`. The flag records that the preferred project-controlled path
encountered a resource failure. Applications that care about execution-path
health should inspect and clear the flag explicitly.

## Exclusions

Version 0.15.0 does not claim complete failure atomicity for:

- GMP-internal allocation;
- certified-constant packet trees;
- transcendental internal work arrays;
- special-function internal work arrays;
- worker metadata and selected cache growth;
- an I/O error after externally visible stream output begins;
- rollback of `%n` side effects;
- restoration of arbitrary input-stream position.
