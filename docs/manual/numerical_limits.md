# Numerical limits and admission policies

Arbitrary precision makes the precision a parameter. It does not remove finite
storage, bounded certification, or the need to validate an argument before
constructing a large intermediate. This chapter explains how to treat those
limits in an application. The source-level inventory in `docs/NUMERICAL_LIMITS.md`
records the individual policies and the available boundary evidence.

## Range, domain, and work are different constraints

A mathematical domain restriction concerns whether a real-valued answer exists
under the operation's definition. A configured exponent range concerns which
answers can be represented in the current context. A work limit bounds a
particular implementation route. These restrictions can produce different
status values, flags, and destinations. Do not infer one from another.

A shortcut can decline an input while a general evaluator remains available.
An evaluator can also exhaust its finite certification budget and return a
numerical error. Raising the destination precision may make a later attempt
more expensive without settling the original uncertainty. Check the actual
result rather than treating a larger precision as a universal remedy.

| Path | Current policy | Application consequence |
|---|---|---|
| Parsed textual exponents | Direct power construction has an absolute exponent budget of 2,000,000. Larger cases require a justified range classification or may be rejected. | Validate input sizes and check parse status. |
| Ordinary periodic trigonometric reduction | The general path can reject a request needing more than 1,000,000 extra reduction bits. | Prefer exact scaled-grid interfaces when the input is naturally specified in that form. |
| Finite Airy Ai input | The admitted absolute input is at most 500, compared exactly. | Check the domain before requesting work outside it. |
| General integer-order Bessel paths | Finite order magnitude is bounded by 100,000. | Do not interpret rejection as a proof that the mathematical function is undefined. |
| PPFIF import | The default maximum is 64 MiB for both payload and limb-rounded significand storage. | Keep the default for untrusted data unless the application deliberately chooses another positive cap. |
| General dyadic certificate construction | Endpoint alignment is limited to 1,000,000 extra bits by default. | An unusable enclosure can cause refinement or numerical rejection. |
| Exponential-minus-one construction | The same extra-bit cap bounds an intermediate endpoint's exponent gap from one before subtraction. | A compact enormous input need not be expanded into an enormous exact intermediate. |
| Negative-parameter incomplete Gamma at finite positive second argument | Recurrence admission requires the first argument to be at least -100,000. Separate special-value paths have their own rules. | Test the admission condition before relying on this recurrence path. |
| Negative-integer zeta | Large negative even integers are recognized directly as exact zeros. The small odd-integer Bernoulli shortcut is admitted before integer expansion. | A shortcut threshold is not a claim that all other zeta arguments are outside the mathematical domain. |

The default import limit can be changed through the documented
`PPF_FPIF_MAX_IMPORT_BYTES` build configuration. The dyadic extra-bit budget is
an alignment policy, not a global ceiling on destination precision. A series
term count is likewise not the same quantity as an input exponent or a byte
allocation limit.

## Attempts and fallback routes

The implementation uses distinct budgets for distinct algorithms. General
transcendental and special-function certification, root refinement, interval
arithmetic, constant builders, text conversion, and fast shortcuts do not all
share a single retry count. Individual series and recurrences have their own
iteration and admission checks. The source inventory identifies these
separately instead of presenting one number as a library-wide limit.

Many of those numbers control efficiency or a particular route. A fast path
may decline and fall back. A builder can stop without a certificate. A final
evaluator may reject after bounded attempts. Only a proven enclosure and a
successful settlement justify a certified returned result. Counting attempts,
or observing that a last term is small, does not supply that proof.

The numerical-arguments reference in `docs/NUMERICAL_ARGUMENTS.md` connects
reviewed paths to their enclosure or remainder arguments and records remaining
obligations. Its distinction between a written argument and finite execution
evidence matters: both are useful, and neither should be silently substituted
for the other.

## Handling a numerical rejection

For an operation whose contract uses the numerical ternary convention:

1. Check for `PPF_TERNARY_ERROR` before reading its sign.
2. Inspect the operation's flags and documented output state.
3. If the operation committed NaN on certification or admission failure, do not
   keep using the old destination as if it were still present.
4. Decide whether to choose another supported representation, change a
   justified precision or range parameter, report the unsupported request, or
   use another algorithm with its own checked contract.

This sequence does not turn every error into the same case. Invalid arguments,
an unsupported resource request, and a recoverable project-allocation failure
can follow different paths. Transactional allocation rollback preserves the
prior numerical destination on an unrecovered project-allocation failure, but
sticky flags may change. GMP allocations, I/O side effects, and application
callbacks remain outside that rollback guarantee.

## Designing application limits

Give the application its own input, precision, time, and output-size budgets.
These should reflect its purpose and the resources actually available. Check
string lengths and exponent syntax before accepting a large batch. Cap import
bytes and total record counts. Estimate the total working set before assigning
one large object to every worker. Treat output formatting as work too: a small
stored value can still be asked to produce a very large number of digits.

When retaining a failure report, record the function, exact input encoding,
destination precision, rounding mode, context range, flags, and return value.
Keep the source version and build features with that record. An exact binary
or hexadecimal input is more useful than a shortened decimal display when
reproducing a rounding boundary.

## Interpreting validation

The example suite checks its complete programs and expected outputs. Numerical
regressions exercise selected domains and resource boundaries. Sanitizers
inspect the executions they observe. Differential comparisons test agreement
on their enumerated cases. These results support concrete engineering claims.
They do not imply support for an untested platform, unbounded precision, or
every admitted input.

A stable public contract can state its finite limits plainly. It should also
state which mathematical and execution obligations remain open. The release
readiness record carries those obligations separately from this user's guide.
