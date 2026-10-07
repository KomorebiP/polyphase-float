# Initialization, assignment, and conversion

This layer makes the independent floating representation usable without first
constructing an MPFR object. All ordinary calculations terminate in `ppf_t` and
use the independent rounding, range, and flag machinery.

## Initialization

The library supports explicit precision, thread-local default precision,
variadic initialization, variadic clearing, precision changes, and exact
structure swaps. Every initialized object owns its allocation and remains valid
independently of the context that created it.

## Assignment

Assignment is available from:

- another `ppf_t`
- signed and unsigned C integer types
- `float`, `double`, and `long double`
- GMP `mpz_t`, `mpq_t`, and `mpf_t`
- integer values multiplied by a binary power
- strings in bases 2 through 62

Rational and text inputs are represented exactly as an integer numerator and
positive integer denominator before the one final binary rounding decision.
The parser supports leading space, an optional sign, binary and hexadecimal
prefixes under base detection, radix points, base exponents, binary exponents
for bases 2 and 16, infinities, signed NaNs, and partial parsing through an end
pointer.

## Conversion

The implemented outputs are:

- C signed and unsigned integer types, with saturation and range flags
- `float`, `double`, and `long double`, including subnormal rounding
- normalized mantissa/exponent pairs
- exact GMP integer significand/exponent pairs
- rounded GMP integers
- exact GMP rationals
- rounded GMP floats using their exponent-aligned effective precision
- arbitrary-base significand strings
- fit predicates for eight C integer types

The GMP floating conversion deserves special treatment. A GNU `mpf_t` has an
integer number of significand limbs, but its usable binary precision depends on
its exponent alignment. The conversion rounds first to that effective
precision and writes the aligned limbs directly. This avoids retaining an
unintended guard limb and reproduces the MPFR ternary result.

## Portable scope

The portable layer covers every nonconditional initialization, assignment,
combined-initialization, conversion, and fit-test entry in the MPFR inventory.
The binary128, decimal64, and decimal128 interfaces are conditional on compiler
and library support and are not enabled in the baseline build.
