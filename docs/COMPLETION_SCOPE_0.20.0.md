# Polyphase Float 0.20.0 release scope

## Release contract

The supported baseline is Linux x86-64 ELF, LP64, and 64-bit GMP limbs, with C11,
POSIX threads and official GMP development files. MPFR is an optional adapter
and test oracle. OpenMP is optional. The production core remains independent
of MPFR. ABI epoch 1 and `libpolyphase_float.so.0` are retained.

The release integrates bounded PPFIF import, exact Airy domain admission, and
floating-setter validation. It also classifies the previous ledger's 79
allocation-evidence entries. The numerical implementation and validation are governed by
the input and resource contracts below.

## Allocation evidence

All 437 public core functions have semantic records. The 79 earlier entries
are now classified as 63 reviewed allocation-free valid-input operations,
12 verified initialize-and-set lifecycles, two verified variadic prefix-cleanup
contracts and two additional transactional wrappers. This brings the total
transactional class to 192. There are no remaining indexed allocation-evidence
gaps. The three previously recorded safe-zero initializer contracts remain.

An allocation-free classification assumes a valid initialized destination,
existing context and caller-provided capacity as stated in its record. It does
not describe GMP-internal allocations. Initialize-and-set failure retains an
initialized object that can be cleared. Variadic initialization clears only
the visited prefix and leaves unvisited objects untouched. An unrecovered
allocation failure in a transactional operation preserves the prior numerical output, while sticky flags may change.
An exact successful fallback may retain `PPF_FLAG_ALLOC` diagnostically.

[Allocation evidence](ALLOCATION_COMPLETION_0.20.0.md) records the reviewed
source paths and failure campaign. GMP's process-level allocations, libc allocations,
and I/O side effects remain outside recoverable project-allocation rollback. These are
explicit contract domains rather than concealed allocation guarantees.

## Numerical and resource limits

| Path | Supported policy |
| --- | --- |
| Text exponent magnitude above 2,000,000 | A result that cannot be classified against the configured binary range may be rejected. |
| Ordinary periodic trig reduction | More than 1,000,000 extra reduction bits may be rejected. Exact scaled-grid paths are separate. |
| Finite Airy Ai | Absolute input at most 500, checked by exact arbitrary-precision comparison. |
| Integer Bessel order | Finite order magnitude at most 100,000 on the general path. |
| PPFIF import | At most 64 MiB each of payload and limb-rounded significand storage by default, including special-value records. |
| Dyadic interval certificates | General transcendental and special-function evaluators limit endpoint alignment to 1,000,000 extra bits by default; an unusable certificate causes retry or numerical rejection. |
| Certification attempts | General transcendental, special and adaptive random paths use 12 attempts; trig and constant paths use 16. Internal construction and iteration limits are stated in their individual source contracts. |

The dyadic alignment limit bounds the extra shifts used to place interval
endpoints on a common binary scale; it is not a global precision ceiling.
Nonfinite or reversed endpoints cannot form such a certificate.
For `ppf_expm1`, `ppf_exp2m1`, and `ppf_exp10m1`, the same 1,000,000-bit cap
applies to an intermediate endpoint's exponent gap from one before subtraction.
Integer-power shortcuts have a precision-dependent construction budget;
larger cases use the bounded evaluator and may be rejected.

A finite iteration budget can reject a difficult input without establishing
that a whole interval is unsupported. In the general special-function evaluator,
exhausting the 12 certification attempts returns `PPF_TERNARY_ERROR`, raises
`PPF_FLAG_ERANGE`, and sets the numerical destination to NaN. The boundary test
forces this path in `PPF_RNDN`, `PPF_RNDZ`, `PPF_RNDU`, `PPF_RNDD`, and
`PPF_RNDA`. This numerical-rejection contract is distinct from destination
preservation on an unrecovered project-allocation failure in a transactional operation. Check the individual
operation's contract rather than treating every error as allocation rollback.

`PPF_FPIF_MAX_IMPORT_BYTES` can configure a different positive import byte cap.
See [PPFIF resource limits](FPIF_RESOURCE_CONTRACT.md) for ownership, admission,
and destination-preservation behavior on import rejection.

The semantic ledger retains 105 bounded implementation paths and six
compiler-conditional interfaces. A complete symbol inventory does not imply
unbounded numerical domains or identical optional formats on every compiler.

## Proof and execution evidence

[Branch review](BRANCH_REVIEW_0.20.0.md) maps the reviewed Gamma and digamma
reflection, Li2 transformations, Bessel/Airy intervals, and retry behavior to
their mathematical
arguments. Its assumptions are explicit. This bounded source review and the
differential MPFR comparison matrix are separate evidence types. Neither is a
whole-library formal proof.

[Qualification](QUALIFICATION.md) gives one offline command for clean GCC,
Clang, and OpenMP builds, complete configured tests, installed consumers, ABI/export checks,
ASan/UBSan hardening and timed seeded fuzz campaigns. The runner binds the
source snapshot and records commands, durations, dependencies and failures.
It must retain actual failure or omitted-check status. LeakSanitizer is
explicitly disabled by default and is distinct from exact project allocator
balance checks. Compiler variation on one host is not external replication.

## Remaining promotion obligations

Version 0.20.0 can be distributed under this contract. A later 1.0 promotion
requires independent correctness reproduction and mathematical review of the
exact candidate and intended claims, together with the obligations in the
[roadmap](../ROADMAP.md). Longer fuzz campaigns and platform expansion have
their own evidence requirements. Windows, macOS, ARM64, big-endian, and 32-bit
compatibility are outside the current supported contract.
