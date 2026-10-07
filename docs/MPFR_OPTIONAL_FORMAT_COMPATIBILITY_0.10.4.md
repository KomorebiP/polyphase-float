# MPFR Optional-Format Compatibility in 0.10.4

## Scope

This patch corrects the validation boundary between Polyphase Float and an
installed official MPFR development header. It does not change the production
`ppf_*` implementation.

## Failure mode

The 0.10.3 conditional-format differential test assumed that including
`mpfr.h` always exposed binary128 and decimal floating conversion types and
functions. The official header instead exposes those declarations only after
the caller requests the corresponding optional interface before inclusion.
The old fallback header was more permissive and therefore concealed the
portability error in environments without the official development header.

A separate benchmark passed an `mpfr_srcptr` to `mpfr_fpif_export`. The official
MPFR prototype accepts `mpfr_ptr`, so warnings-as-errors correctly rejected the
const-discarding call.

## Corrected contract

The conditional differential test now:

1. includes the Polyphase public header to establish the compiler-supported
   conditional types;
2. requests MPFR binary128 declarations with `MPFR_WANT_FLOAT128`;
3. maps MPFR's configurable `mpfr_float128` token to `ppf_float128_t`;
4. requests decimal declarations with `MPFR_WANT_DECIMAL_FLOATS`;
5. uses `ppf_decimal64_t` and `ppf_decimal128_t` instead of nonstandard MPFR
   typedef names;
6. compiles each test family only when CMake confirms that the declarations and
   installed library symbols are usable together.

The test-only minimal header follows the same opt-in structure. This keeps
fallback-header builds useful without allowing them to accept source that the
official header would reject.

The MPFR FPIF benchmark now accepts a mutable MPFR source object at the helper
boundary, matching the official `mpfr_fpif_export(FILE *, mpfr_ptr)` prototype.
The benchmark does not claim that export mutates the value. It merely respects
the library's declared C interface.

## Detection model

`cmake/PolyphaseMPFROptionalFormats.cmake` distinguishes three conditions:

- the compiler provides the corresponding binary128 or decimal types;
- the official MPFR header exposes the requested declarations;
- the selected MPFR library resolves the matching conversion symbols.

With an official header, CMake performs complete compile-and-link probes. With
the test-only minimal header, it verifies the required library symbols and
uses the compiler feature macros from the Polyphase public header.

The Make path compiles, links, and executes the corresponding probe programs in
`tools/probes/`. This verifies the compiler types, declarations, symbols, and
MPFR build-option queries before it compiles the conditional differential test.

## Regression boundary

The release audit checks that:

- the fallback header does not reintroduce `mpfr_decimal64` or
  `mpfr_decimal128` typedef aliases;
- binary128 and decimal declarations remain behind their required opt-ins;
- the conditional test defines those opt-ins before the adapter header;
- CMake retains compile-and-link probes for the optional APIs;
- Make retains compile-link-run probes for the optional APIs;
- the FPIF declaration and benchmark helper both use a mutable MPFR source.

## Unchanged components

All files under `src/` are unchanged from version 0.10.3. Therefore this patch
does not alter numerical values, rounding decisions, exception flags,
performance kernels, planner behavior, or public object layout.
