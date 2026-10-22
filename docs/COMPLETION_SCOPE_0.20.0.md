# Polyphase Float 0.20.0 completion scope

## Candidate contract

The supported baseline is Linux ELF, LP64 and 64-bit GMP limbs, with C11,
POSIX threads and official GMP development files. MPFR is an optional adapter
and test oracle. OpenMP is optional. The production core remains independent
of MPFR. ABI epoch 1 and libpolyphase_float.so.0 are retained.

The release candidate integrates three concrete repairs and closes the
previous ledger's 79 allocation-evidence entries. The numerical implementation
and validation are governed by their declared input and resource contracts.
There is no universal all-input or production-maturity claim.

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
the visited prefix and leaves unvisited objects untouched. Transactional
failure preserves the prior numerical output, while sticky flags may change.
An exact successful fallback may retain PPF_FLAG_ALLOC diagnostically.

ALLOCATION_COMPLETION_0.20.0.md records the reviewed source paths and failure
campaign. GMP's process-level allocations, libc allocations and IO side
effects remain outside recoverable project-allocation rollback. These are
explicit contract domains rather than concealed allocation guarantees.

## Numerical and resource limits

| Path | Supported policy |
| --- | --- |
| Text exponent magnitude above 2,000,000 | A result that cannot be classified against the configured binary range may be rejected. |
| Ordinary periodic trig reduction | More than 1,000,000 extra reduction bits may be rejected. Exact scaled-grid paths are separate. |
| Finite Airy Ai | Absolute input at most 500, checked by exact arbitrary-precision comparison. |
| Integer Bessel order | Finite order magnitude at most 100,000 on the general path. |
| PPFIF import | At most 64 MiB each of payload and limb-rounded significand storage by default, including special-value records. |
| Certification attempts | General transcendental, special and adaptive random paths use 12 attempts; trig and constant paths use 16. Internal construction and iteration limits are stated in their individual source contracts. |

A finite iteration budget can reject a difficult input without establishing
that a whole interval is unsupported. Budget exhaustion must report the
promised error and preserve the transaction's numerical destination.
The new boundary test forces the general 12-attempt failure path, while the
existing suites exercise actual numerical paths and all rounding modes.
PPF_FPIF_MAX_IMPORT_BYTES can configure a different positive import byte cap.
See FPIF_RESOURCE_CONTRACT.md for exact ownership and admission behavior.

The semantic ledger retains 105 bounded implementation paths and six
compiler-conditional interfaces. A complete symbol inventory does not imply
unbounded numerical domains or identical optional formats on every compiler.

## Proof and execution evidence

BRANCH_REVIEW_0.20.0.md maps the reviewed Gamma and digamma reflection, Li2
transformations, Bessel/Airy intervals and retry behavior to their mathematical
arguments. Its assumptions are explicit. This bounded source review and the
independent MPFR comparison matrix are separate evidence types. Neither is a
whole-library formal proof.

QUALIFICATION.md gives one offline command for clean GCC, Clang and OpenMP
builds, complete configured tests, installed consumers, ABI/export checks,
ASan/UBSan hardening and timed seeded fuzz campaigns. The runner binds the
source snapshot and records commands, durations, dependencies and failures.
It must retain actual failure or omitted-check status. LeakSanitizer is
explicitly disabled by default and is distinct from exact project allocator
balance checks. Compiler variation on one host is not external replication.

## Remaining promotion obligations

A 1.0 promotion still requires independent correctness reproduction and
mathematical review of the exact candidate and intended claims. The source
package cannot establish that review by renaming local checks. Longer ongoing
fuzzing and native platform qualification remain separately recorded work.
Broader Windows, macOS, ARM64, big-endian or 32-bit compatibility requires
its own evidence before those platforms enter the supported contract.

The candidate can be distributed with its explicit limits and exact evidence.
No tag, public repository change or remote release is asserted by this file.
