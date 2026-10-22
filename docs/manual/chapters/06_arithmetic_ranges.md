# Arithmetic, rounding, and exponent ranges

A reliable calculation starts with three separate choices: what numbers the inputs represent, how many bits each destination will retain, and which rounding rule each operation will use. The context also determines the permitted exponent range. These choices answer different questions. More precision places representable values closer together; a wider exponent range allows larger and smaller magnitudes. Neither change can recover information already discarded when an input was created.

This chapter develops the numerical meaning of an arithmetic call and shows how to read its result. The complete example is [numeric_ranges.c](../../../examples/manual/numeric_ranges.c). It uses deliberately small precision so that the rounding decisions can be checked by hand.

## The destination determines the precision

For a regular value with precision \(p\), Polyphase Float stores an integer significand \(M\), a sign \(s\), and a normalized exponent \(e\). Its value is

\[
x=sM2^{e-p},\qquad 2^{p-1}\le M<2^p.
\]

Thus a regular magnitude lies in \([2^{e-1},2^e)\). Within this interval the spacing is \(2^{e-p}\). Precision counts significant binary bits, including the leading bit. It does not count decimal places after a decimal point.

In a call such as

```c
int t = ppf_div(&quotient, &numerator, &denominator,
                PPF_RNDN, &ctx);
```

`quotient` supplies the result precision. The inputs may have different precisions. Their stored values are the operands of the division; the library does not reinterpret them as unknown higher-precision originals. Increasing the context's default precision affects subsequent initialization using that default. It does not change the precision of objects that already exist.

For planning output, \(p\log_{10}2\) is a useful approximate count of significant decimal digits. For example, 128 bits correspond to about 38.5 decimal digits. This estimate is not an error bound for a long computation. Cancellation, input uncertainty, conditioning, and intermediate rounding still matter.

## Choose the rounding rule deliberately

The five deterministic rounding modes have straightforward geometric meanings. Consider rounding the positive exact value \(1/3\) to three binary bits. The neighboring values are \(5/16=0.3125\) and \(3/8=0.375\).

| Mode | Rule | Three-bit result for \(1/3\) |
|---|---|---:|
| `PPF_RNDN` | Nearest; a midpoint selects the even significand | 0.3125 |
| `PPF_RNDZ` | Toward zero | 0.3125 |
| `PPF_RNDU` | Toward positive infinity | 0.375 |
| `PPF_RNDD` | Toward negative infinity | 0.3125 |
| `PPF_RNDA` | Away from zero | 0.375 |

For negative values, upward still means toward positive infinity. It therefore moves a negative inexact result toward zero. Downward moves it away from zero. Thinking in terms of the number line avoids a common sign error when constructing bounds.

`PPF_RND_DEFAULT` requests the context's configured mode. Explicit modes are usually easier to review in numerical algorithms, especially when adjacent calls intentionally form lower and upper bounds.

`PPF_RNDF` requests faithful rounding. Code that needs a specified direction, an exactness decision from the return value, or reproducible choice between neighboring values should use a deterministic mode. Do not build a certificate from an assumed faithful ternary convention. The examples in this manual use explicit deterministic modes whenever they inspect the ternary result.

## Read the ternary value before interpreting flags

Most arithmetic functions return a ternary direction. With a deterministic rounding mode and a successful ordinary numerical result, zero means exact, positive means the stored result is greater than the exact mathematical result of that call, and negative means it is smaller. The magnitude of the integer is not an error estimate. A return value of 1 does not mean one bit or one unit in the last place was lost.

Check the failure sentinel first:

```c
int t = ppf_mul(&product, &a, &b, PPF_RNDN, &ctx);
if (t == PPF_TERNARY_ERROR) {
    /* Diagnose the failure; product is not a usable new answer. */
} else if (t == 0) {
    /* For an ordinary finite result, the multiplication is exact. */
} else {
    /* A valid rounded result; the sign of t gives its direction. */
}
```

Testing `if (t)` as an error condition rejects ordinary inexact arithmetic. Conversely, testing only `t < 0` confuses an error sentinel with a legitimate downward rounding direction. Special values also require their documented classification rules: a numerical NaN can be the specified result of a domain error rather than a failure to execute the API.

Not every integer-returning function uses this convention. Reuse an error-handling pattern only for functions with the same return contract.

| Interface | Return interpretation |
|---|---|
| Initialization functions and `ppf_set_str` | Status code; successful parsing is not a claim of exact numerical representation |
| `ppf_get_constant_interval` | Status: 0 for success, -1 for failure |
| `ppf_div_with_plan_many` | Batch status: 0 for success, -1 for failure; individual rounding directions are written to the optional `ternaries` array |
| `ppf_set_dyadic_interval` | `DECIDED`, `UNDECIDED`, or `ERROR`; its ternary is a separate output |
| `ppf_modf`, `ppf_sin_cos`, `ppf_sinh_cosh` | Packed two-output code on success, not one scalar ternary |

For a successful paired call in a deterministic mode, each output contributes a field: 0 means exact, 1 means rounded above its exact component, and 2 means rounded below it. The combined code is the first output's field plus four times the second output's field. The output order is integer/fractional for `ppf_modf`, sine/cosine for `ppf_sin_cos`, and hyperbolic sine/cosine for `ppf_sinh_cosh`. Check `PPF_TERNARY_ERROR` before decoding. A positive combined code can therefore include a downward-rounded component; its sign is not a rounding direction. Special-value cases still require numerical classification.

