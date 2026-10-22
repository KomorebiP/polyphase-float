# Readiness for a stable 1.0 release

The reader's manual and package compatibility work are complete preparation
for a stable release. The current library remains an unreleased 0.20.0
candidate. A stable 1.0 designation has not been applied. This decision follows
the project's stated promotion requirements, rather than a universal rule
that software must have a formal proof before using a major version number.

## Decisions and engineering work completed

| Item | Result |
|---|---|
| ABI and SONAME | Retain ABI epoch 1 and both existing `.so.0` names through the compatible 1.x series. Project major version no longer determines the SONAME. |
| CMake package selection | Preserve existing 0.x behavior, require the governed 0.11 baseline, and explicitly support compatible minimum-version requests across a future 0.x-to-1.x transition. Exact, range, and pointer-size checks remain effective. |
| Support policy | Maintain the compatible 1.x contract for that series. Fixes are provided as available, without a calendar service commitment. |
| Reader's manual | A continuous guide, worked C programs, complete core function reference, type reference, symbol index, glossary, and numerical resource policies. |
| Reference accuracy | Distinguish scalar ternaries, packed paired results, ordinary statuses, predicates, and per-item batch ternaries. State operation-specific allocation outcomes. |
| Numerical admission | Reject excessive negative-parameter incomplete-Gamma recurrence before integer expansion. Recognize negative-integer zeta parity without expanding a compact enormous input. |
| Limit inventory | Classify source work budgets separately from performance thresholds and structural loops; record the available boundary evidence and missing obligations. |
| Fuzz governance | Define permanent corpus admission, minimization, reproducibility, triage, and retained evidence. Supply a scheduled and manually triggered long-campaign workflow. |

The stability and support decisions are in [Stability policy](STABILITY_POLICY.md).
The package-selection regression exercises actual CMake consumers, including
a fixture whose project version is 1.0.0. A fixture is a compatibility test,
not a published 1.0 library.

## Requirements still open

| Promotion requirement | Remaining work |
|---|---|
| Sustained fuzz execution | The policy and workflow do not establish a history of completed long campaigns. Retain actual executions and contrary results under the declared harness and resource model. |
| Every fixed numerical limit has its required boundary evidence | The inventory identifies unexecuted cap-exhaustion and boundary obligations. Do not replace those tests with a declaration that the number is conservative. |
| Every certified numerical path has a complete written argument | The argument map records fast-kernel, primitive, reduction, enclosure, and source-operation dependencies that remain open. A correct final rounder cannot certify an invalid enclosure. |
| Independent review of rounding, constant arguments, and intended claims | Existing local AI/source reviews retain their exact scope and conditional dependencies. They do not close the entire review obligation. The policy does not require a particular human identity or institution. |
| At least two independent correctness reproductions of the exact candidate and support matrix | No qualifying outside-environment reproductions are recorded. Multiple compilers, agents, or containers sharing the current environment are local evidence. |

The detailed mathematical map is [Numerical arguments](NUMERICAL_ARGUMENTS.md).
The concrete work budgets and evidence gaps are in
[Numerical limits](NUMERICAL_LIMITS.md). Independent reproduction has the
meaning defined in [Claims and evidence](CLAIMS_AND_EVIDENCE.md).

Performance replication is required for claims of externally replicated
performance. It is not a blanket extra condition for making a bounded
correctness or compatibility claim.

## Reproduction handoff

Supply each independent reproducer with the same frozen source archive,
checksum, compiler and dependency requirements, declared platform matrix,
qualification commands, and raw-result schema. Require the returned record to
identify its environment and exact source, and to retain failed or omitted
checks. A compiler run that never started does not count.

After a material repair, determine which prior evidence still applies and
which computations must be repeated. Keep the earlier records rather than
relabelling them. Once the open requirements are satisfied, promote the
verified candidate through the documented release process with a deliberate
version and metadata update. A renamed archive is not a release-channel
implementation and does not supply missing evidence.
