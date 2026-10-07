# Trigonometric and hyperbolic boundaries

This block was built as a correctness-first implementation. Several tempting shortcuts were deliberately not promoted to production behavior.

## Independent paired calls

`sin_cos` and `sinh_cosh` currently use the validated individual functions rather than sharing all internal work. This is correct and preserves aliasing and return encoding, but repeats argument reduction or exponential evaluation. It is retained as the conservative baseline until a shared paired kernel passes the same differential and range tests.

## Full floating interval arithmetic in every series step

The ordinary trig and arctangent series presently allocate and normalize complete `ppf_t` endpoint objects throughout the recurrence. This made the proof structure straightforward, but development timing shows that the representation overhead dominates the arithmetic. The next implementation should use a fixed-point endpoint backend rather than merely adjusting Taylor term counts.

## Naive large-angle materialization

A direct reduction that materializes every bit required by an arbitrarily large input exponent can consume impractical memory. The production path has a configured reduction-extra limit and returns an explicit error when the requested operation exceeds that practical limit. It never calls MPFR as a fallback.

## Universal use of exp formulas

The hyperbolic formulas based on `exp(-|x|)` are stable for the ordinary range, but a universal implementation would attempt to represent exponentially tiny values at enormous internal precision. Proven saturation and range shortcuts are therefore required and retained.

## Performance conclusion

The baseline proves that all 35 functions can be implemented independently with correct rounding and complete special-value semantics. It does not yet establish a speed advantage. The main negative result is that full `ppf_t` interval composition is the wrong final performance substrate for periodic functions, just as it was for the first logarithm and exponential implementation.
