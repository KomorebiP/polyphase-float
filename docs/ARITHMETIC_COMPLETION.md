# Independent arithmetic-family completion

## Result

The complete 54-function arithmetic family is now present on the independent `ppf_t` representation.

This step adds 32 previously missing public counterparts:

```text
add_ui add_si add_d add_z add_q
ui_sub sub_ui si_sub sub_si d_sub sub_d z_sub sub_z sub_q
mul_ui mul_si mul_d mul_z mul_q
ui_div div_ui si_div div_si d_div div_d div_z div_q
mul_2ui div_2ui fac_ui fmma fmms
```

The production shared library still links no MPFR symbols. MPFR is used only by the optional bridge and external differential tests.

## Exact scalar lowering

Unsigned integers, signed integers, GMP integers, and finite C doubles are first represented exactly in temporary independent objects with an extended exponent context. The existing independent operation is then applied once at the destination precision.

This prevents a scalar from being rounded prematurely to the destination before the requested operation.

## Exact rational variants

For a finite binary operand `x` and exact GMP rational `q`, the rational variants form the exact mathematical expression in `mpq_t`:

\[
x+q,\quad x-q,\quad xq,\quad \frac{x}{q}.
\]

Only the completed rational result is converted back to binary floating point. Thus the operation has one final rounding rather than a rounded scalar conversion followed by a second arithmetic rounding.

The implementation includes signed-zero rules, division by rational zero, infinities, NaNs, and destination/input aliasing.

## Exact two-product fusion

The new fused operations calculate

\[
ab+cd
\]

and

\[
ab-cd
\]

as two exact dyadic product terms. The terms are phase-aligned exactly, combined, and rounded once.

The implementation handles every destination/input alias arrangement, signed zeros, infinities, NaNs, and the invalid `0 * infinity` combinations.

## Factorial and scaling

`ppf_fac_ui` calculates the exact integer factorial through GMP and performs one final independent binary rounding.

The unsigned power-of-two scaling functions cover the complete `unsigned long` domain. Shifts larger than `LONG_MAX` are resolved through range semantics without attempting an impossible allocation.

## Development performance

The correctness-gated benchmark is stored under `results/arithmetic_completion`.

The development machine showed:

- `mul_z` at approximately 1.99 times MPFR throughput at 100,000 bits;
- `fmma` at parity at 100,000 bits;
- `fac_ui` substantially faster in every measured row;
- most temporary-based scalar and rational variants slower than MPFR, particularly at small precision.

The primary accomplishment of this block is semantic and API completion. The slower scalar wrappers remain clear future speed targets rather than being represented as performance wins.

## Current boundary

The arithmetic family is complete by public function name and differential behavior over the tested domain. Performance is uneven:

- exact GMP-integer multiplication and factorial benefit from the existing independent integer backend;
- exact rational variants currently materialize full rational intermediates;
- scalar integer and double calls currently allocate a temporary independent object;
- `fmma` and `fmms` do not yet expose internal product-channel threading.

These are optimization boundaries, not correctness fallbacks. No operation delegates to MPFR.
