# Polyphase Float FPIF resource-bound repair

This is a tested repair for the preserved Polyphase Float 0.19.0 source. It is
an integration package, not a newly frozen release.

The importer previously allocated the payload size declared in the input
header before proving that those bytes were present. Special-value records
also used the declared precision to allocate numerical storage even though
those records contain no significand payload.

`src/io.c` now enforces a 64 MiB default bound independently on both the
imported payload and rounded significand storage before allocation. This
bounds ordinary import precision to 536,870,912 bits on the GMP64 baseline.
The private build macro `PPF_FPIF_MAX_IMPORT_BYTES` can configure another
positive byte bound. This limit does not change the general `ppf_t` precision
model or restrict export. Import rejection returns -1, sets `PPF_FLAG_ERANGE`,
and preserves the numerical destination.

Regular records stage at most 4096 bytes initially and grow only after all
previously reserved payload bytes have arrived. A truncated header-only
record therefore does not allocate the declared large payload. The importer
still restores the encoded precision and value for supported complete
records. CRC validation and numerical commit remain transactional.

The new `tests/test_fpif_resource_bounds.c` validates resource admission for
all four value categories, bounded staging for truncated records, ordinary
roundtrips across the buffer-growth boundary, every allocation-failure
position in those roundtrips, exact allocator sizes, and zero live blocks
and bytes after cleanup. Its allocator refuses requests above 16 KiB, so no
oversized metadata allocation is executed by the regression itself.

The new regression and the existing object, allocation-failure, I/O allocation-
failure, and API lifecycle suites passed GCC 13.3.0 with AddressSanitizer and
UndefinedBehaviorSanitizer enabled. LeakSanitizer was disabled because its
`/proc` task inspection was inaccessible in the earlier audit environment.
Exact custom-allocator balance checks run independently.

Two bounded negative controls fail when original 0.19.0 `io.c` is restored:
metadata admission records an oversized request instead of rejecting it,
and a truncated regular record requests its whole declared payload instead
of bounded initial storage. These controls do not execute the refused
oversized allocations.

The original fuzz artifact and previous active build directory were absent
from the resumed executor. Its exact replay and a renewed sustained fuzz
campaign have not been performed. `VALIDATION.json` records this limitation,
source hashes, test hashes, and test output.

## Integrate

Apply `fpif_resource_bounds.patch` to the 0.19.0 source, or replace only
`src/io.c` with the included file. Add `tests/test_fpif_resource_bounds.c`
to the normal core hardening test tier and update the public import practical
limit documentation and semantic ledger. No public header, ABI layout,
exported function, or version number is changed by this repair package.

Before producing a new frozen release, rebuild the affected configurations,
repeat relevant import/fuzz checks, regenerate the source/evidence manifest,
and complete the ordinary release and semantic audits. Existing release
metadata is deliberately not replaced by a partial integration claim.
