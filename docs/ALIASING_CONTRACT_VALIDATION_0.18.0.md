# Polyphase Float 0.18.0 aliasing-contract validation

Version 0.18.0 retains the complete function-by-function aliasing contract
introduced in 0.14.0.

| Measure | Result |
|---|---:|
| Public functions classified | 437 |
| Unverified aliasing functions | 0 |
| Distinct-output functions governed | 5 |
| Same-index batch functions governed | 1 |
| Source-distinct preconditions | 2 |

The new single-result transaction layer preserves all established in-place
patterns because the source operands are read while the destination remains
untouched. The allocation matrix includes an in-place arithmetic failure and
an application-managed external destination.

`ppf_div_with_plan_many` retains its verified same-index overlap rule. The new
batch transaction stages all outputs before execution, rejects cross-index and
duplicate-output overlap, and commits no destination or ternary result unless
the complete batch succeeds.
