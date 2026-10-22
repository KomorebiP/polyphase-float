# Documentation and historical records

## Current references

Use these documents for the 0.20.0 release:

| Topic | Reference |
|---|---|
| First build and application use | [README](../README.md), [Building](BUILDING.md) |
| Supported platform, inputs, and resource limits | [Release scope](COMPLETION_SCOPE_0.20.0.md) |
| Public API and aliasing | [Semantic contract](PUBLIC_SEMANTIC_CONTRACT_0.20.0.md), [aliasing contract](ALIASING_CONTRACT_0.20.0.md) |
| Allocation and lifecycle classifications | [Allocation evidence](ALLOCATION_COMPLETION_0.20.0.md) |
| PPFIF import limits | [PPFIF resource contract](FPIF_RESOURCE_CONTRACT.md) |
| Mathematical source review | [Branch review](BRANCH_REVIEW_0.20.0.md) |
| Binary compatibility | [ABI policy](ABI_POLICY.md), [0.20 compatibility record](ABI_COMPATIBILITY_0.20.0.md) |
| Executable validation procedure | [Testing](TESTING.md), [Qualification](QUALIFICATION.md) |
| Release status and future work | [Status](../STATUS.md), [Roadmap](../ROADMAP.md) |
| Interpretation of evidence | [Claims and evidence](CLAIMS_AND_EVIDENCE.md) |

[Release notes](../RELEASE_NOTES.md) record changes between versions.
`PRODUCTION_CHANGESET_0.20.0.json` records the production-source comparison
used for this release's audit.

## Retained development evidence

Earlier reports preserve experiments, rejected approaches, validation
snapshots, and measurements. Their version numbers identify the source they
reviewed. They may contain superseded completion counts or future-work
language; check the current references above before applying a claim to 0.20.0.

| Historical stage | Entry point |
|---|---|
| Deep numerical, lifecycle, and packaging audit | [0.19 deep audit](DEEP_AUDIT_0.19.0.md) |
| Arithmetic, fused-operation, and root allocation contracts | [0.18 contract](ARITHMETIC_FUSED_ROOT_ALLOCATION_CONTRACT_0.18.0.md) |
| Transcendental and special-function allocation contracts | [0.17 contract](TRANSCENDENTAL_SPECIAL_ALLOCATION_CONTRACT_0.17.0.md) |
| Certified-constant allocation and cache contracts | [0.16 contract](CONSTANT_ALLOCATION_CONTRACT_0.16.0.md) |
| Text and interchange allocation contracts | [0.15 contract](IO_ALLOCATION_CONTRACT_0.15.0.md) |
| Systematic aliasing validation | [0.14 validation](ALIASING_CONTRACT_VALIDATION_0.14.0.md) |
| ABI epoch 1 establishment | [0.12 ABI validation](ABI_STABILITY_VALIDATION_0.12.0.md) |
| POSIX-thread ownership and cleanup | [0.11 validation](THREAD_SAFETY_VALIDATION_0.11.0.md) |
| Earlier allocation and fuzz validation | [0.10.9 validation](ALLOCATION_FAILURE_VALIDATION_0.10.9.md) |

The corresponding versioned validation reports and production-change manifests
remain beside these documents. This preserves traceability without making a
historical report the current user entry point.

The `history/` directory holds earlier entry points and reports, including
`README_0.10.0_runtime_complete.md`, `RELEASE_NOTES_legacy_0.10.0.md`,
`REPOSITORY_BUILD_STATUS_0.18.md`, and the earlier hardening report.
`history/SHA256SUMS_0.10.0_runtime_complete.txt` is a checksum manifest for its
original handoff; it is not expected to validate the current source tree.

The full source archive also retains development launchers, benchmark data,
root-level validation reports, and the historical PPFIF patch under
`evidence/history/`. These source-archive records are not all installed with
the library. The PPFIF repair is already integrated into the current source;
the retained patch is provenance, not an additional installation step.
