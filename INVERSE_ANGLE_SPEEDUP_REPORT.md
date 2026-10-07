# Inverse-angle and dense-phase speed refinement

This pass targets the slowest calls left in the trigonometric layer without broadening the public API. The retained changes are:

1. a cubic inverse-angle correction for `atan`;
2. a shared exact-ratio implementation for `atan2`;
3. direct certified tangent intervals inside the inverse-angle correction;
4. density-aware Taylor block selection for dense and pi-scaled arguments.

The production library remains independent. It does not call MPFR to perform an implemented operation. MPFR is used only in the optional interoperability library and in differential tests and benchmarks.

## Cubic inverse-angle refinement

Let

\[
r=\frac{A}{B}>0,
\qquad
\alpha=\arctan r,
\]

and let \(y\) be an approximation to \(\alpha\). If \(t=\tan y\), then

\[
\alpha-y
=
\arctan\!\left(\frac{A-Bt}{B+At}\right).
\]

Define

\[
u=\frac{A-Bt}{B+At}.
\]

The update

\[
y\leftarrow y+u
\]

is cubically convergent because

\[
\arctan u=u-\frac{u^3}{3}+O(u^5).
\]

The implementation starts from a hardware `long double` seed and follows a precision ladder growing by roughly a factor of three. The final correction is certified rather than assumed. For \(|u|\le 1\):

\[
u-\frac{u^3}{3}\le \arctan u\le u \quad (u\ge0),
\]

\[
u\le \arctan u\le u-\frac{u^3}{3} \quad (u\le0).
\]

Both endpoints are propagated to the destination. A result is accepted only when they prove the same rounded value and ternary direction.

## Direct tangent enclosure

Earlier prototypes called the complete public `tan` operation during refinement. That repeated argument handling and outer correct-rounding machinery. The retained implementation calls the shared reduced sine/cosine kernel directly and forms

\[
\tan y\in
\left[
\frac{\sin_-(y)}{\cos_+(y)},
\frac{\sin_+(y)}{\cos_-(y)}
\right].
\]

This is both faster and a stronger certificate than rounding one tangent value and expanding it by neighboring floats.

## Shared exact-ratio `atan2`

For finite nonzero inputs, `atan2(y,x)` now lowers to one positive ratio

\[
\alpha=\arctan\frac{|y|}{|x|}
\]

and one exact quadrant transformation:

\[
\begin{array}{c|c}
(x,y) & \operatorname{atan2}(y,x)\\
\hline
(+,+) & +\alpha\\
(+,-) & -\alpha\\
(-,+) & \pi-\alpha\\
(-,-) & -\pi+\alpha.
\end{array}
\]

The old path independently constructed interval objects for the ratio, arctangent, pi, scaling, and final sum. The new path shares the inverse-angle engine and requests pi only for the two left-half-plane quadrants.

## Density-aware Taylor blocks

The fixed-point sine/cosine evaluator now examines the odd-part bit length of its reduced dyadic input. Sparse inputs, medium-density inverse-angle iterates, and dense pi-scaled inputs do not use the same optimal rectangular block.

The retained policy is:

- small precision: blocks of 8 or 10 terms;
- middle precision: 20 terms;
- 35,000 to 75,000 bits: 24 terms for dense inputs and 32 for medium-density inputs;
- above 75,000 bits: 36 terms for dense inputs and 28 for medium-density inputs.

These are conservative portable defaults. They should still be tuned on important target processors.

## Same-host before and after

Both packages were rebuilt on the same machine with the same compiler and GMP/MPFR installation. Each row is the median of five alternating-order executions.

| Precision | Operation | Prior path | Refined path | Internal speedup |
|---:|---|---:|---:|---:|
| 20,000 bits | `sinpi(0.3)` | 3.010 ms | 2.877 ms | 1.046x |
| 20,000 bits | `atan(0.3)` | 6.821 ms | 5.236 ms | 1.303x |
| 20,000 bits | `atan2(0.75,-0.25)` | 7.388 ms | 5.043 ms | 1.465x |
| 50,000 bits | `sinpi(0.3)` | 17.057 ms | 15.553 ms | 1.097x |
| 50,000 bits | `atan(0.3)` | 37.658 ms | 19.611 ms | 1.920x |
| 50,000 bits | `atan2(0.75,-0.25)` | 45.562 ms | 19.604 ms | 2.324x |
| 100,000 bits | `sinpi(0.3)` | 71.249 ms | 57.173 ms | 1.246x |
| 100,000 bits | `atan(0.3)` | 118.662 ms | 71.204 ms | 1.667x |
| 100,000 bits | `atan2(0.75,-0.25)` | 141.848 ms | 69.821 ms | 2.032x |

The complete raw tables are in `results/inverse_angle_speedups/`.

## Current MPFR boundary

The changes are large internal wins, but MPFR remains faster for these calls in the current one-thread tests. At 100,000 bits, the refined MPFR/Polyphase ratios were approximately:

- `sinpi`: 0.428x;
- `atan`: 0.360x;
- `atan2`: 0.373x.

A ratio below one means MPFR is faster. The present result should therefore be described as a major narrowing of the independent implementation gap, not a broad MPFR performance victory.

## Failed experiments retained as boundaries

The following exact or certified variants were implemented and rejected:

- stable `log1p` rewrites of `asinh`, `acosh`, and `atanh`; correct but not faster;
- dense exact binary splitting for sine/cosine; much slower at the tested dense numerators;
- a two-phase dense-angle split into a short high phase and tiny low phase; rigorous but slower because binary-splitting and recombination overhead dominated;
- globally large Taylor blocks; the best block depends on precision and dyadic density;
- full public `tan` calls inside the cubic correction; correct but duplicated high-level certification work.

These failures narrow the next research target. The remaining inverse-angle cost is dominated by evaluating tangent for a dense, approximately one-third-precision iterate at full destination precision. A competitive next kernel needs a purpose-built dense tangent or bit-burst angle-addition method, not another layer of general floating interval objects.
