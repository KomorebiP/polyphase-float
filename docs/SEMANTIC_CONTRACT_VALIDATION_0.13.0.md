# Polyphase Float 0.13.0 semantic-contract validation

## Scope

This report describes the generated public semantic contract introduced in
0.13.0. The canonical artifacts are:

```text
semantics/contract-v1.json
semantics/function-matrix-v1.csv
semantics/function-purposes.csv
docs/PUBLIC_SEMANTIC_CONTRACT_0.13.0.md
```

The JSON record is canonical. The CSV and Markdown files are generated review
surfaces.

## Evidence model

The generator separates five evidence types:

1. declarations and build conditions parsed from the public header;
2. curated one-sentence purposes and function-family assignments;
3. conservative family rules and explicit function overrides;
4. direct lexical references in test and documentation sources;
5. explicit unresolved gaps.

A direct reference establishes that a public name is exercised or inspected by
a test source. It does not establish complete semantic coverage.

## Audited surface

| Item | Result |
|---|---:|
| Public core functions | **437** |
| Purpose records | **437 / 437** |
| Direct test references | **437 / 437** |
| Conditional functions | **6** |
| Duplicate function names | **0** |
| Missing contract rows | **0** |
| Stale contract rows | **0** |
| Entries without a currently recorded gap | **177** |
| Entries with explicit gaps | **260** |

The six conditional functions are the binary128, decimal64, and decimal128
set/get pairs. Their presence is tied to the compiler-visible public feature
macros rather than inferred from type names.

## Added direct surface test

`tests/test_semantic_surface.c` executes the 47 public names that previously
lacked a direct test-source reference. It covers context metadata, default
rounding, initialize-and-set helpers, scalar conversions, fit predicates,
threaded wrappers, powers-of-two shifts, dot products, constant wrappers,
precomputation, and diagnostic names.

This test is labeled `fast;semantic`. It is intended to prevent public symbols
from silently falling out of the configured validation inventory. It is not a
replacement for family-specific differential or adversarial tests.

## Contract integrity

The public header carries:

```c
PPF_SEMANTIC_CONTRACT_VERSION
PPF_SEMANTIC_CONTRACT_SHA256
```

The policy test regenerates the contract, verifies all classifications, and
compares the canonical hash with the public header. CMake and Make expose:

```text
semantic-contract-audit
semantic-check
```

## Remaining gaps

| Gap | Functions | Interpretation |
|---|---:|---|
| `aliasing_not_individually_verified` | **192** | Complete overlap behavior is not yet an individual guarantee. |
| `allocation_failure_atomicity_incomplete` | **232** | Fail-at-every-allocation behavior is incomplete for the exact entry. |
| `bounded_implementation_path` | **112** | A real attempt, iteration, reduction, parser, or domain bound exists. |
| `conditional_build_surface` | **6** | Enabled and disabled configurations both require validation. |

A function may occur in more than one gap category.

## Claim boundary

Version 0.13.0 supports the claim that every public core function is indexed,
has a curated purpose, and has a direct test-source reference. It does not
support the claim that all 437 functions are exhaustively verified for every
input, rounding mode, alias pattern, resource failure, platform, or practical
limit.


## Build and package validation

The exact 0.13.0 working source passed the following configured inventories:

| Configuration | Result |
|---|---:|
| GCC with OpenMP and benchmark smoke targets | **64 / 64 passed** |
| GCC serial | **51 / 51 passed** |
| Clang serial | **51 / 51 passed** |
| Semantic-specific CTest entries | **3 / 3 passed** |
| CMake installed downstream consumer | **Passed** |
| Make semantic surface and contract policy | **Passed** |
| Public release audit | **Passed** |
| Public declarations and shared-library exports | **437 / 437** |
| Production MPFR imports | **0** |

The local environment supplied the MPFR runtime but not the official development header. The target-machine runner therefore installs and requires the official header before counting that validation path.

## Installation namespace correction

Validation found that the ABI contract and semantic contract originally shared the basename `contract-v1.json` and were initially directed to the same documentation directory. That would have caused one installed artifact to overwrite the other. CMake and Make now install them separately:

```text
share/doc/PolyphaseFloat/semantics/contract-v1.json
share/doc/PolyphaseFloat/semantics/function-matrix-v1.csv
share/doc/PolyphaseFloat/abi/contract-v1.json
```

Both installed contracts were parsed after staged installation, confirmed to have their respective schemas and distinct hashes, and consumed alongside a successful downstream CMake build.
