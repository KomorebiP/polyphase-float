# ABI compatibility for 0.19.0

This release preserves ABI epoch 1 and the public 0.18.0 interface on Linux ELF
LP64 with 64-bit GMP limbs. It adds no public function, type, layout, enum value,
or calling convention. SONAME remains libpolyphase_float.so.0. Internal
certificate helpers are hidden. Semantic and aliasing evidence hashes change.

The normalized comparison is abi/0.18.0-to-0.19.0-comparison.json. CTest checks
source contracts, layouts, export versions, and the current runtime ABI. The
frozen 0.11.0 binary-consumer replay is a separate test with its own command.

Corrections to erroneous numerical results and failure behavior are intended
semantic repairs. Binary compatibility does not preserve incorrect results.
Installed CMake version requests from 0.11 onward are supported within epoch 1;
requests predating the governed baseline are rejected.
