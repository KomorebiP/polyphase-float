# Official MPFR header compatibility fix for 0.10.2

## Trigger

The full differential build uses the official MPFR development header when it
is installed. MPFR 4.2.x marks the legacy `mpfr_root` declaration as
deprecated. With `PPF_WARNINGS_AS_ERRORS=ON`, the direct reference call in
`tests/test_roots.c` therefore stopped the build before the test executable was
linked. Environments using the repository's minimal ABI declarations did not
reproduce the warning because that declaration previously lacked the
deprecation attribute.

## Correction

The root test now computes the legacy reference result without invoking the
deprecated declaration. For a nonzero degree and a zero operand it copies the
operand, preserving the sign of zero. All other cases use `mpfr_rootn_ui`. This
is the semantic distinction required by the legacy `mpfr_root` interface.

The test-only minimal MPFR header now marks `mpfr_root` and `mpfr_grandom` as
deprecated on GCC and Clang. The public release audit also rejects direct calls
to those interfaces in C validation sources. These two controls make a local
minimal-header build and an official-header build agree on the warning policy.

## Scope

Only tests, test-only compatibility declarations, release auditing, metadata,
and runner diagnostics changed. No file under `src/` changed.
