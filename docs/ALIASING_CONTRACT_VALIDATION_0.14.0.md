# Polyphase Float 0.14.0 aliasing-contract validation

## Scope

Version 0.14.0 assigns an explicit aliasing rule to every public core function and compiles a systematic overlap test for every governed numerical pattern.

| Measure | Result |
|---|---:|
| Public functions classified | 437 |
| Functions with aliasing evidence | 213 |
| Unverified aliasing functions | 0 |
| Distinct-output functions governed | 5 |
| Same-index batch functions governed | 1 |
| Source-distinct preconditions | 2 |

## Tested classes

- ordinary in-place unary, binary, ternary, and quaternary destinations;
- overlap with every source position;
- all input sources identical, including destination overlap;
- exact sums, dot products, roots, divisor plans, and threaded wrappers;
- paired floating outputs and auxiliary integer outputs;
- same-index batch overlap for reusable divisor plans;
- initialization-only destinations and source-distinct preconditions;
- all six release-defined rounding modes;
- value, ternary-return, and flag agreement against out-of-place references.

## Edge-policy corrections

Three invalid-overlap paths are now transactional: `ppf_modf` with identical outputs, `ppf_grandom` with identical pair outputs, and constant-interval export with identical lower and upper destinations. These corrections change overlap handling, not numerical formulas.
