<!-- SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors -->
<!-- SPDX-License-Identifier: LGPL-3.0-or-later -->

# GitHub publication and release procedure

This document covers the public repository at
`https://github.com/KomorebiP/polyphase-float`. Polyphase Float remains a
pre-1.0 research release.

## Initial publication

Create an empty repository named `KomorebiP/polyphase-float` with `main` as its
default branch. Do not initialize it with a generated README, license, or
`.gitignore`. Commit the contents of this source tree at the repository root.
Do not commit the enclosing archive directory, local build trees, staged
installations, generated archives, or runner-bundle logs.

Before the first push, run:

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

Enable GitHub Actions, the dependency graph, Dependabot alerts, CodeQL code
scanning, and private vulnerability reporting. Protect `main` against force
pushes and deletion. After the first workflow run establishes the exact check
names, require the principal CI and ABI-governance checks before merging.

The repository includes `.github/CODEOWNERS`, issue forms, a private-security
contact link, pull-request guidance, CI, ABI governance, CodeQL, extended
validation, and Dependabot configuration.

## Release assets

Generate deterministic project archives from the audited tag checkout:

```sh
rm -rf dist
SOURCE_DATE_EPOCH=1788134400 python3 tools/create_source_release.py --output-dir dist
sha256sum -c dist/SHA256SUMS
```

For `v0.20.0`, attach these files to a GitHub pre-release:

- `polyphase-float-0.20.0.zip`
- `polyphase-float-0.20.0.tar.gz`
- `SHA256SUMS`
- the separate reproducibility bundle, when publishing retained validation logs

GitHub's automatically generated tag archives are supplementary. They are not
substitutes for the project's deterministic source archives.

## Tagging

Create an annotated or signed tag only after the protected-branch workflows
pass:

```sh
git tag -a v0.20.0 -m "Polyphase Float 0.20.0"
git push origin v0.20.0
```

Create the corresponding GitHub Release and mark it as a pre-release. State the
supported ABI boundary exactly as Linux ELF LP64/GMP64, ABI epoch 1. State that
MPFR and OpenMP are optional for the production core, while GMP and the current
POSIX-thread baseline are required.

## Post-release verification

Clone the tagged release into a new directory. Configure a serial core build,
run the fast and ABI checks, install into an empty prefix, and compile an
external consumer with both the installed CMake package and `pkg-config`
metadata. Record platform-specific limits in `STATUS.md` and `ROADMAP.md`
rather than broadening the support claim without evidence.
