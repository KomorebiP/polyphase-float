# Polyphase Float 0.15.0 aliasing-contract validation

Version 0.15.0 retains the complete function-by-function aliasing contract
introduced in 0.14.0.

| Measure | Result |
|---|---:|
| Public functions classified | 437 |
| Unverified aliasing functions | 0 |
| Distinct-output functions governed | 5 |
| Same-index batch functions governed | 1 |
| Source-distinct preconditions | 2 |

The I/O allocation changes do not broaden any numerical overlap guarantee. The
systematic compiled alias matrix remains the governing evidence for ordinary
in-place operations, paired outputs, same-index batch replacement, lifecycle
destinations, and source-distinct preconditions.
