# Format-security patch validation for 0.10.1

## Trigger

A release-core build performed in a WSL environment inherited the common
distribution hardening flags `-Wformat -Werror=format-security`. GCC rejected
two zero-argument calls of the form `snprintf(..., native_spec)` in the native
`%m` path. The internal parser constrained `native_spec`, but the compiler
could not prove that property.

## Repair

The `%m` conversion no longer passes a dynamic format string to a variadic
function without data arguments. It now:

1. captures the active errno value;
2. resolves the ordinary error text or GNU alternate symbolic name;
3. applies precision and left/right space padding with bounded builder
   operations;
4. restores errno after successful formatting.

On glibc 2.35 and newer, `%#m` retains symbolic names and the native behavior
for unknown errno numbers, including the fact that numeric fallback ignores
string precision. Other supported `%m` flags continue to match the host C
library.

## Regression coverage

`tests/test_format_errno.c` is independent of MPFR and compares Polyphase
output with the host `asprintf` implementation for:

- ordinary, zero, known, and unknown errno values;
- width and left alignment;
- precision;
- zero, plus, space, and grouping flags;
- GNU alternate `%#m`;
- combined alternate, width, alignment, and precision.

The `hardening` CMake preset and the independent-core CI job compile with
`-Wformat -Werror=format-security` and warnings-as-errors.

## Scope

The patch changes only native formatted-output handling and its test/build
coverage. Arithmetic, rounding, constant, transcendental, special-function,
parallel-product, and serialization kernels are unchanged.
