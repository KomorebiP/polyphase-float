# Polyphase Float 0.14.0 semantic-contract validation

## Result

The generated semantic contract covers all **437** public core functions. Every function has at least one direct test reference. The systematic aliasing block removes the prior aliasing gap category entirely.

| Measure | Result |
|---|---:|
| Functions indexed | 437 |
| Functions with direct test reference | 437 |
| Functions with no recorded contract-v1 gap | 188 |
| Functions with explicit remaining gaps | 249 |
| Unverified aliasing functions | 0 |
| Allocation atomicity gaps | 232 |
| Bounded implementation paths | 112 |
| Conditional build entries | 6 |

The ledger is an evidence index, not a claim that every numerical branch is formally proved. Remaining gaps are retained explicitly.
