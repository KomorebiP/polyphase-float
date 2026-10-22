# Numerical edge and interval audit

The deep audit found failures that the existing structured special-function
tests did not cover. The changes preserve the public API and keep MPFR outside
the production implementation. MPFR supplies the independent test oracle.

## Reproduced failures

Unmodified-library executions reproduced the Gamma-at-1, Beta-at-(1,1),
log-Gamma-at-NaN, and negative-order Bessel-at-zero failures. The additional
log-Gamma pole and infinity differences below were identified from the source
and independently queried MPFR behavior. All listed cases were then checked
against MPFR on the corrected library.

| Input or certificate | Earlier behavior | Corrected behavior |
|---|---|---|
| `gamma(1)`, 2-bit nearest rounding | `PPF_TERNARY_ERROR`, NaN, range-error flag | Exact 1, ternary 0, no flags |
| `beta(1,1)`, 53-bit nearest rounding | Precision retry exhaustion, `PPF_TERNARY_ERROR`, NaN | Exact 1, ternary 0, no flags |
| `lgamma(NaN)` | Gamma sign output 0 | Gamma sign output 1, matching MPFR |
| `lgamma(-inf)` | NaN and Gamma sign 0 | Positive infinity and Gamma sign -1 |
| Log-Gamma at signed zero or a negative integer | Missing division-by-zero flag; wrong negative-integer Gamma sign | Division-by-zero flag; negative-integer Gamma sign 1 |
| `lngamma(-inf)` | NaN | Positive infinity |
| `jn(-3,+0)` | Positive zero | Negative zero |
| Gamma subintervals used to initialize Airy coefficients | `Gamma(lower)` used as a lower bound and `Gamma(upper)` as an upper bound, despite Gamma decreasing at 1/3 and 2/3 | Endpoint order reversed over the explicitly checked interval `(0,1]` |

The Airy coefficient defect was tested independently before the fix. At
53 bits, the two computed endpoints for `Gamma(1/3)` collapsed to
`0x2.adceea72905e2p+0`, below the independently computed value. The endpoints
for `Gamma(2/3)` collapsed to `0x1.5aa77928c3679p+0`, above the independently
computed value. Neither pair enclosed its intended exact rational argument.
The earlier final Airy output tests could still pass because other interval
operations widened the result.

## Additional certificate corrections

Source review also found enclosure constructions that were weaker than the
documented mathematical argument:

* A positive geometric remainder bound widens the upper endpoint only. Ei,
  Li2, and the Hasse zeta numerator now retain the partial sum as the lower
  endpoint rather than adding the whole remainder majorant to both endpoints.
* The factorial denominator in the Bessel J series now rounds downward and
  upward separately. Rounding both denominator endpoints to nearest did not
  certify the exact factorial.
* Even powers on a negative input interval reverse their argument endpoints.
* Gamma and digamma reflection now preserve the interval for `1-x`.
* Li2 reductions propagate directed transformed arguments through the
  monotone reduced region.
* General Beta sums and incomplete-Gamma recurrence arguments must be exact
  at the current working precision. A nonzero ternary result triggers the
  existing precision retry instead of treating a rounded scalar as exact.
* Negative-s zeta reflection requires an exact scalar sine argument and an
  exact `1-s` transformation. It retries at higher precision when necessary.

These source-level defects are distinct from a reproduced incorrectly rounded
final public result. The audit reproduced the failures in the table above;
it did not find a final-value mismatch for every deficient enclosure route.

## Exact results and practical boundaries

Gamma uses an exact factorial route for positive integers at most 100000.
Log-Gamma at 1 and 2 returns exact positive zero. Beta uses the reciprocal
identity when either argument is 1, and an exact factorial ratio for positive
integer arguments whose sum is at most 100000. Signed zeros and infinities in
the tested Beta domain have explicit handling.

Finite nonzero Bessel evaluations now require `abs(order) <= 100000`.
Larger orders return `PPF_TERNARY_ERROR`, NaN, and `PPF_FLAG_ERANGE` before
attempting a factorial or a recurrence proportional to that order. NaN,
infinity, and zero cases remain supported for the full `long` order domain.
Order magnitude and parity no longer negate `LONG_MIN` in signed arithmetic.

## Reproducible checks

`tests/test_special_edge_audit.c` performs 1460 MPFR comparisons over five
rounding modes. It checks values, signed zeros, ternary direction, flags, and
the Gamma sign output. Destination precisions include 2, 24, 53, and 113 bits.
The suite also checks 512-bit inputs at `1 +/- 2^-400`,
`-1 +/- 2^-400`, and `-2 +/- 2^-400` against 53-bit outputs, exercises aliases,
and checks eight unsupported finite Bessel-order calls.

`tests/test_special_interval_audit.c` checks 20 Gamma rational certificates
against independently directed 1024-bit MPFR enclosures and three positive
remainder certificates, including aliases.

Run the new checks and existing special-function suites with:

```sh
ctest --test-dir build/audit-validation \
  -R '^special_(functions|values|faithful|constants|alias|random|large|edge_audit|interval_audit)$' \
  --output-on-failure -j2
```

The interval test requires the static library build because it tests hidden
internal helpers directly. The public edge test works with the ordinary
optional MPFR adapter.

Passing this bounded campaign is evidence over these inputs and execution
paths. It is not exhaustive validation over every precision, exponent,
special-function argument, or allocation behavior, and it does not establish
universal resource-independent completion.
