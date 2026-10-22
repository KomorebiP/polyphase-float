# Polyphase Float: finished manual and release-candidate status

The **317-page User Guide and Reference** is finished in PDF and offline HTML.
It covers all 437 core functions and all five MPFR adapter functions, includes
14 complete programs with checked output, and provides a learning path,
numerical guidance, troubleshooting, type reference, symbol index and glossary.
The source archive includes the manual sources and build tools.

The library remains **0.20.0, an unreleased candidate**. Stable 1.0 promotion
has not been applied. The private [draft pull request](https://github.com/KomorebiP/polyphase-float/pull/2)
contains the finished manual, two compact-exponent Gamma/zeta admission fixes,
corrected return and rounding descriptions, package compatibility safeguards,
and explicit stability, numerical-evidence and fuzzing policies.

## Exact candidate

- Source commit: [`0b551118e9664b4aff174b74ebe1579357995fa9`](https://github.com/KomorebiP/polyphase-float/tree/0b551118e9664b4aff174b74ebe1579357995fa9).
- Git tree: `dbc93e7d9f1c30f98585a372525dfe9422ad2d67`.
- Source snapshot: 736 files; SHA-256 `77f607231ff6deccfc6f974657f579c71889d6a1e42bef863576c07099919ed1`.
- Source ZIP: SHA-256 `f5cfe8795df5f8b99b51928e512b0179396afabe7dbb79339d805726bdb84145`.
- Source TAR.GZ: SHA-256 `94119de405af959b8651904060970d9ec928c1571527c70e9ab78b3259dbe133`.

The source archives contain exactly the frozen paths and bytes. Every member's
normalized permissions were checked, and two package builds produced identical
archive bytes in the recorded environment. All 736 GitHub source files, modes,
byte sizes and Git blob hashes were read back and verified. The final manual's
PDF and HTML bytes match those in the archives.

## Validation

The aggregate is `PASS_WITH_RECORDED_EXECUTION_RECOVERY`. The original GCC
runner receipt remains `FAIL`; its separately recorded recovery completes the
missing executed test. See `qualification/FINAL_QUALIFICATION_SUMMARY.json`.

| Scope | Recorded result |
|---|---|
| GCC serial CTest | Initial run: 87 of 88 tests passed; one valid generated ELF had mode 0644 and could not launch. After a mode-only correction to 0755, with identical executable bytes, its separate retry passed 1/1. The original suite failure remains preserved. |
| Clang serial and GCC OpenMP | 88/88 tests passed in each configuration. |
| ASan/UBSan hardening | 30/30 tests passed. LeakSanitizer was disabled. The ctypes-based export check is excluded from this scope and remains in the ordinary compiler suites. |
| Seeded fuzz campaigns | Text and PPFIF targets each exited 0 after 601 seconds: 4,001,432 and 55,246,784 executions respectively. Both logs retain final footers; no diagnostic failure artifacts were found. |
| Separate MPFR-free core | 47/47 tests passed; shared and static installed consumers each passed 1/1. |
| Additional bounded checks | Benchmark smoke 13/13; focused ThreadSanitizer 1/1. These are smoke and targeted results within their recorded scope. |
| Finite numerical oracle | 322,027 main-matrix cases, zero mismatches in the recorded range; this remains finite evidence. |
| Make | Core/adapter build, full test recipe, package-version policy, semantic/aliasing/ABI checks and installation passed. All 255 installed documentation, example, ABI and semantic files match the frozen source bytes. |
| Manual programs and reference | All 14 complete programs passed with GCC and Clang, with exact expected output; declarations, capability-guarded types, teaching fragments, shell syntax, links and installed application examples were checked. |
| Manual layout | All 317 pages were rendered. Visual review, exact page comparisons and fresh inspection of changed pages passed; automated bounds and character scans found no issues. |

The fuzz corpus archive preserves immutable inputs, the interrupted earlier
attempt and the final campaign separately, with hashes and dispositions. Its
SHA-256 is `d3565b5beadacf58130bed5904c8e5f501a2ec813aa1a2d9c18f0763eaae3c96`.
The earlier source snapshot, failed checks, interrupted runs and corrected
coherence findings retain their original identity and status. See the
qualification archive for the authoritative aggregate, original receipts,
commands, raw logs and recovery records.

These are bounded local results on Linux x86-64 ELF/LP64/GMP64. They do not
establish universal numerical correctness or outside-environment reproduction.

## Hosted checks

The final source commit has 19 failed GitHub checks across five workflow runs.
The recorded jobs have empty runner names and no executed steps. The checks did
not execute their test suites. The precise hosting failure cause for this
commit remains unverified; a prior candidate's billing diagnosis is retained
as historical evidence. No hosted success is claimed.

## Requirements remaining for 1.0

The project's five promotion requirements remain open:

1. A retained history of sustained fuzz execution and triage.
2. The required boundary and cap-exhaustion evidence for every fixed numerical limit.
3. Complete written arguments for every certified numerical path and its dependencies.
4. Independent review of rounding, constant arguments and intended claims.
5. Two outside-environment correctness reproductions of this exact candidate and its declared matrix.

The source's `docs/ONE_POINT_ZERO_READINESS.md` identifies the completed work
and remaining obligations. No particular human identity or institution is
required for review or reproduction. Multiple local compilers and agents
remain local evidence. Performance replication is conditional on externally
replicated-performance claims.

The pull request remains draft. No merge, tag, public release or visibility
change was performed.

## Outside-environment correctness reproduction

The exact candidate is **Polyphase Float 0.20.0, unreleased**, source commit
`0b551118e9664b4aff174b74ebe1579357995fa9`, tree
`dbc93e7d9f1c30f98585a372525dfe9422ad2d67`. It has not been promoted to 1.0.

| Identifier | SHA-256 |
|---|---|
| Source ZIP | `f5cfe8795df5f8b99b51928e512b0179396afabe7dbb79339d805726bdb84145` |
| Source TAR.GZ | `94119de405af959b8651904060970d9ec928c1571527c70e9ab78b3259dbe133` |
| Sorted source snapshot JSON, 736 files | `77f607231ff6deccfc6f974657f579c71889d6a1e42bef863576c07099919ed1` |

Use the delivered `SOURCE_SNAPSHOT.json` and archives. Verify `SHA256SUMS` in
the evidence directory, then extract the chosen source archive. Keep all
qualification output outside the extracted source directory. Do not edit
source or documentation before execution; a changed snapshot is a different
candidate.

The declared platform is Linux x86-64 ELF, LP64, with 64-bit GMP limbs.
The reference environment used GCC 13.3.0, Clang 18.1.3, CMake/CTest 3.28.3,
Python 3.12.14, GMP 6.3.0, and MPFR 4.2.1, with official development headers.
Install a working `pkg-config` and the compiler sanitizer runtimes. The GMP
core needs POSIX threads; MPFR supports the adapter and numerical test oracles;
GCC OpenMP supplies the selected parallel configuration. Record actual
versions and omissions when using a different conforming environment.

From the extracted `polyphase-float-0.20.0` directory:

```sh
python3 tools/release_audit.py --root .
python3 tools/qualify_release.py \
  --source . --output ../outside-qualification \
  --only gcc-serial clang-serial gcc-openmp asan fuzz \
  --fuzz-seconds 600 --fuzz-case-timeout 30 --fuzz-max-len 8192 \
  --text-seed 2026102201 --fpif-seed 2026102202 \
  --detect-leaks 0 --openmp-threads 4 --jobs 2
python3 tools/check_manual_examples.py \
  --source . --output ../outside-manual-examples --cc gcc clang --jobs 2
```

The qualifier writes a dated result directory with `qualification.json`,
`source_snapshot.json`, raw `logs/`, and build/installation records. Return
those receipts and raw logs, the complete generated fuzz corpus and any
failure artifacts, the command lines and exit statuses, and the actual host,
compiler, dependency, sanitizer and runtime settings. Hash retained corpora
and artifacts. Do not report a final fuzz execution count unless the log
contains one. Keep failed, interrupted, omitted and not-started checks visible.
LeakSanitizer is explicitly disabled above; it is not qualified by that command.
Additional package, ABI, benchmark and ThreadSanitizer records in the evidence
archive state their own tested scope and exclusions.

The promotion policy requires **two independent correctness reproductions in
outside environments** of this exact candidate and declared matrix. No
particular human identity or institution is required. Several compilers,
processes, agents or containers sharing the present environment remain local
evidence. A completed outside run must identify its independent environment
and exact source and retain contrary results; a job that never started does
not count.

These reproductions close only their named promotion requirement. Sustained
fuzz execution, complete required numerical-limit boundary evidence, complete
written arguments for every certified numerical path, and independent review
of rounding, constant arguments and intended claims remain separate
requirements. See `docs/ONE_POINT_ZERO_READINESS.md`,
`docs/NUMERICAL_LIMITS.md`, `docs/NUMERICAL_ARGUMENTS.md` and
`docs/CLAIMS_AND_EVIDENCE.md` in the source archive.

This evidence directory records source commit `0b551118e9664b4aff174b74ebe1579357995fa9` (tree `dbc93e7d9f1c30f98585a372525dfe9422ad2d67`). The source archives contain the finished reader manual, offline HTML, all example programs, and the qualified 0.20.0 candidate. It is not a stable 1.0 release. Earlier evidence remains at its original path and commit.

The qualification archive includes explicitly selected receipts, raw logs, initial failed attempts, and reproducible probe sources. A matched 313-page earlier manual is retained only as a labeled visual-QA input; the final manual in the source archives has 317 pages. It excludes build binaries, dependency copies, private service receipts, and signed URLs. Same-environment automated checks do not provide the two required outside-environment correctness reproductions.
