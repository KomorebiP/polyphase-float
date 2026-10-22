# Allocation contracts completed for Polyphase Float 0.20.0

The 79 allocation-evidence entries retained in 0.19.0 have been resolved by
individual source classification and direct execution. They are not all
transactional numerical operations. The resulting ledger adds 63 reviewed
project-allocation-free entries, 12 initialization lifecycles, two prefix-cleanup
lifecycles, and two directly fault-tested transactional wrappers. There are now
192 directly fault-tested transactional entries in the complete ledger.

## Allocation domains

These statements govern calls to the project context or object allocator.
GMP's process-level allocator is separate. libc's variadic-prefix tracking
array, stream buffering and kernel allocations remain outside this recoverable
failure contract. Valid initialized objects must have capacity for their declared
precision. Caller-owned storage must satisfy the direct-storage API contract.
An optional context must already exist before fault injection begins.

## Source review

The exact function sets are stored in the contract generator and the accompanying
JSON receipt. Every entry is directly named by `test_allocation_completion.c`.
The review follows these implementation paths:

| Path | Reason no project allocation is needed |
| --- | --- |
| Object clear, swap, special values, direct limbs and commit | Metadata updates, fixed-capacity memory access, or release of existing storage. These operations are not old-value transactions. |
| Integer, binary/decimal, GMP, sign and ordinary assignment | GMP arithmetic prepares exact values. Fixed-precision commits reserve only already-owned capacity. Range conversion uses GMP rounding or special-value metadata. |
| Range adjustment and subnormalization | GMP arithmetic and fixed-capacity destination updates. No project scratch or new numerical object is initialized. |
| Integer rounding, fractional/remainder and min/max operations | Exact GMP temporaries and fixed-capacity commits. Multi-output calls retain their distinct-output and initialized-storage preconditions. |
| Neighboring values and exponent setters | Fixed-limb arithmetic or special-value metadata. |
| Raw precision, external storage and nearest-away finishing | Valid existing capacity, caller-managed storage, and assignment without increasing precision. |
| `ppf_mul_2exp` and `ppf_div_2exp` | Their internal exact-scaling paths use existing capacity and GMP range fallback. They do not enter the public arithmetic transaction wrapper. |

This is a reviewed allocation-domain classification, not a proof that GMP
performs no private allocation or a proof of all numerical branches. The
conditional binary128 and decimal declarations remain conditional in the ledger.

## Executed cases

The fixed-storage test uses precisions 3, 53, 257 and 4097, all six rounding
modes and three exponent-range regimes. A refusing allocator remains armed.
Advanced-storage cases check raw precision, direct limb commit, external storage
movement, swap, and exact allocator release sizes and balances.

The twelve initialize-and-set wrappers pass 78 injected-failure cases across
all rounding modes. Initialization failure leaves a safe-zero object. A later
setter failure leaves an initialized, clearable object, which the caller must
clear. This explicitly differs from preserving an old initialized destination.

The strtofr and positive-difference wrappers are tested with 16385-bit objects,
ordinary and aliased destinations, caller-owned output storage, all rounding
modes, wide and narrow ranges, and fresh scratch pools before each run. The
96 injected failures produce 48 unchanged error outcomes and 48 exact successful
fallbacks. Successful fallbacks retain the allocation flag as a diagnostic.
The two exact-scaling aliases record zero project allocations in these cases,
consistent with their reviewed source paths.

Variadic initialization is faulted at each of three object positions for both
initializers. The initialized prefix is cleared, the failed initializer is
safe-zero, and unvisited destinations retain their sentinel bytes. This verifies
prefix cleanup without promising a whole-list initialization transaction.

## Additional repaired defect

The ordinary floating setters and conditional binary128/decimal setters
previously accepted invalid rounding modes on zero, infinity and NaN before
validating the mode. They now validate the context and rounding mode before
changing the destination. Direct regressions cover signed zeros, signed
infinities, NaN and an ordinary finite value. A negative-control shared library
restoring only the two original implementation files fails this regression.

## Reproduction and evidence

`tests/test_allocation_completion.c` is the executable test. Its final build
integration belongs to both CMake and Make. The exact output, source hashes,
negative-control failure and ledger classification are recorded in
`evidence/ALLOCATION_COMPLETION_0.20.0.json`. The main release report records
subsequent compiler and sanitizer matrix runs separately. A passing local
allocation campaign does not establish external replication or universal
correct rounding.
