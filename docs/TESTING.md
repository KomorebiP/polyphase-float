# Testing

## Test categories

The configured validation tree contains six CTest categories.

### Fast

`fast` covers ordinary object, arithmetic, conversion, I/O, formatting, range,
root, constant, transcendental, special-function, runtime, conditional-format,
and planner behavior.

```sh
ctest --test-dir build/validation -L fast --output-on-failure
```

### Semantic

`semantic` verifies the generated all-function contract and executes the direct
surface test that covers public names not otherwise referenced by the existing
family suites.

```sh
ctest --test-dir build/validation -L semantic --output-on-failure
cmake --build build/validation --target semantic-contract-audit
make semantic-check
make aliasing-check
```

A direct test reference is surface evidence. It does not imply exhaustive
branch, rounding, aliasing, allocation-failure, or domain-limit coverage. See
`PUBLIC_SEMANTIC_CONTRACT_0.18.0.md`,
`SEMANTIC_CONTRACT_VALIDATION_0.18.0.md`, and
`ALIASING_CONTRACT_VALIDATION_0.18.0.md`.

### Hardening

`hardening` contains deterministic resource-failure and lifecycle tests.
`allocation_failure` sweeps the context-owned allocation sites reached by the
covered arithmetic set. `io_allocation_failure` governs text and interchange
commit boundaries. `constant_allocation_failure` governs all public certified-
constant routes. `transcendental_special_allocation_failure` governs 79
transcendental and special endpoints. `arithmetic_root_allocation_failure`
governs 56 arithmetic, fused/planned-operation, and root endpoints, including
the batch transaction. `thread_safety` validates default-context isolation,
thread-exit cleanup, shared immutable operands and divisor plans, cache purge,
and post-purge reconstruction.

```sh
ctest --test-dir build/validation -L hardening --output-on-failure
```

See `ALLOCATION_FAILURE_CONTRACT.md`,
`ALLOCATION_FAILURE_VALIDATION_0.10.9.md`, and
`THREAD_SAFETY_VALIDATION_0.11.0.md`.

### Threading

`threading` selects the focused POSIX-thread ownership, cleanup, and cache
lifecycle suite. The test is marked serial at the CTest process level because
it creates its own worker population.

```sh
ctest --test-dir build/validation -L threading --output-on-failure
```

### Extended

`extended` contains randomized, exhaustive, or large-precision suites:

- exhaustive small-precision arithmetic
- randomized and large transcendental tests
- randomized and large trigonometric and hyperbolic tests
- randomized and large special-function tests

```sh
ctest --test-dir build/validation -L extended --output-on-failure
```

### Smoke

`smoke` executes benchmark and tool paths at small sizes. It establishes that
the executable validates a result and emits the required evidence schema. It
is not performance evidence.

```sh
ctest --test-dir build/validation -L smoke --output-on-failure
```

## Full configured inventory

```sh
ctest --test-dir build/validation --output-on-failure -j1
```

The 0.18.0 configured inventory adds the 56-endpoint arithmetic, fused-operation, and root allocation matrix to the semantic, ABI, numerical, and hardening inventory. The 79-endpoint transcendental and special-function matrix remains part of the cumulative release suite.

Run the exact configured count from CTest rather than relying on a historical total:

```sh
ctest --test-dir build/validation --show-only
```

The ABI checks can also be run directly:

```sh
cmake --build build/validation --target abi-contract-audit
make abi-check
```

## Sanitizers and fuzzing

The allocation and thread-lifecycle suites are run with AddressSanitizer,
UndefinedBehaviorSanitizer, and leak detection. A dedicated serial build uses
ThreadSanitizer through `PPF_ENABLE_THREAD_SANITIZER=ON` or `make tsan`.
Two libFuzzer targets exercise the public text parser and PPFIF importer. See
`THREAD_SAFETY_VALIDATION_0.11.0.md` and
`PORTABILITY_AND_FUZZING_0.10.9.md`.

## MPFR dependency

Most differential tests require the optional MPFR adapter. The compatibility
header under `compat/` permits local test builds when the shared MPFR library
exists without its development header. Release and target-machine validation
should use the official header because conditional features and declarations
can vary with the installed build.

## Correctness interpretation

A passing differential test demonstrates agreement for the exercised inputs,
precision, rounding mode, range, and special-value cases. It is not a proof over
every representable input.

The project combines:

- exact identities and representation invariants
- differential comparison with MPFR
- signed-zero, NaN, infinity, and range cases
- random and exhaustive small-precision tests
- high-precision and narrow-range cases
- aliasing tests
- deterministic allocation-failure injection
- sanitizer and fuzzing builds

## Remaining safety work

Aliasing closure is complete for contract version 1. Production maturity still requires systematic closure of the remaining allocation-atomicity,
practical-limit, and conditional-build gaps recorded in
the generated matrix, together with sustained fuzzing, broader platform and
high-thread-count validation, and library-unload stress.

## Binary compatibility test

`tools/run_abi_binary_compat.py` builds the frozen 0.11.0 source, compiles a
consumer against that header and shared object, and then runs the unchanged
executable with both 0.11.0 and the current library. This checks the SONAME and
the tested public layouts and calls.
