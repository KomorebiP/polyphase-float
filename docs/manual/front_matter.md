# Reading this manual {.unnumbered}

Polyphase Float is a C library for computations whose precision, rounding, and
resource use need to be explicit. It stores binary floating-point values at a
precision chosen by the application and provides arithmetic, elementary and
selected special functions, constants, conversion, text formatting, and binary
interchange. Its core uses GMP for exact integer machinery and does not call
MPFR. An optional adapter moves values between the two libraries.

The first part of this book develops a working application from initialization
through checked results and cleanup. The middle chapters explain numerical
workflows, storage, threads, and integration. The final part contains the
numerical resource policies, every core function declaration, an alphabetical
symbol index, and a glossary. You can read straight through or use the function
reference beside an existing program.

Begin with the first-program chapter if the library is already installed. Read
the rounding and error chapter before interpreting numerical return values.
Read the memory and overlap chapter before replacing inputs in place. If a
calculation returns an error, use the troubleshooting chapter and the numerical
limits rather than assuming that more precision is always the remedy.

The source package includes the complete programs in `examples/manual/`.
Short excerpts in the prose are teaching aids. Build and run the complete file
when you want its full setup, checks, and cleanup. Each example has a named
source file and an expected result that you can compare with your own run.

## Scope and conventions

The qualified baseline is Linux x86-64 ELF with the LP64 data model and 64-bit
GMP limbs. Linux under WSL follows that same target contract. Native Windows,
macOS, ARM64, big-endian, and 32-bit targets require separate qualification.
OpenMP and the MPFR adapter are optional. Portable-looking source code alone
does not establish support for every platform on which it can be compiled.

The manual describes the governed core API and ABI epoch 1. The source package's
version macros and release record identify the particular library build.
Precision is measured in bits unless decimal digits are stated explicitly.
An equation describes an exact mathematical value; a stored `ppf_t` describes
its finite binary representation. These are different objects, and much of
reliable numerical programming consists of keeping the distinction visible.

The library has finite domain and resource policies. A successful certified
path and a bounded numerical rejection are different outcomes. A complete API
inventory is useful for navigation, but is not a whole-library formal proof.
The numerical-limits appendix separates documented behavior from remaining
review and boundary-evidence obligations.

## License and attribution

The library and accompanying source are distributed under LGPL-3.0-or-later.
See `LICENSE.md`, `LICENSE`, `COPYING`, and `THIRD_PARTY_NOTICES.md` in the source
package for the applicable terms and third-party notices. The release author
uses the name Komorebi P.
