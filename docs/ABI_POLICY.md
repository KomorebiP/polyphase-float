# ABI policy

## Governed baseline

Polyphase Float 0.12.0 established **C ABI epoch 1** for the supported Linux
x86-64 ELF baseline. The baseline is the public C surface of 0.11.0 plus the additive
ABI-introspection interfaces introduced in 0.12.0. Version 0.20.0 retains
that function and layout surface, including the source-level constants that
identify the generated semantic contract.

The compatibility commitment currently covers:

- Linux, including Linux under WSL, on an x86-64 LP64 target;
- 64-bit pointers and `size_t`;
- the GMP limb model recorded by the installed contract;
- the C calling convention used by the supported GCC and Clang builds;
- the shared-library SONAME `libpolyphase_float.so.0`;
- the public layouts, enum values, constants, and function declarations in the
  epoch-1 contract.

This contract applies to the stated baseline. Windows, macOS, ARM64,
big-endian systems, and 32-bit targets require separate ABI qualification.

## Compatibility rules for ABI epoch 1

A release remains compatible with ABI epoch 1 only when all of the following
are true:

1. Existing exported functions keep the same calling convention, return type,
   parameter types, and semantics needed for binary use.
2. Existing public structures keep their size, alignment, field order, field
   types, and ownership meaning on a matching data model.
3. Existing enum values and public constants do not change.
4. Existing ELF symbols remain available at symbol version `PPF_0.11`.
5. Additive public functions receive a later symbol-version node.
6. The SONAME remains load-compatible with binaries linked to the epoch-1
   baseline.
7. The generated source contract and the runtime layout probe both pass.

Additive functions, new opaque types, and new reserved-bit meanings may be
introduced without ending the epoch. A source or binary breaking change
requires a new ABI epoch and an explicit SONAME transition.

## Public structures

The project has chosen to retain the existing exposed structures for ABI epoch
1. The
following layouts are therefore governed:

- `ppf_allocator_t`
- `ppf_arena_mark_t`
- `ppf_context_t`
- `ppf_t`
- `ppf_product_plan_t`
- `ppf_constant_stats_t`

The epoch-1 contract records each structure definition. The runtime ABI record
also exposes the sizes, alignments, and the field offsets of `ppf_context_t`
and `ppf_t`.

Direct field mutation remains discouraged. A layout being ABI-stable does not
make every internal-looking field a supported application extension point.
Callers should continue to use the public functions for initialization,
precision changes, assignment, range handling, flags, and cleanup.

## Runtime introspection

Applications and diagnostic tools may query:

```c
ppf_abi_epoch();
ppf_abi_revision();
ppf_abi_tag();
ppf_abi_contract_sha256();
ppf_abi_info_size();
ppf_get_abi_info();
```

`ppf_get_abi_info` uses a size-tagged record. It reports the active data model,
feature bits, public structure sizes and alignments, and selected field
offsets. A caller may pass a smaller known prefix and receives a truncation
status rather than an out-of-bounds write.

The contract hash identifies the normalized public source contract. It is not a
cryptographic attestation of the loaded binary and should not be used as a
security signature.

## ELF symbol versions

On supported Linux ELF builds, symbol versioning is controlled by:

```text
PPF_ELF_SYMBOL_VERSIONING=AUTO
PPF_ELF_SYMBOL_VERSIONING=ON
PPF_ELF_SYMBOL_VERSIONING=OFF
```

`AUTO` enables version scripts on Linux and disables them elsewhere. `ON`
requires a supported ELF linker. `OFF` is available for unusual toolchains and
static-only environments, but release binaries for the governed Linux baseline
must use symbol versioning.

Core symbols inherited from 0.11.0 are exported as `PPF_0.11`. The six
introspection functions added in 0.12.0 are exported as `PPF_0.12`. The MPFR
adapter has the independent node `PPF_MPFR_0.11`.

## Shared-library version

The SONAME remains `libpolyphase_float.so.0` in 0.20.0. An unchanged
executable linked against the 0.11.0 shared library must be able to load the
compatible 0.20.0 implementation on the governed baseline. The MPFR adapter
retains `libpolyphase_float_mpfr.so.0`.

Before version 1.0, the project must decide whether to retain this SONAME or
introduce a new major SONAME while shipping an epoch-1 compatibility library.
That decision must also cover CMake package-version compatibility and the
support window. The current 0.x package accepts minimum-version requests from
0.11.0 onward within that major series; exact-version requests remain exact.
A future project-version change must not silently invalidate the compatibility
evidence established here.

## PPFIF is separate

The `PPFIF01` interchange format is a serialized-data contract, not the C ABI.
Its version, byte order, validation, and compatibility rules are governed
separately.

## Automated checks

The release process performs five independent checks:

1. **Source contract comparison.** The normalized declarations, types, layouts,
   enums, and constants are compared with the frozen 0.11.0 baseline.
2. **Runtime layout comparison.** A small program compiled against the current
   header is compared with the frozen LP64/GMP64 layout record.
3. **Export and symbol-version audit.** The shared object must export exactly
   the declared core surface with the required version nodes.
4. **Previous-release comparison.** The current source contract is checked
   against both the frozen 0.11.0 epoch baseline and the immediately previous
   release contract.
5. **Old-binary execution.** A consumer compiled and linked against the actual
   0.11.0 library is run unchanged with the current epoch-1 shared library.

A data-model mismatch is reported as unsupported evidence rather than being
misrepresented as a compatibility pass.

## Compatibility exceptions

A breaking change may be considered only for a demonstrated security,
correctness, or unsound-ownership defect that cannot be repaired compatibly.
Such a change requires:

- a new ABI epoch;
- a documented migration path;
- a SONAME and symbol-version decision;
- an explicit compatibility report;
- release notes identifying the exact break;
- preservation of the previous compatibility library when practical.
