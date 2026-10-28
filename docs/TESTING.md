# Testing

Use a separate build directory for each configuration. The full differential
suite requires the official GMP and MPFR development headers and libraries.
Follow [Building](BUILDING.md#strict-full-validation) to configure it.

## Choose the test scope

CTest labels overlap: one test may appear under several labels. The exact
inventory depends on enabled dependencies and build options.

| Label | Purpose |
|---|---|
| `fast` | Routine object, arithmetic, conversion, I/O, range, numerical, runtime, planner, and contract checks |
| `semantic` | Generated API and aliasing contracts, direct public-surface checks, and focused lifecycle tests |
| `hardening` | Allocation-failure injection, resource boundaries, lifecycle checks, and ABI checks |
| `threading` | POSIX-thread ownership, context isolation, cache lifecycle, and cleanup |
| `extended` | Randomized, exhaustive small-precision, and large-precision numerical suites |
| `smoke` | Small benchmark and tool runs that check execution and evidence schemas |
| `packaging` | Installed and relocated downstream consumers |

For a routine local check:

```sh
ctest --test-dir build/validation -L fast --output-on-failure
```

For the complete configured suite, including slower numerical tests:

```sh
ctest --test-dir build/validation --output-on-failure -j1
```

List exactly what your build contains, or choose a focused label:

```sh
ctest --test-dir build/validation --show-only
ctest --test-dir build/validation -L hardening --output-on-failure
ctest --test-dir build/validation -L extended --output-on-failure
```

A core-only build has fewer tests because MPFR-dependent tests are absent. A
passing subset is evidence for that subset. Use the complete
[qualification matrix](QUALIFICATION.md) for release validation. A smoke test establishes executable and
output-schema behavior, not performance.

## Allocation and boundary tests

The cumulative suite includes the original core allocation-failure checks,
text and interchange transactions, certified-constant transactions, 79
transcendental/special endpoints, and 56 arithmetic/fused/root endpoints.
`allocation_completion` exercises the additional allocation-free,
initialize-and-set, prefix-cleanup, and transaction classifications.
`api_lifecycle_audit` and `precision_remainder_failure` exercise focused
ownership and failure cases.

When the required static libraries and MPFR adapter are built,
`release_boundaries` checks exact Airy admission, floating-setter validation,
and forced certification-retry exhaustion. `special_interval_audit` checks
selected interval constructions. PPFIF resource-boundary tests cover import
admission and destination preservation on rejection.

See [Allocation evidence](ALLOCATION_COMPLETION_0.20.0.md),
[PPFIF resource limits](FPIF_RESOURCE_CONTRACT.md), and
[Thread safety](THREAD_SAFETY.md) for the applicable contracts. Project
allocator checks do not exercise recoverable GMP-internal allocation failure.

## Contract and installed-consumer checks

```sh
cmake --build build/validation --target semantic-contract-audit
cmake --build build/validation --target abi-contract-audit
```

The semantic reference and direct test-source references describe coverage.
They do not establish exhaustive input, branch, rounding, or failure coverage.
See the [semantic contract](PUBLIC_SEMANTIC_CONTRACT_0.20.0.md) and
[aliasing contract](ALIASING_CONTRACT_0.20.0.md).

`install_packaging` creates a separate serial static installation under a
nested library directory, moves the complete prefix, and compiles external
CMake and `pkg-config` consumers. It checks core-only consumption with MPFR
discovery disabled, default package behavior, and the adapter component when
available.

`tools/run_abi_binary_compat.py` builds the frozen 0.11.0 source and compiles a
consumer against its header and shared object. It then runs that unchanged
executable with the baseline and current libraries. This is separate from the
source-contract, export, and runtime-layout checks. See [ABI policy](ABI_POLICY.md).

## Sanitizers and fuzzing

[Release qualification](QUALIFICATION.md) defines the reproducible GCC, Clang,
OpenMP, sanitizer, and fuzz matrix. From a clean source extraction:

```sh
python3 tools/qualify_release.py --output ../polyphase-qualification --with-sanitizers
```

Keep the source frozen during this run and retain the output outside it. The
receipt records commands, source hashes, actual outcomes, and omitted scopes.
The default fuzz campaigns run for 600 seconds per target. Extend the duration
when collecting longer-term evidence.

The qualification sanitizer configuration runs the hardening tests under
ASan and UBSan, with the Python `ctypes` export check excluded as explained in
[Qualification](QUALIFICATION.md). **LeakSanitizer is disabled by default**.
Use `--detect-leaks 1` on a host that supports it. Allocator balance assertions
and ASan/UBSan passes must not be described as a LeakSanitizer run.

A separate serial ThreadSanitizer configuration is available:

```sh
cmake --preset tsan
cmake --build --preset tsan -j
ctest --preset tsan
```

ThreadSanitizer is not included in the default qualification command. Record
its actual result separately. An unavailable runtime is not a pass.

## Interpreting results

A passing differential test demonstrates agreement for the exercised inputs,
precisions, rounding modes, exponent ranges, and special values. Exact
identities, invariants, aliasing tests, allocation injection, sanitizers, and
fuzzing provide complementary evidence.

The current ledger has no remaining indexed allocation-evidence gaps. It
still records bounded numerical paths and compiler-conditional interfaces.
The [release scope](COMPLETION_SCOPE_0.20.0.md) states those limits, and the
[roadmap](../ROADMAP.md) separates future safety and independent-review work
from completed local checks.

[Documentation](README.md) · [Manual](manual/README.md) · [Building](BUILDING.md) · [Qualification](QUALIFICATION.md)
