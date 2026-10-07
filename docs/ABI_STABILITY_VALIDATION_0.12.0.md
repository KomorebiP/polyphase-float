# Polyphase Float 0.12.0 ABI stability validation

## Purpose

Version 0.12.0 establishes the first mechanically governed C ABI baseline. It
preserves the public 0.11.0 Linux ELF interface and adds runtime ABI
introspection without changing numerical formulas.

## Compatibility boundary

The validated baseline is Linux x86-64 using the LP64 data model and 64-bit GMP
limbs. The compatibility tools reject or skip a layout claim when the active
machine model does not match the frozen record.

## Implemented controls

- normalized public-header contract generation;
- frozen 0.11.0 source-contract baseline;
- additive/breaking contract comparison;
- runtime structure-size, alignment, and offset probe;
- frozen 0.11.0 LP64/GMP64 layout baseline;
- exact shared-object export audit;
- ELF symbol-version maps;
- ABI epoch, revision, tag, contract hash, and size-tagged runtime record;
- unchanged 0.11.0 consumer binary execution against 0.12.0;
- CMake and Make ABI-audit targets;
- release-audit integration;
- dedicated CI workflow.

## Source-contract result

The 0.12.0 contract is compatible with the 0.11.0 baseline.

```text
Breaking differences:       0
Compatible additions:      13
Unchanged contract entries: 465
```

The additions consist of the ABI-introspection structure, its supporting
constants and enum values, and six new functions. Existing function
prototypes, public structures, enum values, and constants are unchanged.

## ELF export result

The core shared library contains 437 public `ppf_*` functions:

```text
431 inherited functions at PPF_0.11
  6 new ABI functions at PPF_0.12
```

The optional MPFR adapter retains its five `PPF_MPFR_0.11` symbols. No
production `mpfr_*` symbol is imported by the core library.

## Binary-consumer result

A consumer was compiled and linked against the actual 0.11.0 header and shared
library. The resulting executable records `libpolyphase_float.so.0` as its
needed library and was not recompiled for the second run.

It produced the same deterministic output with the 0.11.0 and 0.12.0 shared
libraries:

```text
ppf_t=88 context=88 exact=121932631112635380 pi=3.1415926535897931 plan=1/4/1
```

This validates the tested function calls, public object layouts, planner record,
and SONAME transition for that consumer. It is not a proof that every possible
third-party binary exercises no undocumented behavior.

## Runtime ABI record

`ppf_get_abi_info` reports:

- ABI epoch and revision;
- data-model widths and byte order;
- GMP limb width;
- conditional-format and thread-lifecycle feature bits;
- sizes and alignments of governed public structures;
- field offsets for `ppf_context_t` and `ppf_t`.

The function supports prefix-sized callers. An undersized but valid record is
filled through the supplied boundary and returns a truncation status.

## Numerical change boundary

The only production source added in this block is:

```text
src/abi.c
```

No existing production source file changed. The release does not alter:

- arithmetic or product identities;
- final rounding;
- roots or divisor plans;
- certified constants;
- elementary or special functions;
- product-planner decisions;
- allocation or thread-lifecycle behavior.

## Remaining boundary

The ABI commitment does not yet cover native Windows, macOS, ARM64,
big-endian, or 32-bit targets. Those platforms require their own layout and
calling-convention records before they can join the governed compatibility
matrix.
