# Polyphase Float 0.16.0 semantic-contract validation

## Coverage

| Measure | Result |
|---|---:|
| Public core functions | 437 |
| Functions with direct test-source references | 437 |
| Conditional functions | 6 |
| Entries without a currently recorded gap | 190 |
| Entries with one or more explicit gaps | 247 |

## Explicit gaps

A function may occur in more than one category.

| Gap | Functions |
|---|---:|
| Allocation-failure atomicity incomplete | 216 |
| Bounded implementation path | 112 |
| Conditional build surface | 6 |
| Aliasing not individually verified | 0 |

Version 0.16.0 promotes the 12 certified-constant evaluation routes only where
`tests/test_constant_allocation_failure.c` provides deterministic direct
evidence. The complete 24-entry constant family retains bounded-path markers
because fixed attempt limits, guard policies, and optional execution choices
remain documented practical boundaries.

## Reproduction

```sh
python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json
python3 tests/test_semantic_contract.py --root .
python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json
python3 tests/test_aliasing_contract.py --root .
```
