# Contributing to Polyphase Float

Polyphase Float accepts focused contributions that improve correctness,
clarity, portability, validation, or measured performance without weakening
the numerical contract.

## Before opening a pull request

1. Search existing issues and pull requests for overlapping work.
2. Keep one pull request to one coherent technical change.
3. Explain the affected numerical contract, algorithm, API, or build path.
4. Add or update tests before changing a correctness-sensitive path.
5. Record third-party provenance and licensing when applicable.
6. Do not include generated binaries, local build trees, credentials, or large
   raw benchmark outputs.

## Build and test

A normal development validation is:

```sh
cmake -S . -B build/dev \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_EXAMPLES=ON
cmake --build build/dev -j
ctest --test-dir build/dev -L fast --output-on-failure
python3 tools/release_audit.py --library build/dev/libpolyphase_float.so
```

Run extended tests for changes to transcendental, trigonometric, special,
random, range, or large-precision behavior:

```sh
ctest --test-dir build/dev -L extended --output-on-failure
```

The exact target filename may vary by platform. Omit `--library` when running
the source-only release audit.

## Numerical changes

A numerical change must document:

- the mathematical identity or algorithm used;
- the enclosure, remainder, or rounding argument;
- supported domains and resource limits;
- special-value and signed-zero behavior;
- every applicable rounding mode;
- aliasing behavior;
- comparison against an independent oracle or exact identity;
- performance evidence when speed is part of the claim.

A faster result that weakens correctness is not accepted. A claimed speedup
must use matched precision, rounding, worker resources, and timing boundaries.
Raw measurements must be retained.

## Public API and ABI changes

The project is pre-1.0 and the ABI is not frozen. Nevertheless, public changes
must be intentional. Update the header, documentation, examples, version
metadata, API audit, and release notes in the same pull request. Serialized
format changes require an explicit versioning and migration plan.

## Style

- C11 is the implementation language.
- Use four spaces in C and headers.
- Keep functions focused and error paths explicit.
- Check all allocation and size calculations.
- Avoid silent fallbacks that change numerical semantics.
- Prefer generated tables only when their generator and verification are
  included.
- New files must carry SPDX metadata.

## Commit provenance

By submitting a contribution, you certify that you have the right to submit it
under `LGPL-3.0-or-later`. Preserve authorship and third-party notices. Do not
copy code from incompatible sources.
