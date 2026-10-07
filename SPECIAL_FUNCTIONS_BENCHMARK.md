# Special-function development benchmark

Each row compares one independent Polyphase call with one MPFR call at the same binary precision and nearest-even rounding. The benchmark checks the returned value and ternary direction before recording the timing.

A ratio above one favors Polyphase.

| Bits | Operation | Polyphase (s) | MPFR (s) | MPFR / Polyphase |
|---:|---|---:|---:|---:|
| 64 | `eint` | 0.000498341 | 0.000011935 | 0.024× |
| 64 | `li2` | 0.000470821 | 0.000011429 | 0.024× |
| 64 | `gamma` | 0.001225626 | 0.000013260 | 0.011× |
| 64 | `gamma_inc` | 0.001409442 | 0.000021648 | 0.015× |
| 64 | `digamma` | 0.000678425 | 0.000019396 | 0.029× |
| 64 | `beta` | 0.003300062 | 0.000025217 | 0.008× |
| 64 | `zeta` | 0.002775398 | 0.000076175 | 0.027× |
| 64 | `erf` | 0.000100356 | 0.000007491 | 0.075× |
| 64 | `erfc` | 0.000098621 | 0.000007543 | 0.076× |
| 64 | `j0` | 0.000084121 | 0.000001426 | 0.017× |
| 64 | `y0` | 0.000537823 | 0.000006666 | 0.012× |
| 64 | `agm` | 0.000013037 | 0.000000472 | 0.036× |
| 64 | `ai` | 0.008227187 | 0.000013637 | 0.002× |
| 64 | `const_euler_cached` | 0.000000265 | 0.000000066 | 0.249× |
| 64 | `const_catalan_cached` | 0.000000151 | 0.000000055 | 0.364× |
| 128 | `eint` | 0.000615002 | 0.000015041 | 0.024× |
| 128 | `li2` | 0.000658631 | 0.000039058 | 0.059× |
| 128 | `gamma` | 0.001655700 | 0.000017798 | 0.011× |
| 128 | `gamma_inc` | 0.002025975 | 0.000034812 | 0.017× |
| 128 | `digamma` | 0.001052009 | 0.000037070 | 0.035× |
| 128 | `beta` | 0.005353545 | 0.000048060 | 0.009× |
| 128 | `zeta` | 0.004148920 | 0.000171088 | 0.041× |
| 128 | `erf` | 0.000124180 | 0.000004921 | 0.040× |
| 128 | `erfc` | 0.000123060 | 0.000004789 | 0.039× |
| 128 | `j0` | 0.000101029 | 0.000002410 | 0.024× |
| 128 | `y0` | 0.000758740 | 0.000012663 | 0.017× |
| 128 | `agm` | 0.000014140 | 0.000000951 | 0.067× |
| 128 | `ai` | 0.010969779 | 0.000017945 | 0.002× |
| 128 | `const_euler_cached` | 0.000000120 | 0.000000046 | 0.383× |
| 128 | `const_catalan_cached` | 0.000000128 | 0.000000064 | 0.500× |
| 256 | `eint` | 0.000879528 | 0.000024277 | 0.028× |
| 256 | `li2` | 0.001114442 | 0.000079987 | 0.072× |
| 256 | `gamma` | 0.003149580 | 0.000033510 | 0.011× |
| 256 | `gamma_inc` | 0.003583208 | 0.000058418 | 0.016× |
| 256 | `digamma` | 0.002081058 | 0.000077143 | 0.037× |
| 256 | `beta` | 0.009589848 | 0.000079105 | 0.008× |
| 256 | `zeta` | 0.007160308 | 0.000562157 | 0.079× |
| 256 | `erf` | 0.000187821 | 0.000009664 | 0.051× |
| 256 | `erfc` | 0.000188189 | 0.000009869 | 0.052× |
| 256 | `j0` | 0.000149273 | 0.000004734 | 0.032× |
| 256 | `y0` | 0.001039168 | 0.000023204 | 0.022× |
| 256 | `agm` | 0.000017196 | 0.000001526 | 0.089× |
| 256 | `ai` | 0.019252064 | 0.000028380 | 0.001× |
| 256 | `const_euler_cached` | 0.000000116 | 0.000000047 | 0.405× |
| 256 | `const_catalan_cached` | 0.000000139 | 0.000000055 | 0.396× |

## Interpretation

This is a correctness-first reference implementation. The present special-function algorithms use directed interval arithmetic and conservative series, recurrences, reflection formulas, and precision escalation. The benchmark is included to identify the next speed targets; it is not presented as a performance win over MPFR.

The explicit speed-return backlog is recorded in `SPEED_RETURN_BACKLOG.md`. It includes both this special-function layer and the previously deferred inverse-angle and dense trigonometric work.
