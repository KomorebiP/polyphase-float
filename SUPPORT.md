# Support

Use the public issue tracker for reproducible build failures, API defects,
numerical mismatches, documentation errors, and benchmark-methodology issues.

Before filing an issue, include the output of:

```sh
cmake --version
cc --version
pkg-config --modversion gmp
pkg-config --modversion mpfr 2>/dev/null || true
```

Also include the exact configure command, failing test name, precision,
rounding mode, and a minimal reproducer. Security-sensitive reports belong in
the private process described in `SECURITY.md`.
