# Progress after range control, roots, and reusable division

## What can now be done independently

The library can perform this complete path without MPFR:

1. initialize arbitrary-precision binary objects;
2. assign from C values, GMP values, rationals, or text;
3. configure rounding, exponent range, and exception flags;
4. calculate with independent arithmetic;
5. calculate square, reciprocal, cube, and general roots;
6. reuse one exact denominator across many correctly rounded divisions;
7. compare, round, split, reduce, and select values;
8. emulate a fixed-point/subnormal range when needed;
9. convert results back to C, GMP, rational, or text form.

MPFR is confined to the optional interoperability library and the test oracle.

## Completed major layers

- owned normalized floating-point representation
- contexts, allocation, scratch storage, and flags
- central exact rounding and certified interval decisions
- complete portable assignment and conversion foundation
- core binary arithmetic and fused reductions
- complete comparison/classification family
- complete integer-rounding/remainder family
- complete exponent-range and flag-control family
- complete public root family represented in the arithmetic inventory
- reusable-denominator division plans
- Make, CMake, sanitizer, installation, and binary-independence validation

## Public coverage

The public header contains 224 distinct `ppf_*` callables. Of these, 182 map
directly to the 349-symbol implementation inventory:

\[
\frac{182}{349}=52.15\%.
\]

The completed inventory families are:

- initialization and precision
- combined initialization and assignment
- comparison and classification
- integer rounding and remainder
- exponent range and exception flags

The portable assignment/conversion foundation is complete apart from six
compiler-dependent binary128 and decimal interfaces.

## Performance boundary

The reusable-plan path demonstrates that stateful arithmetic can beat a
stateless baseline when expensive denominator work is amortized. In the latest
development sweep:

- one-million-bit planned division was about 1.69 times MPFR;
- the equal-four-worker planned batch was about 1.17 times MPFR;
- five-million-bit direct division was about 1.14 times MPFR;
- the five-million-bit planned batch was about 0.88 times MPFR.

This is a useful boundary. A plan should not be built blindly. The expected
number of applications and target precision must guide the choice.

The root family currently ranges from roughly 0.16 to 0.88 times MPFR in the
large development rows, depending on the operation. Its value in this
milestone is semantic completeness and independence. Its next performance
step should replace full integer scaling and generic `mpz_rootrem` with
precision-doubling Newton iterations and direct high products.

## Current engineering assessment

- independent numerical foundation: 90–95 percent complete;
- broad binary arithmetic and utility library: 70–80 percent complete;
- literal MPFR-style public surface: 52.15 percent;
- total effort toward a mature full MPFR replacement: roughly 30–40 percent.

The total-work estimate remains below the symbol percentage because correctly
rounded transcendental and special-function families are much harder per
function than flags, predicates, or assignments.

## Next major milestone

The next coherent step is to move the certified constants directly into
`ppf_t` and build their independent caches:

- pi through the existing Polyphase-Chudnovsky interval
- log(2) through the exact packet enclosure
- Euler's constant and Catalan's constant as later additions
- reusable `can_round`/precision-escalation infrastructure

That will establish the constant and Ziv-loop machinery needed before the
first general `log`, `exp`, and power family.
