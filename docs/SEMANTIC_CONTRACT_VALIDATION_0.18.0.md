# Polyphase Float 0.18.0 semantic-contract validation

## Coverage

| Measure | Result |
|---|---:|
| Public core functions | 437 |
| Functions with direct test-source references | 437 |
| Conditional functions | 6 |
| Entries without a currently recorded gap | 246 |
| Entries with one or more explicit gaps | 191 |
| Verified transactional functions | 213 |

## Explicit gaps

A function may occur in more than one category.

| Gap | Functions |
|---|---:|
| Allocation-failure atomicity incomplete | 81 |
| Bounded implementation path | 112 |
| Conditional build surface | 6 |

Version 0.18.0 promotes exactly 56 arithmetic, fused/planned-operation, and
root entries from `documented_partial` to `verified_transactional`. Each
promotion requires an exact public-name reference in
`tests/test_arithmetic_root_allocation_failure.c` and deterministic
fail-at-every-project-allocation evidence.

## Reproduction

```sh
python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json
python3 tests/test_semantic_contract.py --root .
python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json
python3 tests/test_aliasing_contract.py --root .
```
