# Public release status

Polyphase Float 0.20.0 is distributed under LGPL-3.0-or-later with a bounded
pre-1.0 contract for Linux x86-64 ELF LP64 and 64-bit GMP limbs. ABI epoch 1 and
`libpolyphase_float.so.0` are retained. The production core is independent of
MPFR; the adapter and differential tests use MPFR optionally.

The [release scope](COMPLETION_SCOPE_0.20.0.md) is the current reference for
supported inputs, allocation behavior, resource limits, and review boundaries.
The [semantic contract](PUBLIC_SEMANTIC_CONTRACT_0.20.0.md) indexes the 437 core
functions, while the [allocation reference](ALLOCATION_COMPLETION_0.20.0.md)
records the completed classifications and targeted lifecycle tests.

Use [Qualification](QUALIFICATION.md) to reproduce validation. The resulting
receipt records actual execution against an exact source snapshot; historical
reports do not replace it. [Claims and evidence](CLAIMS_AND_EVIDENCE.md)
explains how to interpret tests, mathematical arguments, and benchmark results.

The [roadmap](../ROADMAP.md) lists outstanding independent review and external
reproduction requirements for a later 1.0 release. Source packaging and local
qualification do not assert a remote commit, tag, or published release asset.
