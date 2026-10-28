<!-- SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors -->
<!-- SPDX-License-Identifier: LGPL-3.0-or-later -->

# GitHub publication and release procedure

This document covers the public repository at
[KomorebiP/polyphase-float](https://github.com/KomorebiP/polyphase-float).
Polyphase Float remains **beta and work in progress**. Follow the
[release process](RELEASE_PROCESS.md) to prepare and qualify an exact source
revision before publishing its artifacts.

<a id="initial-publication"></a>

## Prepare the checkout

The repository is already public. Start from a checkout of its existing
`main` branch and record `git rev-parse HEAD`. Keep local builds, staged
installations, generated archives and runner logs outside the tracked source.

The following commands provide a local build check. The complete release
matrix is described in [Qualification](QUALIFICATION.md). From the source root:

```sh
python3 tools/release_audit.py
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_EXAMPLES=ON \
  -DPPF_BUILD_BENCHMARKS=OFF \
  -DPPF_WARNINGS_AS_ERRORS=ON
cmake --build build/release --parallel
ctest --test-dir build/release -L 'fast|hardening|semantic' --output-on-failure
python3 tools/release_audit.py --root . --library build/release/libpolyphase_float.so
```

## Repository settings

Review these settings before a release. Enable GitHub Actions, the dependency
graph, Dependabot alerts, CodeQL code scanning, and private vulnerability
reporting. Protect `main` against force pushes and deletion. Use the check
names from a completed workflow run to require the principal CI and
ABI-governance checks before merging.
These are maintainer instructions, not a record that each setting is enabled.

The repository includes `.github/CODEOWNERS`, issue forms, a private-security
contact link, pull-request guidance, CI, ABI governance, CodeQL, extended
validation, and Dependabot configuration.

## Release assets

Generate deterministic project archives from the audited tag checkout:

```sh
SOURCE_DATE_EPOCH=1792627200 \
  python3 tools/create_source_release.py --output-dir ../polyphase-release-dist
(cd ../polyphase-release-dist && sha256sum -c SHA256SUMS)
```

This uses the same fixed epoch, 2026-10-22 UTC, as the packager and
[release process](RELEASE_PROCESS.md#4-package-and-verify). Choose a fresh output
directory for a new candidate. Run the checksum check inside that directory
because `SHA256SUMS` contains archive filenames relative to it.

For a reviewed 0.20.0 release, attach these files to a GitHub prerelease:

- `polyphase-float-0.20.0.zip`
- `polyphase-float-0.20.0.tar.gz`
- `SHA256SUMS`
- the separate reproducibility bundle, when publishing retained validation logs

GitHub's automatically generated tag archives are supplementary. They are not
substitutes for the project's deterministic source archives.

## Tagging

Create an annotated or signed tag only after the protected-branch workflows
pass for the exact commit being tagged. The commands below illustrate a
versioned release. Use a new tag name if that name already exists and preserve
all previously published tags. A successful workflow on a later commit does
not qualify an earlier tag.

```sh
git tag -a v0.20.0 -m "Polyphase Float 0.20.0"
git push origin v0.20.0
```

Create the corresponding GitHub Release and mark it as a prerelease. State the
supported ABI boundary exactly as Linux ELF LP64/GMP64, ABI epoch 1. State that
MPFR and OpenMP are optional for the production core, while GMP and the current
POSIX-thread baseline are required.

## Post-release verification

Clone the tagged release into a new directory. Configure a serial core build,
run the fast and ABI checks, install into an empty prefix, and compile an
external consumer with both the installed CMake package and `pkg-config`
metadata. Record platform-specific limits in `STATUS.md` and `ROADMAP.md`
rather than broadening the support claim without evidence.

[Documentation](README.md) · [Release process](RELEASE_PROCESS.md) · [Qualification](QUALIFICATION.md) · [Project overview](../README.md)
