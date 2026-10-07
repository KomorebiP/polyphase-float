# API coverage

Direct lowercase inventory coverage: **349/349**.

The independent shared library exports **431** `ppf_*` functions on the
validated GCC configurations. The public header declares the same 431 exported
functions. `ppf_round_nearest_away` is an
additional function-like macro.

The increase from 429 to 431 in 0.11.0 is the calling-thread lifecycle API:

```text
ppf_thread_cleanup
ppf_buildopt_thread_cleanup_p
```

| Family | Implemented | Inventory | Coverage |
|---|---:|---:|---:|
| Initialization and precision | 10 | 10 | 100.0% |
| Assignment and parsing | 25 | 25 | 100.0% |
| Combined initialization and assignment | 9 | 9 | 100.0% |
| Conversion and fit tests | 28 | 28 | 100.0% |
| Arithmetic | 54 | 54 | 100.0% |
| Comparison and classification | 26 | 26 | 100.0% |
| Logarithms, exponentials and powers | 24 | 24 | 100.0% |
| Trigonometric functions | 25 | 25 | 100.0% |
| Hyperbolic functions | 10 | 10 | 100.0% |
| Special functions and constants | 24 | 24 | 100.0% |
| Stream and raw I/O | 5 | 5 | 100.0% |
| Formatted output | 10 | 10 | 100.0% |
| Integer rounding and remainder | 19 | 19 | 100.0% |
| Rounding control and certification | 7 | 7 | 100.0% |
| Neighbors, min/max, random, exponent/sign and build information | 23 | 23 | 100.0% |
| Exponent range and exception flags | 34 | 34 | 100.0% |
| Memory and caches | 4 | 4 | 100.0% |
| MPF compatibility | 5 | 5 | 100.0% |
| Custom storage interface | 7 | 7 | 100.0% |

The three allocation-flag convenience functions added in 0.10.9 remain part of
the 431-function public surface. The two 0.11.0 lifecycle functions are
Polyphase-specific additions rather than entries in the lowercase compatibility
inventory.

## Conditional baseline

Binary128 and decimal64 or decimal128 entries are compiled when the compiler
and relevant optional interface expose the corresponding types. Build-query
functions report availability at runtime.

## Distinction

Literal surface completion does not imply that every algorithm has the same
maturity, performance, resource limits, or cross-platform history as GNU MPFR.
It means every lowercase symbol in the supplied inventory has an independent
`ppf_*` counterpart or conditional counterpart.
