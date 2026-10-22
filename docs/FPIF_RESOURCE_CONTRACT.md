# PPFIF import resource contract

The PPFIF importer admits encoded records before allocating numerical storage.
By default, each record may request at most 64 MiB of payload and at most
64 MiB of limb-rounded significand storage. Both tests apply independently.
The latter test also applies to zero, infinity and NaN records, whose encoded
payload is empty. On the supported GMP64 baseline this admits precision up to
536,870,912 bits.

The private build macro `PPF_FPIF_MAX_IMPORT_BYTES` can select a different
positive byte bound. The general object precision model and export operation
are unchanged. A valid value that exceeds the import resource policy may be
exported but cannot be imported by that configuration.

Resource rejection returns `-1`, sets `PPF_FLAG_ERANGE`, and leaves the
destination unchanged. Malformed or truncated records return `-1` without
committing a numerical value. Project allocation failure also preserves the
destination and raises `PPF_FLAG_ALLOC`. Sticky flags are not rolled back.

A regular payload initially reserves at most 4096 bytes. The buffer grows
only after all bytes in its previous capacity have been read. A header that
claims a large payload but supplies no payload therefore cannot cause the
whole claimed payload allocation. The final numerical temporary is created
only after all payload bytes and the CRC have been validated.

The bound is per allocation domain, not a global memory budget. Payload and
temporary numerical storage can coexist, allocator bookkeeping is additional,
and libc stream buffers and GMP internals have separate ownership. Exported
files are not promised to be safe for an application's unrelated memory
policy merely because they meet this bound.

`tests/test_fpif_resource_bounds.c` checks all four value categories, rejected
metadata, truncated streams, growth-boundary round trips, every project
allocation observed in those round trips, exact allocator sizes and zero
remaining tracked allocations. Its allocator refuses oversized requests so
negative controls do not execute the allocations being rejected. The test is
registered in both the CMake hardening tier and the Make harness.

The repair originated in the preserved October 9 integration package. Its
historical validation is retained separately from the fresh release evidence.
