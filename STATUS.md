# Status

Polyphase Float 0.20.0 is a pre-1.0 source release for Linux x86-64 ELF LP64 with
64-bit GMP limbs. It retains ABI epoch 1 and `libpolyphase_float.so.0`.
The core requires GMP and POSIX threads; MPFR and OpenMP are optional.

The public semantic ledger indexes 437 core functions. Its allocation
classifications contain 192 transactional contracts, 63 reviewed
allocation-free contracts, 12 initialize-and-set lifecycles, two prefix-cleanup
contracts, and three safe-zero initializer contracts. There are no remaining
indexed allocation-evidence gaps. The ledger also identifies 105 bounded
implementation paths and six compiler-conditional interfaces.

The release includes bounded PPFIF resource admission, exact Airy boundary
checking, and floating-setter validation before special-value mutation.
Supported inputs and failure behavior are stated in the
[release scope](docs/COMPLETION_SCOPE_0.20.0.md).

Use [Qualification](docs/QUALIFICATION.md) to reproduce the compiler, consumer,
sanitizer, and timed fuzz checks. A receipt records the exact source and the
checks actually executed. Historical evidence is indexed separately in
[History](docs/HISTORY_INDEX.md).

The [roadmap](ROADMAP.md) records future platform work and the independent
review and external reproduction required for 1.0. Release preparation and
local validation do not assert a remote tag or public publication.
