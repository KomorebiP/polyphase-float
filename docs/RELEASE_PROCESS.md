# Release process

## 1. Freeze the candidate

- choose the version;
- stop unrelated algorithm changes;
- update `CMakeLists.txt`, `polyphase_float.h`, `CITATION.cff`, release notes,
  and changelog/status files;
- confirm no generated or temporary files are staged.

## 2. Validate source and license

```sh
python3 tools/release_audit.py
```

Review `LICENSE.md`, `THIRD_PARTY_NOTICES.md`, and contribution provenance.

## 3. Build the independent core

```sh
cmake -S . -B build/release-core \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_EXAMPLES=ON
cmake --build build/release-core -j
```

Run the independence audit against the produced shared library:

```sh
python3 tools/release_audit.py \
  --library build/release-core/libpolyphase_float.so
```

## 4. Build validation configuration

```sh
cmake -S . -B build/release-validation \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DPPF_MPFR_SUPPORT=ON \
  -DPPF_BUILD_EXAMPLES=ON \
  -DPPF_BUILD_BENCHMARKS=ON
cmake --build build/release-validation -j
ctest --test-dir build/release-validation -L fast --output-on-failure
ctest --test-dir build/release-validation -L extended --output-on-failure
ctest --test-dir build/release-validation -L smoke --output-on-failure
```

## 5. Test installation

```sh
cmake --install build/release-core --prefix "$PWD/stage/prefix"
cmake -S tests/install_consumer -B build/install-consumer \
  -DCMAKE_PREFIX_PATH="$PWD/stage/prefix"
cmake --build build/install-consumer -j
ctest --test-dir build/install-consumer --output-on-failure
```

## 6. Create deterministic source archives

```sh
SOURCE_DATE_EPOCH=1788134400 \
python3 tools/create_source_release.py --output-dir dist
```

The packager creates `.tar.gz`, `.zip`, and `SHA256SUMS` outputs from the clean
source tree while excluding local build products and raw local results.

## 7. External review

Provide reviewers with the exact candidate hash, dependency versions, build
commands, raw benchmark procedure, and known limitations. Do not rewrite a
tagged release after review. Create a new candidate for every change.

## 8. Publish

- sign the tag and release artifacts where possible;
- publish source and checksums together;
- archive the exact artifact used for the paper or benchmark report;
- record independent replications without suppressing negative results.

For GitHub-specific publication, repository settings, tagging, and release assets, see `GITHUB_RELEASE.md`.
