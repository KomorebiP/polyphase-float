# Release process

This procedure prepares a source release and its evidence. The supported
contract is defined by [Release scope](COMPLETION_SCOPE_0.20.0.md). The current
beta version is 0.20.0 with ABI epoch 1 and `libpolyphase_float.so.0`.

## 1. Prepare and freeze the source

Choose the release version and complete related source, documentation, and
metadata updates before qualification. Check `CMakeLists.txt`,
`include/polyphase_float.h`, `CITATION.cff`, release notes, and status files for
agreement. Retain required ABI baselines, regression corpora, and historical
evidence. Exclude local builds, qualification output, and temporary files.

For any future major version, decide the ABI epoch, SONAME, package-version
compatibility, and support policy explicitly. A project version change alone
must not silently invalidate the existing binary-consumer commitment.

Run the source and license audit from the source directory:

```sh
python3 tools/release_audit.py
```

Review `LICENSE.md`, `THIRD_PARTY_NOTICES.md`, and contribution provenance.

## 2. Qualify the exact source

Install the prerequisites in [Qualification](QUALIFICATION.md), then run:

```sh
python3 tools/qualify_release.py --output ../polyphase-qualification --with-sanitizers
```

This runs strict GCC serial, Clang serial, and GCC/OpenMP configurations,
complete configured test suites, installed consumers, ASan/UBSan hardening,
and bounded seeded fuzz campaigns. The qualification reference states the
exact exclusions, default duration, and LeakSanitizer policy.

Inspect `qualification.json`, `source_snapshot.json`, and the retained logs.
Every required scope must have a successful result before describing that
matrix as passed. Record failed, unavailable, or unselected checks explicitly.
`NOT_RUN` is not a pass. Run and record any additional ThreadSanitizer,
LeakSanitizer, or longer fuzz campaign separately.

Keep the source unchanged throughout qualification. Any subsequent source or
documentation change creates a new snapshot and needs evidence tied to that
snapshot. Review reusable evidence explicitly rather than relabeling an older
receipt.

## 3. Verify the independent core

Use a separate MPFR-free build and run the shared-library independence audit:

```sh
cmake -S . -B build/release-core \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF \
  -DPPF_BUILD_EXAMPLES=ON
cmake --build build/release-core -j
./build/release-core/example_basic
python3 tools/release_audit.py \
  --library build/release-core/libpolyphase_float.so
```

The qualification matrix already exercises installed consumers. For a manual
check of this core-only installation, follow [Building](BUILDING.md#verify-the-installation).

## 4. Package and verify

Create deterministic archives from the frozen source:

```sh
SOURCE_DATE_EPOCH=1792627200 \
  python3 tools/create_source_release.py --output-dir ../polyphase-release-dist
```

The command above uses the release's fixed packaging epoch (2026-10-22 UTC).
A release may select another epoch, but record and reuse the same value for every rebuild.
The packager produces `.tar.gz`, `.zip`, and `SHA256SUMS`, excluding local
build products and raw local results. Inspect the archive contents and verify
that extraction, build, installation, and the basic example work from the
packaged source. Check that both archive formats contain the intended files.

Keep the exact source archives, checksums, qualification receipts, and
source snapshots together in the release record. A source archive and a
qualification receipt answer different questions: the checksum identifies
what was distributed. The receipt identifies what was executed.

## 5. Review the release claim

State the supported platform and numerical contract, completed validation,
known limits, and any omitted checks. Separate operation-specific benchmark
results from correctness and compatibility evidence.

Version 0.20.0 can be distributed under its documented pre-1.0 contract.
A later 1.0 promotion also needs the independent review and external
reproduction evidence listed in the [roadmap](../ROADMAP.md). Supply reviewers
with the exact source snapshot, dependency versions, commands, and raw data.
Retain contrary results as well as successful reproductions.

## 6. Publish the reviewed artifacts

Publish source and checksums together, with the applicable evidence and
release notes. Sign tags and artifacts where supported. Never replace a tagged
release with different bytes. Issue a new version for a correction.

See [GitHub release procedure](GITHUB_RELEASE.md) for repository settings,
tagging, and release assets. Local packaging does not itself publish a tag,
release, or hosted workflow result.

[Documentation](README.md) · [Qualification](QUALIFICATION.md) · [GitHub publication](GITHUB_RELEASE.md) · [Project overview](../README.md)
