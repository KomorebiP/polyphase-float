# Polyphase Float 0.12.0 ABI stability release report

## Result

Version 0.12.0 establishes ABI epoch 1 for the validated Linux ELF
LP64/GMP64 baseline and preserves the public 0.11.0 interface.

The compatibility work is additive. Existing production numerical source files
are byte-for-byte unchanged. The only production file added is `src/abi.c`.

## Validation matrix

| Validation | Result |
|---|---:|
| Normalized 0.11.0-to-0.12.0 source-contract comparison | Compatible |
| Breaking contract differences | 0 |
| Compatible contract additions | 13 |
| Unchanged contract entries | 465 |
| Governed core functions | 437 |
| Inherited `PPF_0.11` functions | 431 |
| Additive `PPF_0.12` functions | 6 |
| Optional adapter functions | 5 |
| Production MPFR imports | 0 |
| LP64/GMP64 layout differences from 0.11.0 | 0 |
| GCC OpenMP configured CTest inventory | 60 / 60 passed |
| GCC serial configured CTest inventory | 60 / 60 passed |
| Clang serial configured CTest inventory | 60 / 60 passed |
| Complete Make differential suite | Passed |
| Make ABI audit | Passed |
| OpenMP installed consumer | Passed |
| Serial installed consumer | Passed |
| Installed ABI policy and contract | Present |
| Symbol-versioning `ON` path | Passed |
| Symbol-versioning `OFF` path | Passed |
| Old 0.11.0 binary run with 0.12.0 | Passed |
| Make/CMake clean-tree coexistence | Passed |

## Old-binary execution

The compatibility test built the actual 0.11.0 source, compiled and linked one
consumer against the 0.11.0 header and shared object, and then ran the unchanged
binary with each implementation.

Both runs produced:

```text
ppf_t=88 context=88 exact=121932631112635380 pi=3.1415926535897931 plan=1/4/1
```

The consumer's dynamic section required:

```text
libpolyphase_float.so.0
```

The second run did not recompile or relink the executable.

## Source and layout contract

`abi/contract-v1.json` is generated from the public headers and records:

- function declarations and conditional availability;
- public typedefs;
- public structure definitions;
- enum members and values;
- ABI-relevant constants.

`abi_layout_probe` independently records the active data model, type sizes,
alignments, enum values, and public field offsets. A layout claim is skipped on
a machine whose data model does not match the frozen baseline rather than being
reported as a false pass.

## ELF governance

The release's Linux shared objects use explicit version scripts.

```text
PPF_0.11       existing core interface
PPF_0.12       additive ABI-introspection interface
PPF_MPFR_0.11  optional adapter interface
```

The export auditor rejects missing symbols, unexpected public symbols,
unexpected non-Polyphase exports, and incorrect symbol-version nodes.

## Runtime introspection

The six additive APIs expose the ABI epoch, revision, tag, normalized contract
hash, ABI-record size, and size-tagged runtime ABI record. The record includes
data-model widths, feature flags, structure sizes and alignments, and selected
field offsets.

## Build-system coexistence correction

The mixed-build validation found that the Make `clean` target removed the whole
`build/` directory, including unrelated CMake evidence trees. Version 0.12.0
limits Make cleanup to Make-owned generated files, objects, libraries, and
executables. A CMake-tree sentinel survives `make clean` in the final source.

## Validation limitation

The local artifact environment had the MPFR runtime library but not the
official `mpfr.h` development header. MPFR-dependent local builds therefore
used the supplied compatibility declaration layer. The target-machine runner
installs and requires `libmpfr-dev` before executing its official-header build.

This distinction remains important because earlier strict official-header
validation exposed conditional binary128 and decimal declarations that were not
available under the assumptions made by the old harness.

## Compatibility boundary

The evidence applies to the documented Linux ELF LP64/GMP64 baseline. It does
not establish ABI compatibility for native Windows, macOS, ARM64, big-endian,
or 32-bit targets. It also does not turn undocumented direct mutation of public
structure internals into a supported programming interface.
