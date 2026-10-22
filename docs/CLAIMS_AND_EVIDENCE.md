# Claims and evidence policy

A release statement should say what was established, on which source and
platform, and by which method. The following evidence levels answer different
questions; one does not automatically establish the next.

## Implemented surface

The supplied lowercase reference inventory has 349 direct or conditional
`ppf_*` counterparts. The core semantic ledger indexes 437 public functions,
including compiler-conditional interfaces. An inventory count establishes
surface coverage, not semantic equivalence with MPFR or equal maturity of all
paths.

## Executed tests

Tests compare results with exact checks, invariants, or an external oracle for
specified inputs. Reports should identify source hashes, platform, dependency
versions, input classes, precision, rounding mode, and resource conditions.
A function named in a test source has a direct test reference; that alone does
not establish exhaustive branch or input coverage.

The current allocation ledger records 192 transactional contracts, 63 reviewed
allocation-free contracts, 12 initialize-and-set lifecycles, two prefix-cleanup
contracts, and three safe-zero initializer contracts. There are no remaining
indexed allocation-evidence gaps. These classifications have different
preconditions and failure outcomes; see
[Allocation evidence](ALLOCATION_COMPLETION_0.20.0.md). They do not extend
recoverable project-allocation rollback to GMP, libc, or I/O side effects.

The [thread-ownership contract](THREAD_SAFETY.md) defines the supported sharing
and cleanup patterns. Tests for those patterns do not make concurrent mutation
of an unprotected object, context, or random state safe.

[Qualification](QUALIFICATION.md) produces current execution receipts. Record
sanitizer and fuzz duration and exclusions explicitly. In particular, the
default ASan/UBSan configuration disables LeakSanitizer. Historical test logs
remain evidence for their recorded source and environment.

## Mathematical arguments

A written argument may establish an enclosure, tail bound, exact
transformation, or rounding decision. The implementation and its input
assumptions must be traceable to that argument. Passing an MPFR comparison
does not itself prove a path for every representable input.

The [special-function review](BRANCH_REVIEW_0.20.0.md) states its selected
branches, arguments, and assumptions. The [release scope](COMPLETION_SCOPE_0.20.0.md)
and semantic ledger retain 105 bounded implementation paths and six
compiler-conditional interfaces. A documented implementation limit is not a
mathematical domain restriction on the underlying function.

## Compatibility evidence

ABI epoch 1 governs the Linux x86-64 ELF LP64/GMP64 baseline, including public layouts,
exports, symbol versions, and `libpolyphase_float.so.0`. Source-contract,
runtime-layout, and old-binary execution checks establish different parts of
that commitment. See [ABI policy](ABI_POLICY.md). Polyphase Float is not a
binary replacement for MPFR, and the governed ABI does not imply compatibility
on other data models or operating systems.

## Independent reproduction and maturity

Independent reproduction rebuilds the exact source and reproduces the claim
outside the original development environment using retained commands and data.
Changing compilers on one host is useful local variation, not external
replication. Review and reproduction claims must identify their source and
scope; a report's title or a test executable's name does not establish
independence.

A production-maturity claim additionally needs a maintained support policy,
sustained safety testing, independent review, and validation across its declared
support matrix. Version 0.20.0 is a bounded pre-1.0 release. The outstanding
requirements for 1.0 are recorded in the [roadmap](../ROADMAP.md).

## Performance claims

Benchmark results are specific to the operation, inputs, implementation path,
and machine. Retain wins and losses. Do not infer universal speed superiority
from selected rows or compare worker counts as though they used equal resources.

A public performance statement should identify:

1. operation and implementation path;
2. precision, operand class, and rounding mode;
3. worker count and affinity policy;
4. cold or warm cache state and the timed boundary;
5. compiler and dependency builds;
6. in-process and independent-process counts, estimator, and uncertainty method;
7. retained timing samples and correctness checks;
8. source, executable, environment, and run-configuration fingerprints;
9. raw-data and analysis-manifest locations;
10. any independent reproduction and its exact scope.

See [Benchmark evidence](BENCHMARK_EVIDENCE_PIPELINE.md) for the retained-sample
workflow. Machine-specific historical reports remain historical until their
claims are reproduced on the intended release and environment.
