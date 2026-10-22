# Public release status

The current source is Polyphase Float 0.20.0 under LGPL-3.0-or-later. It is a
pre-1.0 candidate for external reproduction on Linux ELF LP64/GMP64.

The 437-function semantic ledger now assigns explicit contracts to all 79
previous allocation-evidence entries. The classification distinguishes
allocation-free valid-input paths, safe initialization, prefix cleanup and
transactional numerical outputs. GMP and libc allocation domains remain
separate. The new tests and review are described in
ALLOCATION_COMPLETION_0.20.0.md.

The integrated PPFIF importer checks per-record payload and significand
resource limits before allocation. Airy Ai uses exact input comparison at its
finite-domain boundary. Floating setters validate rounding before handling
special values. These repairs preserve the public source and binary interface.

BRANCH_REVIEW_0.20.0.md maps reviewed special-function branches to their
mathematical arguments and assumptions. It is a bounded source review, not a
whole-library proof. Numerical caps and certification-budget rejection remain
published in COMPLETION_SCOPE_0.20.0.md.

QUALIFICATION.md gives one offline command for fresh GCC, Clang, OpenMP,
installed-consumer, sanitizer and timed fuzz validation. Retain its generated
receipt and exact source snapshot. Compiler variation on one host is not
independent external replication.

This package does not claim universal correct rounding, unbounded numerical
domains, native platform validation beyond the stated baseline, or completion
of external mathematical review. No remote repository publication is asserted.
