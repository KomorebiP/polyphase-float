# Polyphase Float public semantic contract

Release: `0.19.0`  
Contract version: `1`  
Contract SHA-256: `2cdeccca5aab6bb0d3ee2ff5097f9116a40ec6da374b75b09da95d80443c068d`

## Purpose and boundary

This document is the human-readable summary of the generated public semantic ledger. Every core function declared in `include/polyphase_float.h` has exactly one machine-readable record in `semantics/contract-v1.json` and one review row in `semantics/function-matrix-v1.csv`.

The ledger distinguishes mechanically parsed facts, curated descriptions, conservative family-level classifications, direct test references, and explicit unresolved gaps. A direct test reference means that the function is named in a test source. It does not by itself prove every branch, input class, rounding mode, alias pattern, or allocation-failure point.

An entry without a currently recorded gap is not a universal correctness proof. It means that none of the gap categories modeled by contract version 1 applies to that entry.

## Coverage summary

| Item | Count |
|---|---:|
| Public core functions | **437** |
| Functions with a direct test reference | **437** |
| Functions with a pre-existing documentation reference | **153** |
| Conditional functions | **6** |
| Entries without a currently recorded gap | **253** |
| Entries with one or more explicit gaps | **184** |

## Explicit gap counts

A function may appear in more than one row below.

| Gap | Functions | Meaning |
|---|---:|---|
| `allocation_failure_atomicity_incomplete` | 79 | Fail-at-every-allocation transactional behavior has not been demonstrated for this exact entry. |
| `bounded_implementation_path` | 104 | The current source contains a real domain, attempt, iteration, or reduction bound. |
| `conditional_build_surface` | 6 | The declaration depends on a compiler or build capability and must be validated in both enabled and disabled configurations. |

## Classification rules

### Return values

`ternary` records the floating-point relation to the exact result and uses `PPF_TERNARY_ERROR` for an unrecovered failure. `status` records an ordinary success or failure code. Predicates, comparisons, counts, borrowed pointers, and caller-owned pointers are classified separately.

### Rounding

`explicit` means a public rounding parameter is present. `fixed_by_operation` means the operation name selects the rule. `not_applicable` means the function does not perform a floating-point settlement step governed by a public rounding argument.

### Contexts and flags

`explicit_required` means a null context is invalid for that lifecycle call. `explicit_or_thread_default` means a null context resolves to the calling thread's default context. Flag lists marked `family_upper_bound` are conservative: they identify flags reachable somewhere in the family, not a claim that every flag is reachable for every input to every function.

### Aliasing

`verified_in_place` means the destination/source overlap positions named by the systematic matrix have direct evidence. `verified_distinct_outputs` governs paired outputs that must remain distinct while permitting documented input overlap. `verified_same_index_overlap` governs batch interfaces whose output may replace only its corresponding input. `source_must_be_distinct` and `lifecycle_destination` record real preconditions rather than implying unsupported in-place use. Direct-storage functions remain application-managed advanced interfaces.

### Failure atomicity

`verified_transactional` means deterministic allocation-failure injection directly exercises the entry. `documented_partial` means a family-level transaction rule exists but fail-at-every-site coverage is incomplete for that exact wrapper. `not_yet_verified` is not promoted to a guarantee.

### Practical limits

`implementation_bound` records a bound found in the current source, such as a maximum escalation count, iteration count, parser exponent policy, or a finite domain restriction. These are implementation properties, not mathematical limitations of the represented function.

## Function families

| Family | Functions |
|---|---:|
| Arithmetic and product planning | 40 |
| Assignment and parsing | 21 |
| Build and ABI information | 7 |
| Certified constants and constant caches | 24 |
| Certified interval assignment | 1 |
| Comparison, ordering, sign, and absolute value | 20 |
| Contexts, allocators, flags, and scratch memory | 23 |
| Conversion to C and GMP types | 20 |
| Core comparison, validation, and diagnostics | 7 |
| Direct significand access (advanced) | 4 |
| Exponent range and exception flags | 34 |
| Initialization and precision | 15 |
| Initialize-and-set shortcuts | 12 |
| Integer rounding, fractional parts, and remainders | 20 |
| Logarithms, exponentials, and powers | 24 |
| Neighboring values, min/max, and exponent access | 8 |
| Random values, build information, cache control, and custom storage | 28 |
| Reusable division, fused operations, powers of two, sums, and dot products | 23 |
| Roots and hypotenuse | 10 |
| Special functions | 20 |
| Special values, signs, and rounding control | 17 |
| Text I/O, PPFIF interchange, formatting, and fit tests | 22 |
| Trigonometric and hyperbolic functions | 37 |

## Reproduction

```sh
python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json
python3 tests/test_semantic_contract.py --root .
cmake --build build/validation --target semantic-contract-audit
make semantic-check
```

The JSON contract is canonical. The CSV is a flattened review surface generated from the same data. This Markdown summary is generated from the same contract and is not a substitute for the per-function records.

## Installed artifact layout

Installed packages preserve the semantic and ABI contracts at distinct paths:

```text
share/doc/PolyphaseFloat/semantics/contract-v1.json
share/doc/PolyphaseFloat/semantics/function-matrix-v1.csv
share/doc/PolyphaseFloat/abi/contract-v1.json
```

This separation prevents the two independently governed `contract-v1.json` files from overwriting one another.
