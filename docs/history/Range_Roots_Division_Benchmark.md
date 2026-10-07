# Range, roots, and reusable-division benchmark

Medians of 5 warmed runs. Matched batch rows give both implementations 4 outer workers.

| Precision | Method | Median (s) |
|---:|---|---:|
| 100,000 | ppf_plan_build | 0.040525188 |
| 100,000 | ppf_cbrt | 0.000998033 |
| 100,000 | mpfr_cbrt | 0.000510738 |
| 100,000 | ppf_root7 | 0.031836477 |
| 100,000 | mpfr_root7 | 0.002118507 |
| 100,000 | ppf_rec_sqrt | 0.001377095 |
| 100,000 | mpfr_rec_sqrt | 0.000463939 |
| 100,000 | ppf_hypot | 0.003707997 |
| 100,000 | mpfr_hypot | 0.000553302 |
| 100,000 | ppf_div_direct | 0.000503167 |
| 100,000 | ppf_div_planned | 0.000525811 |
| 100,000 | mpfr_div | 0.000358582 |
| 100,000 | ppf_plan_batch_matched | 0.001875787 |
| 100,000 | mpfr_batch_matched | 0.001222226 |
| 1,000,000 | ppf_plan_build | 0.058819259 |
| 1,000,000 | ppf_cbrt | 0.105655410 |
| 1,000,000 | mpfr_cbrt | 0.020557634 |
| 1,000,000 | ppf_root7 | 0.105236989 |
| 1,000,000 | mpfr_root7 | 0.047879506 |
| 1,000,000 | ppf_rec_sqrt | 0.071016110 |
| 1,000,000 | mpfr_rec_sqrt | 0.011344771 |
| 1,000,000 | ppf_hypot | 0.017138266 |
| 1,000,000 | mpfr_hypot | 0.017327672 |
| 1,000,000 | ppf_div_direct | 0.009461023 |
| 1,000,000 | ppf_div_planned | 0.008586914 |
| 1,000,000 | mpfr_div | 0.014538284 |
| 1,000,000 | ppf_plan_batch_matched | 0.044369272 |
| 1,000,000 | mpfr_batch_matched | 0.051710691 |
| 5,000,000 | ppf_plan_build | 0.421622199 |
| 5,000,000 | ppf_cbrt | 0.116101927 |
| 5,000,000 | mpfr_cbrt | 0.067471271 |
| 5,000,000 | ppf_root7 | 0.333415315 |
| 5,000,000 | mpfr_root7 | 0.167063480 |
| 5,000,000 | ppf_rec_sqrt | 0.124451078 |
| 5,000,000 | mpfr_rec_sqrt | 0.055275651 |
| 5,000,000 | ppf_hypot | 0.074752070 |
| 5,000,000 | mpfr_hypot | 0.065771061 |
| 5,000,000 | ppf_div_direct | 0.044052892 |
| 5,000,000 | ppf_div_planned | 0.047593210 |
| 5,000,000 | mpfr_div | 0.050154815 |
| 5,000,000 | ppf_plan_batch_matched | 0.118994062 |
| 5,000,000 | mpfr_batch_matched | 0.104726221 |

## Comparisons

A speedup above one favors Polyphase. Single-operation rows are one-thread comparisons. The batch row is equal-worker throughput.

| Precision | Comparison | Speedup |
|---:|---|---:|
| 100,000 | cbrt_1t | 0.512x |
| 100,000 | root7_1t | 0.067x |
| 100,000 | rec_sqrt_1t | 0.337x |
| 100,000 | hypot_1t | 0.149x |
| 100,000 | direct_division_1t | 0.713x |
| 100,000 | planned_division_1t | 0.682x |
| 100,000 | planned_batch_equal_workers | 0.652x |
| 1,000,000 | cbrt_1t | 0.195x |
| 1,000,000 | root7_1t | 0.455x |
| 1,000,000 | rec_sqrt_1t | 0.160x |
| 1,000,000 | hypot_1t | 1.011x |
| 1,000,000 | direct_division_1t | 1.537x |
| 1,000,000 | planned_division_1t | 1.693x |
| 1,000,000 | planned_batch_equal_workers | 1.165x |
| 5,000,000 | cbrt_1t | 0.581x |
| 5,000,000 | root7_1t | 0.501x |
| 5,000,000 | rec_sqrt_1t | 0.444x |
| 5,000,000 | hypot_1t | 0.880x |
| 5,000,000 | direct_division_1t | 1.139x |
| 5,000,000 | planned_division_1t | 1.054x |
| 5,000,000 | planned_batch_equal_workers | 0.880x |
