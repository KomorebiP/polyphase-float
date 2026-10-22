# Polyphase Float 0.19.0 completion scope

## Release boundary

Version 0.19.0 is a tested pre-1.0 release that repairs defects identified by
the fresh deep audit. The governed runtime and ABI baseline remains Linux
ELF, LP64, and 64-bit GMP limbs. A passing suite establishes the tested
contracts and input matrices on the recorded compiler and dependency build.
The release does not imply that every numerical path, platform, or allocation
domain has been exhausted.

The next version boundary should be a 1.0 release candidate only after the
remaining concrete gates below have been met. A readiness percentage is not
used because the remaining items differ in scope and cannot be reduced to a
reproducible scalar score.

## Reading the evidence ledger

The semantic ledger indexes 437 core functions. Its allocation-failure
classification now uses explicit endpoint allowlists for each fault-injection
suite. Calls used to prepare inputs, inspect flags, or clean up objects no
longer inherit a transactional claim merely because their names occur in the
same test source.

The corrected ledger records 190 endpoints with verified transactional
evidence and 79 allocation-atomicity entries with incomplete evidence. It
records 104 numerical endpoints using a bounded backend, plus six conditional
binary128 or decimal endpoints. Eight metadata or cleanup functions previously
inherited a numerical limit from their header section and now carry no such
limit. These counts describe evidence classifications. They are not a count of
known broken functions or a measure of universal numerical correctness.

The new fail-at-each-project-allocation tests close the recorded entries for
`ppf_prec_round`, `ppf_fmod_ui`, `ppf_set_dyadic_interval`, `ppf_urandomb`,
`ppf_urandom`, `ppf_nrandom`, `ppf_grandom`, `ppf_erandom`, and `ppf_reldiff`.
Precision, scalar remainder, and direct dyadic-interval
tests cover safe retries and preservation of existing object state. The scalar
remainder tests include ordinary destinations, supported in-place calls, and
application-owned external significands.
Direct interval tests also preserve the caller's storage on a successful
commit and leave the ternary output untouched on errors or undecided intervals.

## Allocation domains and lifecycle

The recoverable allocation contract governs allocations controlled by the
project's context or object allocator. GMP manages the storage used by
`mpz_t`, `mpq_t`, `mpf_t`, and random-state internals through its separate,
process-level allocator. Failure injection into a `ppf_context_t` cannot
establish a guarantee about those allocations. GMP allocation exhaustion is
outside the current recoverable-failure contract, as are libc stream-buffer,
kernel, and external-backend failures.

An existing initialized numerical destination must remain unchanged on an
unrecovered project allocation failure for endpoints covered by the
transactional contract. Sticky context flags may change. A successful exact
fallback may retain `PPF_FLAG_ALLOC` as a diagnostic.

Object initialization has a different lifecycle. If significand allocation in
`ppf_init2_ctx` fails, the output is zeroed and can safely be cleared. There is
no prior initialized value to preserve. Invalid-argument rejection occurs
before that initialization contract begins. Initialize-and-set shortcuts
need dedicated failure tests covering both the initialization step and any
subsequent setter failure.

The variadic `ppf_inits` and `ppf_inits2` implementations clean up the objects
already initialized if a later step fails. They also use a libc `realloc`
array to track that prefix. Objects after the failure point have not been
visited. A whole-list safe-zero guarantee would require additional staging or
a revised, explicit lifecycle contract and corresponding tests. Both batch
initializers are therefore classified as partial lifecycle contracts. These
two evidence gaps correct an overbroad ledger label and do not indicate a new
implementation regression.

Several remaining ledger entries are evidence gaps for operations with no
project allocation on valid inputs. Examples include `ppf_set_zero`,
`ppf_set_inf`, `ppf_set_nan`, `ppf_swap`, `ppf_clears`, `ppf_limbs_write`,
`ppf_commit_regular`, `ppf_set_exp`, and `ppf_custom_move`. They should be
closed through a reviewed source classification and focused lifecycle tests.
Adding transaction buffers to allocation-free operations would create new
failure paths without improving their existing behavior.

## Numerical limits

| Limit | Consequence | Completion work |
| --- | --- | --- |
| Parsed exponent magnitude above 2,000,000 | The parser may reject a representable value when its result cannot be classified as outside the configured binary range. | Test the rejection boundary and all rounding modes. Keep the limitation explicit or add a certified large-exponent parser. |
| General periodic trig reduction requires more than 1,000,000 extra bits | Some finite large arguments are rejected by the ordinary radian reduction path. Exact scaled-grid cases can use separate paths. | Add a scalable certified reduction backend or explicitly retain this supported-domain boundary. |
| Airy Ai finite input with absolute value above 500 | The current public implementation rejects these finite inputs. | Add certified asymptotic/domain-extension paths before claiming full finite-real coverage. |
| Integer Bessel order magnitude above 100,000 | The current Bessel order guard rejects those orders. | Retain an explicit order boundary or implement a scalable certified large-order method. |
| Precision retries and iteration budgets | Certification may report an error for difficult inputs before exhausting memory. | Test forced-budget exhaustion and verify the promised return, flags, and destination behavior. A finite budget is a resource policy, not proof of an unsupported input interval. |

The backend policies include 12 attempts for general transcendental, special,
and adaptive random-distribution rounding, 16 for trigonometric and constant
certification, and series/reduction iteration limits. Several gamma and zeta
construction steps have smaller recurrence or working-size guards. These must
be documented at the exact endpoint and path that uses them rather than
assigned automatically to every family control function.

## Gates for a 1.0 release candidate

1. Resolve each allocation evidence entry with an endpoint-specific test or a
   reviewed source/domain classification. Govern multi-output commits,
   lifecycle failure, and external storage explicitly. Test at every project
   allocation observed in each governed scenario, including aliases and
   successful retries.
2. State the supported numerical domain for each public endpoint. Test hard
   input boundaries, all rounding modes, narrow exponent ranges, exceptional
   values, and deliberate certification-budget exhaustion. Add domain
   extensions where the intended 1.0 contract requires them.
3. Retain reproducible clean builds, installation consumers, ABI checks,
   differential numerical tests, sanitizer runs, and fuzz seeds. Native
   Windows, macOS, ARM64, big-endian, and 32-bit data models require their own
   execution evidence before their compatibility is claimed.
4. Run sustained fuzzing and independent replication. Record elapsed duration,
   seeds, platform, dependency versions, exact commands, and failures. A short
   smoke run cannot substitute for the planned sustained campaign.
5. Finish branch-level proof-to-source review where certified mathematical
   enclosures are claimed. Separate enclosure validity, endpoint rounding,
   failure behavior, and performance evidence in the release report.

The supported-platform defect fixes and packaging checks can be released as
0.19.0 once the recorded fresh checks pass. The remaining gates are explicit
work items for the release candidate and subsequent 1.0 decision.
