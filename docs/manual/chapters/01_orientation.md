# Finding your way

Polyphase Float is a C library for binary floating-point arithmetic with a precision you choose for each number. It provides arithmetic, roots, mathematical constants and functions, conversions, and input/output through the `ppf_t` number type. A calculation can use more significand bits than the machine's ordinary `double`, and it can choose how each result is rounded.

This book takes you from a working installation to calculations whose precision, ownership, and error handling you can explain. The first five chapters form a practical introduction. Later chapters develop the arithmetic families, numerical limits, interchange formats, and advanced interfaces. The reference entries are there when you need the exact declaration or contract of an individual function.

You do not need to understand the implementation's multiplication algorithms to use the library. You do need to distinguish a number's precision from its printed length, initialize and clear owned objects, and interpret return values correctly. Those three habits account for much of the introductory material.

## Choose a reading route

| Your starting point | Suggested route |
|---|---|
| New to C numerical libraries | Build and run the supplied example, read objects and contexts, then work through all four introductory programs. |
| Comfortable with C, new to arbitrary precision | Read the precision discussion below and the rounding chapter before adapting the first program. |
| Moving from another multiprecision library | Read ownership, explicit versus default contexts, return-value conventions, and the function reference for each migrated call. |
| Integrating an existing application | Start with the installed CMake consumer, then review thread ownership, error handling, and the exact contracts of your operation families. |
| Investigating an unexpected answer | Check the original input, destination precision, intermediate rounding, flags, and result category in that order. |

The examples deliberately use small, inspectable calculations. Their simplicity makes API mistakes visible. Once those patterns are familiar, use the same initialization, return checking, and cleanup structure around a larger algorithm.

If you are reading a PDF, the source links identify complete files in the accompanying source tree. Commands run from the source directory unless a section explicitly changes directory. Shell prompts such as `$` are omitted so that commands can be copied directly.

## The four ideas behind a calculation

A typical operation combines four things:

1. **Operands** contain the values already stored in the input objects.
2. **A destination** supplies the precision of the result.
3. **A rounding mode** chooses a representable answer when the exact result does not fit.
4. **A context** supplies the exponent range, default policy, allocation callbacks, and diagnostic flags.

For example, `ppf_mul(&result, &a, &b, PPF_RNDN, &ctx)` multiplies the stored values of `a` and `b`, rounds into the existing precision of `result`, and uses `ctx` for the calculation. The `&` operator passes the address of an object. The first argument is the object to be written; the following number arguments are inputs.

An operand's high precision does not automatically make the destination high precision. A 256-bit input multiplied into a 32-bit destination produces a 32-bit result. The context's default precision does not resize an existing destination either. Choosing the destination is part of choosing the numerical calculation.

## Bits, decimal digits, and magnitude

Precision is measured in **binary significand bits**. It describes the amount of significant binary information a regular value can hold, not the number of digits after the decimal point. The exponent controls magnitude separately. A very large value and a very small value can therefore use the same precision.

A useful conversion is

\[
\text{decimal-digit equivalent} \approx p\log_{10}(2),
\]

where \(p\) is the number of significand bits.

| Precision | Approximate decimal-digit equivalent |
|---:|---:|
| 24 bits | 7.22 |
| 53 bits | 15.95 |
| 80 bits | 24.08 |
| 128 bits | 38.53 |
| 256 bits | 77.06 |

These are measures of storage resolution. They are not guarantees that an arbitrary calculation has that many correct decimal digits. Inaccurate input, cancellation, and an ill-conditioned problem can reduce the useful accuracy of a result.

To budget space for roughly \(d\) decimal significant digits, start with \(\lceil d\log_2(10)\rceil\) bits, then allow additional working precision according to the algorithm. For example, 50 decimal digits correspond to about 167 bits before any allowance for intermediate error. There is no universal number of extra bits that makes every algorithm reliable.

Some values are exactly representable in a finite binary significand. One half is exact because it is a power of two. One tenth and one third have nonterminating binary expansions, so storing them at a finite precision generally involves rounding. Printing more decimal digits can reveal the stored approximation; it cannot create missing information.

The default precision is 53 bits. That familiar size does not make every aspect of the object model identical to a C `double`. Polyphase Float has its own exponent range, explicit rounding controls, flags, and special-value rules.

## What the core depends on

The production core uses GMP for integer storage and arithmetic support. MPFR is optional: it supports an interoperability adapter and differential tests. You can build and use the core without an MPFR runtime dependency. The build chapter starts with that smaller configuration.

The supported baseline is Linux x86-64 with the LP64 data model and 64-bit GMP limbs. LP64 means that pointers and C `long` integers are 64 bits. Windows readers can follow the Linux workflow inside an x86-64 Ubuntu environment under WSL; native Windows toolchains are outside this baseline. The installation chapter makes the shell boundary explicit.

The public API includes finite-precision arithmetic and bounded algorithms. Some mathematical-function implementations can reject requests outside their practical limits or after a finite refinement budget. The relevant reference entries describe those limits and failure behavior. A wide representable exponent range does not imply that every mathematical function can successfully evaluate every representable argument.

## Read results at three levels

When an operation finishes, ask three separate questions:

- **Did the operation report an unrecovered failure?** Ternary-returning operations use `PPF_TERNARY_ERROR` for this purpose.
- **What kind of result was produced?** A result may be regular, zero, infinity, or NaN.
- **What numerical diagnostics were raised?** Context flags record conditions such as inexact rounding, overflow, or division by zero.

A nonzero ternary result is usually a rounding direction, not a failure. Conversely, a special value may be produced through the documented arithmetic rules without returning `PPF_TERNARY_ERROR`. The rounding chapter develops both distinctions with runnable examples.

The word **exact** also needs a reference point. If an input string has already been rounded into an eight-bit object, copying that stored value to a 128-bit object may be exact. The copy still contains the original eight-bit approximation. Exactness of one operation does not recover information lost earlier.

## A small vocabulary

A **significand** carries the significant digits of a number; an **exponent** scales that significand by a power of the radix. The **radix** is the numeral-system base, such as two or ten. A **destination** is the initialized object an operation writes. A **rounding mode** chooses among nearby representable results. A **ternary result** describes whether the stored result is below, equal to, or above the exact result of the operation. A **sticky flag** remains set until explicitly cleared or restored. An **owned object** is responsible for storage that must eventually be released.

These terms recur because they describe different responsibilities. Precision belongs to a number; flags belong to a context; storage belongs to an owner; and mathematical accuracy belongs to the complete calculation.

## Before moving on

You are ready to begin when you can identify the source directory, choose a Linux build environment, and set aside a separate build directory. In the next chapter you will compile the library, run an example, and link a small application against an installed package. You can return to the numerical discussion after seeing that first program run.

**Check your understanding.** Does requesting 40 decimal digits of printed output make a 53-bit number more precise?

**Answer.** No. It changes the text representation. To retain more information, initialize the destination at a higher binary precision and compute or parse the value at that precision. Widening an existing approximation alone does not recover its discarded information.
