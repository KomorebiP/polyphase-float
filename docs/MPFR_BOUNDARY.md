# MPFR boundary

The floating object library itself links only GMP and the C runtime. MPFR support is isolated in a separate adapter.

The adapter provides:

- exact import at the source MPFR precision
- rounded import into an existing Polyphase object
- export into an initialized MPFR destination
- rounding-mode translation

This separation is intentional. Existing applications can continue using `mpfr_t` at their public boundary while calculations gradually move to the independent representation. Once a calculation enters the independent representation, intermediate operations no longer need repeated MPFR-to-GMP and GMP-to-MPFR conversion.
