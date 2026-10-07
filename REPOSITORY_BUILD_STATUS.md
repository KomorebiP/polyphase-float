<!-- SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors -->
<!-- SPDX-License-Identifier: LGPL-3.0-or-later -->

# Repository build status

This is a private local staging candidate prepared on 2026-10-07 from
`polyphase-float-0.18.0_GITHUB_READY_CANONICAL.zip`. A GitHub account, owner,
repository, remote, branch, push, tag, public release, and completed GitHub
Actions run have not been confirmed by this staging work.

All 459 canonical files were extracted without changing their bytes. This
status document is the only added candidate file. The archive SHA-256 is
`73888e965a2a3c8978462133f78403ebb40967870691b136ba5f54953aa2be6f`.
The existing LGPL-3.0-or-later license and third-party notices are preserved.

## Checks run in this session

The local environment is Linux x86_64, GCC 13.3.0, and Python 3.12.14. CMake,
Clang, Ninja, pkg-config, GMP development headers, and the official MPFR
header were unavailable. The GMP and MPFR runtime libraries were present.
No dependencies were downloaded or installed during these checks.

| Command | Result | Exit code | Seconds |
|---|---|---:|---:|
| `cmake -S . -B ../../private/polyphase-build-cmake -DCMAKE_BUILD_TYPE=Release -DPPF_OPENMP=OFF -DPPF_MPFR_SUPPORT=OFF -DPPF_BUILD_EXAMPLES=ON` | BLOCKED: cmake unavailable | 127 | 0.006 |
| `make -j2 PPF_OPENMP=OFF core` | FAIL: missing GMP development header | 2 | 1.221 |
| `python3 tools/release_audit.py` | PASS | 0 | 22.493 |
| `python3 scripts/check_api_coverage.py` | PASS | 0 | 0.065 |
| `python3 tools/semantic_contract.py verify --root . --contract semantics/contract-v1.json` | PASS | 0 | 8.512 |
| `python3 tests/test_semantic_contract.py --root .` | PASS | 0 | 8.966 |
| `python3 tools/aliasing_contract.py verify --root . --contract semantics/aliasing-contract-v1.json` | PASS | 0 | 6.336 |
| `python3 tests/test_aliasing_contract.py --root .` | PASS | 0 | 5.490 |
| `python3 tools/abi_contract.py verify --root . --contract abi/contract-v1.json` | PASS | 0 | 0.053 |
| `python3 tools/abi_contract.py compare --baseline abi/baselines/0.11.0-contract.json --current abi/contract-v1.json` | PASS | 0 | 0.053 |
| `python3 tools/abi_contract.py compare --baseline abi/baselines/0.17.0-contract.json --current abi/contract-v1.json` | PASS | 0 | 0.096 |
| `python3 tests/test_generate_product_profile.py` | PASS | 0 | 0.381 |

Durations are wall-clock durations, and independent source checks ran in
parallel. The CMake output directory is shown as an equivalent relative path. Full command output and machine-readable check records are retained
with the staging evidence outside the candidate source tree.

The source-only release audit passed. It reported 216 SPDX-bearing source-like
files, 437 public function declarations, 437 direct semantic test references,
191 semantic entries with explicit gaps, and zero unverified aliasing entries.
The API coverage script reported 349/349 direct counterparts and explicitly
stated that the library was not built. These results verify the recorded
source contracts and policy, and do not establish compiled numerical behavior.

The canonical CMake configuration was blocked because `cmake` was absent.
The alternative serial Make core build failed while compiling `src/abi.c`
because `gmp.h` was absent. No compiled library or executable tests ran.
The temporary Make build tree was removed after preserving its failure log.

## Limits and publication items

- Compiled differential, exact, allocation-failure, threading, sanitizer,
  fuzzing, benchmark, install-consumer, binary-compatibility, and export checks
  were not run in this session. Historical evidence remains in the canonical
  documents, with its original scope and date.
- The current README and release procedure assume
  `KomorebiP/polyphase-float`. README badges, citation URLs, the issue-template
  security link, and `CODEOWNERS` entry `@KomorebiP` need confirmation against
  the actual GitHub destination. The source audit does not check whether these
  remote URLs exist.
- The coordinating build checked the official GitHub API reference
  `actions/checkout@v7.0.1`, which resolved to
  `3d3c42e5aac5ba805825da76410c181273ba90b1`. The original workflows are
  preserved. `github/codeql-action@v4` still needs reference validation before
  using CodeQL as a publication gate; no hosted workflow execution has been
  recorded by this worker.
- Attribution is already under the pseudonym Komorebi P. A bounded text scan
  found no Victor name, email address, or user-home path in the canonical
  text files. This scan does not certify every possible identity disclosure.
- `CITATION.cff` records the historical release date 2026-08-31. Confirm that
  date and the archive/tag metadata before publishing a new GitHub release.
- The final README paragraph still mentions "an ABI decision" while the
  earlier status table and ABI policy declare epoch 1 for Linux ELF LP64/GMP64.
  Reconcile that documentation language during release review.
- The existing notices identify external GMP, optional MPFR, OpenMP, POSIX
  threads, and platform math libraries. They state that GMP and MPFR
  implementation sources are not vendored. `compat/mpfr_minimal.h` is a
  local-test fallback; public builds should use official development headers.

Production source and mathematical implementation were not modified. This
file records the current staging check results, and does not expand platform,
performance, correctness, or production-maturity claims.
