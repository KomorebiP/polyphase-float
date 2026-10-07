# Completed arithmetic-family benchmark

Each row compares one independent Polyphase operation with the corresponding MPFR operation at the same destination precision and nearest-even rounding. The benchmark verifies the numerical result and ternary direction before retaining the timing.

A ratio above one favors Polyphase.

| Bits | Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |
|---:|---|---:|---:|---:|
| 1,000 | `add_ui` | 0.000000344 | 0.000000031 | 0.090× |
| 1,000 | `add_q` | 0.000001826 | 0.000000385 | 0.211× |
| 1,000 | `mul_z` | 0.000000382 | 0.000000164 | 0.430× |
| 1,000 | `div_q` | 0.000003271 | 0.000000418 | 0.128× |
| 1,000 | `fmma` | 0.000000687 | 0.000000403 | 0.586× |
| 1,000 | `fac_ui` | 0.000000184 | 0.000004320 | 23.515× |
| 10,000 | `add_ui` | 0.000000396 | 0.000000077 | 0.196× |
| 10,000 | `add_q` | 0.000022025 | 0.000009598 | 0.436× |
| 10,000 | `mul_z` | 0.000008016 | 0.000005309 | 0.662× |
| 10,000 | `div_q` | 0.000029679 | 0.000013668 | 0.461× |
| 10,000 | `fmma` | 0.000016270 | 0.000015467 | 0.951× |
| 10,000 | `fac_ui` | 0.000004747 | 0.000214260 | 45.132× |
| 100,000 | `add_ui` | 0.000001394 | 0.000000573 | 0.411× |
| 100,000 | `add_q` | 0.000607472 | 0.000365380 | 0.601× |
| 100,000 | `mul_z` | 0.000180548 | 0.000358325 | 1.985× |
| 100,000 | `div_q` | 0.000814251 | 0.000485796 | 0.597× |
| 100,000 | `fmma` | 0.000363797 | 0.000364284 | 1.001× |
| 100,000 | `fac_ui` | 0.000133187 | 0.015204523 | 114.159× |

## Interpretation

The scalar wrappers are primarily a public-surface completion milestone. Integer and double variants lower into the existing independent binary kernels, while rational variants retain exact GMP-rational arithmetic until one final binary rounding. The extra temporary construction makes several small and middle-precision scalar rows slower than MPFR.

`mul_z` begins to benefit from the existing large-integer multiplication path at high precision. `fmma` is near parity in the largest development row because the two exact products are fused before the single final rounding.

`fac_ui` delegates the exact integer factorial to GMP and performs only one final independent floating-point rounding. That specialized path is substantially faster than the installed MPFR factorial implementation in the measured rows. The factorial argument is selected from the target precision so that the exact integer has the same general scale as the destination.

These measurements are development-machine evidence, not portable crossover claims. The supplied launcher reruns them on the target processor and GMP/MPFR build.
