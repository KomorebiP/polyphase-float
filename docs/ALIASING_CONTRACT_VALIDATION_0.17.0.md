# Polyphase Float 0.17.0 aliasing-contract validation

Version 0.17.0 retains the complete function-by-function aliasing contract
introduced in 0.14.0.

| Measure | Result |
|---|---:|
| Public functions classified | 437 |
| Unverified aliasing functions | 0 |
| Distinct-output functions governed | 5 |
| Same-index batch functions governed | 1 |
| Source-distinct preconditions | 2 |

The new result transaction preserves the established in-place guarantees. The
allocation matrix explicitly checks an aliased `ppf_log` destination. Paired
`ppf_sin_cos` and `ppf_sinh_cosh` outputs still must be distinct, and both are
validated before either transaction commits.
