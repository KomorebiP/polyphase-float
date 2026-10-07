# Public release hardening report

## Scope

This report records the release-engineering block completed for Polyphase
Float 0.10.0 on 2026-08-12. The block prepares the repository for public,
open-source development. It does not claim to finish production hardening,
freeze the ABI, or establish new numerical-performance results.

The retained production implementation under `src/` was not algorithmically
modified in this block. Its source changes are SPDX license headers and end-of-line whitespace
cleanup only. The obsolete `src/ppf_internal.h.tmp` file was removed. The public version string
was normalized from the earlier suffixed development identifier to `0.10.0`.

## Completed work

### Licensing and public project policy

- selected `LGPL-3.0-or-later` for the library source;
- added the complete LGPLv3 and GPLv3 license texts;
- added per-file SPDX identifiers to source-like files;
- added authorship, third-party notice, contribution, security, governance,
  support, conduct, and citation files;
- separated current project claims from historical development reports.

### Build and package system

- replaced the private-handoff CMake file with an installable package build;
- added shared-only, static-only, and combined shared/static configurations;
- added versioned shared-library sonames;
- made native CPU tuning opt-in rather than a redistributable default;
- made OpenMP an explicit configuration requirement instead of allowing a
  partial silent build;
- added `PPF_MPFR_SUPPORT=AUTO`, `ON`, and `OFF` modes;
- kept MPFR outside the production core and made its adapter conditional;
- added exported CMake targets `PolyphaseFloat::shared` and
  `PolyphaseFloat::static`;
- added conditional installed adapter targets;
- added relocatable core and adapter `pkg-config` metadata;
- corrected dependency visibility: GMP is public because `gmp.h` and GMP types
  occur in the core public header, and MPFR is public for the adapter header;
- installed the adapter header only when an installable adapter is built;
- retained the Makefile as a research compatibility harness while making
  CMake the canonical installation path.

### Validation and automation

- classified tests as `fast`, `extended`, and benchmark `smoke` suites;
- added CMake presets;
- added a downstream install-tree consumer for the core;
- added a downstream install-tree consumer for the optional MPFR adapter;
- added source, version, SPDX, API, export, hygiene, and independence audits;
- added deterministic `.tar.gz` and `.zip` source packaging;
- added GitHub Actions definitions for GCC, Clang, independent-core,
  extended-validation, and CodeQL jobs;
- added Dependabot and issue/pull-request templates.

## Defects found and corrected during validation

1. The first downstream core consumer omitted the required context argument to
   `ppf_sgn`. The consumer was corrected and now builds only against installed
   headers and targets.
2. CMake benchmark executables containing outer OpenMP regions were not linked
   to `OpenMP::OpenMP_C`. Consequently `_OPENMP` was absent and the worker
   parameter could be ignored. The benchmark targets now receive OpenMP
   compilation and linkage explicitly.
3. The Makefile-generated `.pc` file preserved literal `$(basename ...)` text,
   producing invalid downstream paths despite passing a superficial metadata
   parse. Prefix-relative Make variables now generate valid paths.
4. The initial `.pc` dependency classification treated GMP and MPFR as private
   even though their types occur in public headers. Both are now public
   dependencies in the relevant metadata.
5. The old root checksum manifest described the pre-hardening tree and failed
   against the new source. It is retained only as a labeled historical record;
   current release checksums are generated for the release archives.
6. Temporary `.tmp` files were present under production and compatibility
   directories. They were removed from the public source tree.

## Local validation environment

```text
Date:             2026-08-12
Kernel:           Linux 6.18.35 x86_64
CPU:              AMD EPYC 9V74
GCC:              14.2.0
Clang present:    17.0.0
CMake:            3.31.6
Ninja:            1.12.1
GMP:              6.3.0
MPFR runtime:     4.2.2
OpenMP with GCC:  4.5 (_OPENMP=201511)
```

The local image contained the MPFR runtime library but not the official MPFR
header or `mpfr.pc`. Differential tests therefore used the repository's
minimal MPFR declaration file against the installed MPFR 4.2.2 runtime. The
adapter-install export path was additionally tested with that declaration file
provided temporarily as `mpfr.h` plus local test-only metadata. This validates
the package mechanics in the constrained image; it is not a substitute for the
CI job using the official MPFR development package.

The local Clang compiler could not be configured because this image does not
contain a Clang-compatible OpenMP header/runtime. The public CI definition
installs `libomp-dev` for the Clang job. A local Clang pass is therefore not
claimed by this report.

## Results

### Builds

- GCC portable independent shared and static build with project warnings as
  errors: **PASS**;
- GCC MPFR differential/benchmark build with project warnings as errors:
  **PASS**;
- GCC shared-only build and installation: **PASS**;
- GCC static-only build and installation: **PASS**;
- legacy Makefile `all`, independence audit, and API coverage targets:
  **PASS**.

### Tests

| Suite | Result |
|---|---:|
| Fast semantic and differential tests | 29 / 29 passed |
| Extended randomized, large, and exhaustive tests | 7 / 7 passed |
| Benchmark executable smoke tests | 11 / 11 passed |
| Combined CTest total | 47 / 47 passed |
| Core installed consumer | passed |
| Static-only installed consumer | passed |
| Shared-only installed consumer | passed |
| Adapter-enabled installed consumers | 2 / 2 passed |

### Audits and package checks

```text
Lowercase compatibility inventory: 349 / 349
Public ppf_* declarations:           423
Production shared-library exports:   423
Production mpfr_* imports:           0
```

The following package checks passed:

- source-tree release audit;
- production-library independence audit;
- header/export equality;
- CMake package discovery from a staged install;
- `pkg-config` compilation from a staged install;
- shared and static transitive dependency closure;
- package discovery after relocating the installed prefix;
- Makefile staged installation and corrected metadata generation.

## Deliberately unresolved

This block does not provide evidence for:

- a stable ABI or version 1.0 compatibility guarantee;
- Windows, macOS, ARM64, big-endian, or 32-bit support;
- a local Clang build in the absence of `libomp`;
- continuous fuzzing or a mature regression corpus;
- exhaustive allocation-failure injection;
- ThreadSanitizer coverage;
- a fresh sanitizer campaign for this exact hardened archive;
- external correctness or benchmark replication;
- formal proof-to-code traceability for every certified function;
- universal performance superiority over MPFR;
- production maturity.

Those items remain on the roadmap and must not be inferred from the successful
release-engineering checks above.
