# Polyphase Float 0.20.0 — Release Report

22 October 2026

Polyphase Float 0.20.0 provides arbitrary-precision binary floating-point
arithmetic in C11. This release supports Linux x86-64 ELF with the LP64 data
model and 64-bit GMP limbs. The core requires GMP and POSIX threads. OpenMP
and the MPFR interoperability adapter are optional; the production core is
independent of MPFR.

ABI epoch 1, public declarations, symbol versions, and
`libpolyphase_float.so.0` are retained. The release remains 0.20.0 under its
documented pre-1.0 contract and LGPL-3.0-or-later license.

## What was finalized

- Bounded PPFIF import storage and exact Airy admission; validation of invalid
  setter rounding modes before destination mutation.
- Safe rejection of invalid text-output radices and impossible zero-output
  lengths; native floating-point conversion without exponent-sized allocation.
- Full-width GMP bit counts in rounding, correct division carry-to-overflow
  handling, and corrected exact subnormalization flags.
- Interval settlement that defers when endpoint range flags disagree; safe
  partial-initialization cleanup and bounded dyadic certificate construction.
- Early settlement of proven exponential and Gamma range cases; direct integer
  base-2 exponentials and bounded exact-power and exponential-minus-one work.
- A checked introductory example, practical build and consumer instructions,
  current API and ownership references, and usable installed documentation.

The semantic reference indexes 437 core functions and records 192 transactional
allocation contracts, with no remaining indexed allocation-evidence gaps.
Published numerical and resource limits still apply. Numerical rejection may
produce NaN with `PPF_TERNARY_ERROR` and `PPF_FLAG_ERANGE`; allocation rollback
has its own documented destination-preservation contract. See
`docs/COMPLETION_SCOPE_0.20.0.md` in the source archive.

## First use

Install a C11 compiler, CMake 3.23 or newer, and official GMP development files.
From the extracted source directory:

```sh
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_EXAMPLES=ON
cmake --build build/release -j
./build/release/example_basic
```

The example prints the exact value 31 at 256-bit and 80-bit precision and
shows checked operations and cleanup. Follow `README.md` to install the
library and build an application using the CMake `core` component.
`docs/BUILDING.md` covers OpenMP, the adapter, static linking, and troubleshooting.

## Final validation

The following results are tied to the frozen source identified below.

| Check | Final result |
|---|---|
| Finite arithmetic/conversion/range/interval oracle matrix | 322,027 cases; zero mismatches |
| Constants | 64 interval, 384 rounding, and 32 exact-rational enclosure checks passed |
| Strict GCC serial, Clang serial, and GCC/OpenMP | 73/73 tests passed in each configuration |
| ASan/UBSan hardening | All 30 distinct tests passed; one permission-only retry |
| Seeded text-parser and PPFIF fuzzing | About 601 seconds each; clean exits; no crash files |
| Source archives | Byte-identical same-environment rebuilds; all 676 files match the frozen source |
| Extracted MPFR-free build and installed shared/static consumers | 33/33 core tests; both consumer checks passed |
| GNU Make build, test, and audit | Passed; 60 test executables |
| ABI, exports, production dependencies, and `pkg-config` | Passed |
| Benchmark smoke checks | 13/13 passed |
| Focused ThreadSanitizer check | 1/1 passed |
| Hosted CI, ABI, and CodeQL workflows | Blocked before execution by account billing/spending-limit restrictions |

The numerical matrix used the final GCC serial static library and includes
explicit precision and input bounds. Its manifest records matching source and
library hashes before and after execution. PPFIF fuzzing completed 56,220,383
executions. The text-parser log records at least 161,552 executions but lacks
its final count footer. Both timed processes exited successfully.

An earlier OpenMP run and one sanitizer test could not execute because generated
files lacked execute permission. The OpenMP build was repeated cleanly; the
sanitizer retry used verified unchanged binary bytes after a mode repair.
The permission change's cause remains unidentified; the original unsuccessful
attempts remain in the evidence.

The canonical ASan/UBSan configuration disables LeakSanitizer and excludes the
Python `ctypes` export check; ordinary compiler configurations run that export
check. The Make run used its supported minimal MPFR compatibility declarations; the
CMake matrix uses official headers. Benchmark smoke checks establish execution,
not performance superiority.

## Source and delivery identity

- Verified source commit: [`ba82abe7cff3b77ba4d2b07833bcfda953e92a2c`](https://github.com/KomorebiP/polyphase-float/commit/ba82abe7cff3b77ba4d2b07833bcfda953e92a2c).
- Final source snapshot SHA-256: `6da1bcdac98a970b7cf8af5a54e379f191f7eab153b4e1d2fd30dcfeec9e37d2`.
- `polyphase-float-0.20.0.tar.gz`: SHA-256 `85629446ffbaeaf7f582bff028563ef4b89e5490001117cecf5150924d3389ed`.
- `polyphase-float-0.20.0.zip`: SHA-256 `0b83ba5d044aa26c27b9731d5625cf73df16bc02cbc72c2d2030d7f54726f982`.
- Evidence: `qualification-evidence.zip`, indexed by `EVIDENCE_MANIFEST.json`; `SOURCE_SNAPSHOT.json` and `SHA256SUMS` identify the accompanying files.
- Source review: [draft pull request #2](https://github.com/KomorebiP/polyphase-float/pull/2) contains the verified source commit above. Hosted checks did not execute; local validation is reported separately.
- No release tag or GitHub Release has been published.

The later 1.0 promotion still requires independent mathematical review,
external correctness reproduction, and the remaining obligations in
`ROADMAP.md`. Local validation is evidence for the recorded source, platforms,
and exercised cases.

## Evidence package

This directory preserves the release-candidate source archives and bounded qualification evidence for source commit `ba82abe7cff3b77ba4d2b07833bcfda953e92a2c` (tree `1f97f349d682ccb4da2e4dab50836f5fc114f0f7`). The qualified source branch and its file set are unchanged by this evidence-only commit.

### Files

- `polyphase-float-0.20.0.zip` and `.tar.gz`: deterministic source archives. Every file was compared with the frozen source snapshot. Archive permissions follow the source packager's shebang normalization.
- `qualification-evidence.zip`: explicitly selected reports, manifests, reproducible probe sources/scripts, and raw logs. Paths retain their original `qualification/`, `review/`, or `release_tools/` prefix. Historical failed or interrupted attempts retain their original status; their presence is not evidence of passing the final candidate.
- `EVIDENCE_MANIFEST.json`: source identity, selected evidence paths and hashes, archive verification, and precise packaging scope.
- `SOURCE_SNAPSHOT.json`: SHA-256 for every qualified source file.
- `SHA256SUMS`: hashes of the delivered archives and metadata.

The evidence excludes build binaries, build directories, dependency installation copies, private service-operation receipts, and signed transfer URLs. Local workspace paths in logs describe the recorded run and are not portable installation instructions. Same-task automated reviews are not outside human reviews or independent organizational reproductions.
