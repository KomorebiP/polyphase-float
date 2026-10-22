# Status

Version 0.20.0 is a pre-1.0 candidate with a reproducible qualification command.
It integrates the PPFIF resource-admission repair, exact Airy boundary checking
and floating-setter validation before special-value mutation.

The semantic ledger indexes 437 core functions. It records 192 transactional
contracts, 63 reviewed allocation-free contracts, 12 verified initialize-and-set
lifecycles, two verified prefix-cleanup contracts and three existing safe-zero
initializers. There are no remaining indexed allocation-evidence gaps.
The 105 bounded implementation paths and six compiler-conditional interfaces
remain explicit. Classification and finite tests do not prove every branch or
all representable inputs.

ABI epoch 1, public declarations, Linux ELF LP64/GMP64 layouts, symbol versions
and SONAME libpolyphase_float.so.0 are retained. Core-only consumers can omit
MPFR. GMP and POSIX threads remain production requirements.

Run the command in docs/QUALIFICATION.md to produce fresh compiler, consumer,
ASan/UBSan and timed fuzz receipts outside the source tree. The supplied
historical and targeted evidence is identified separately. No remote tag or
public release is claimed by this source package.

External correctness replication and independent mathematical review remain
open 1.0 gates. Wider platforms require their own execution evidence.