## Precision and range are independent

The context restricts regular exponents to `emin <= e <= emax`. The smallest positive regular value is

\[
2^{\mathrm{emin}-1},
\]

and, at precision \(p\), the largest positive finite value is

\[
(1-2^{-p})2^{\mathrm{emax}}.
\]

These formulas use the normalized exponent convention above. Treating `emin` as the exponent of the smallest power of two would introduce a factor-of-two mistake.

The default exponent range is intentionally much wider than a native `double` range. The absolute bounds accepted by the context API are exposed through the public constants and range-query functions. Wide representational limits do not imply that every algorithm can economically process every combination of precision and exponent. Some operations impose checked workspace or iteration limits.

Changing a context's range does not rewrite every object previously created with that context. Set the intended range before the calculation where possible. If applying a restricted range to a result calculated in a wider context, use `ppf_check_range` with the correct incoming ternary from that calculation. Inventing an incoming zero discards information needed to avoid a second-rounding error at a boundary.

## A range example you can verify by hand

The example initializes two eight-bit inputs and a two-bit destination, then sets the context range to [-9,9]. It computes

\[
\frac{7}{15\,2^{-10}}=\frac{7168}{15}\approx477.8667.
\]

The largest finite destination value is \(3\cdot2^7=384\). The next value in an unbounded exponent system is 512. Their midpoint is 448, so nearest rounding chooses 512 before the exponent restriction is applied. Since that exponent is outside the context range, the final result is positive infinity with overflow and inexact flags.

Toward-zero rounding selects 384 directly. That rounded value fits the exponent range, so this call raises inexact without overflow. This example illustrates why comparing the exact magnitude with the largest finite value is insufficient to predict every range flag: the rounding mode matters.

The program prints:

```text
Nearest quotient: +infinity; overflow and inexact.
Toward-zero quotient: 384; inexact only.
Exact subnormal emulation: underflow without inexact.
```

The final line comes from a separate subnormal-emulation calculation described below. Every line is printed only after the program checks the value, return direction, and relevant flags. A failed check produces a nonzero process exit.

## Flags describe events, and they accumulate

Context flags are sticky. An inexact flag can remain set after a later exact operation. Save or clear flags at a meaningful boundary, perform the operation or group of operations, and then inspect the resulting mask. In an explicit-context program, use the `ppf_context_flags_*` functions with that same context. The short forms operate on the calling thread's default context.

| Flag | Practical interpretation |
|---|---|
| `PPF_FLAG_INEXACT` | A rounded operation recorded loss of exact representability |
| `PPF_FLAG_UNDERFLOW` | The operation encountered its underflow condition; subnormal emulation has a specific rule |
| `PPF_FLAG_OVERFLOW` | The rounded exponent exceeded the permitted range |
| `PPF_FLAG_DIVBY0` | A documented pole or division-by-zero case occurred |
| `PPF_FLAG_NAN` | An invalid numerical input or domain case produced NaN |
| `PPF_FLAG_ERANGE` | A request could not be completed under the applicable representation, domain, or certification limits |
| `PPF_FLAG_ALLOC` | A request to the configured Polyphase allocator failed |

An inexact result is normal for division by three or evaluation of an irrational constant. It is usually not an application error. Allocation failure, a domain error, and an exhausted certification path require different responses even if more than one flag is present. Record the requested operation and its input policy alongside the flags; the mask alone is not a complete diagnostic message.

## Emulating subnormal arithmetic

A `ppf_t` has regular values and signed zero, but no separate stored subnormal category. `ppf_subnormalize` emulates a fixed-point region by reducing effective precision according to the current exponent and `emin`. In that region the spacing becomes the fixed quantum \(2^{\mathrm{emin}-1}\).

The function needs the rounding mode and ternary from the operation that produced its input. It uses that information when a second rounding would otherwise choose the wrong side of a midpoint. Its returned ternary describes the final value relative to the original exact result, not merely relative to the intermediate input.

When the subnormal step actually reduces effective precision, it raises underflow even when the value remains exact. This follows the `mpfr_subnormalize`-style API rule. In the example, precision is two, `emin` is -30, and the input is exactly \(2^{-31}\). The value remains unchanged and the ternary is zero, yet underflow is set. Inexact remains clear. Do not apply the ordinary “underflow must imply inexact” shortcut to this API.

## Select arithmetic that matches the expression

Use a fused operation when the mathematical specification calls for one rounding of a combined expression. `ppf_fma` evaluates \(ab+c\) with a single final rounding; separate multiplication and addition round an intermediate product. Near cancellation, these are different numerical computations. The same design consideration applies to fused two-product operations, sums, dot products, and `ppf_hypot`.

For roots, select the operation that expresses the intended degree and sign convention. Odd roots of negative real values are meaningful; even roots of negative real values are not real. Negative degrees introduce reciprocals and their zero-domain conditions. The reference distinguishes `ppf_rootn_ui`, `ppf_rootn_si`, and the older `ppf_root` interface, including their signed-zero conventions.

Aliasing is a separate contract. Many scalar operations support replacing an input with their output, but this does not authorize arbitrary overlap in paired or batched operations. Use the documented alias pattern, especially when outputs may be written concurrently. Numerical correctness, supported memory overlap, and successful allocation are three requirements that must all hold for a reliable call.
