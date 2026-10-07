# Polyphase Float 0.16.0 aliasing-contract validation

Version 0.16.0 retains the complete function-by-function aliasing contract
introduced in 0.14.0.

| Measure | Result |
|---|---:|
| Public functions classified | 437 |
| Unverified aliasing functions | 0 |
| Distinct-output functions governed | 5 |
| Same-index batch functions governed | 1 |
| Source-distinct preconditions | 2 |

The constant allocation changes do not broaden any overlap guarantee. Constant
evaluation still permits the ordinary destination form defined by the public
API. The new staging object changes only the failure commit boundary.
