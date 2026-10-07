# Failed and conditional inverse-angle speed experiments

## Stable inverse-hyperbolic identities

Tested identities included

\[
\operatorname{asinh}a=\log1p\!\left(a+\frac{a^2}{1+\sqrt{1+a^2}}\right),
\]

\[
\operatorname{acosh}x=\log1p\!\left((x-1)+\sqrt{(x-1)(x+1)}\right),
\]

and

\[
\operatorname{atanh}a=\frac12\log1p\!\left(\frac{2a}{1-a}\right).
\]

They were correct and numerically stable, but the extra interval arithmetic offset the `log1p` advantage. They were rejected from production.

## Dense binary splitting

Relaxing the numerator-size gate allowed exact binary splitting on dense reduced angles. Runtime increased dramatically, reaching seconds in cases where the retained fixed-point recurrence took milliseconds. Rejected.

## Two-phase dense angle

The reduced angle was split exactly as \(r=r_H+r_L\), with a short high numerator and tiny low remainder. Each phase was evaluated independently and recombined with exact angle-addition bounds. The method was correct but 4 to 6 times slower at 20,000 bits and increasingly worse at 50,000 and 100,000 bits. The high-phase product tree and full-precision recombination cost dominated. Rejected.

## Universal block size

Block sizes 16, 20, 24, 28, 32, 36, 40, 64, 96, and 128 were tested. No universal winner exists. The retained density-aware planner was chosen from the observed crossover pattern.
