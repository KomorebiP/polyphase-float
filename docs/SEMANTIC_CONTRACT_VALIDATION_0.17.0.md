# Polyphase Float 0.17.0 semantic-contract validation

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
| Allocation-failure atomicity incomplete | 137 |
| Bounded implementation path | 112 |
| Conditional build surface | 6 |
| Aliasing not individually verified | 0 |

Version 0.17.0 promotes exactly 79 logarithmic, exponential, power,
trigonometric, hyperbolic, and special-function endpoints to
`verified_transactional`. The promotion requires both membership in the
reviewed family and a direct reference from
`tests/test_transcendental_special_allocation_failure.c`. Setup helpers named
by the test do not inherit that classification.

The contract records 157 public functions as transactionally verified. It
retains practical-limit markers for the governed numerical families because
allocation closure does not remove iteration caps, argument limits, or other
bounded implementation choices.

## Reproduction

```sh
python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json
python3 tests/test_semantic_contract.py --root .
python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json
python3 tests/test_aliasing_contract.py --root .
```
