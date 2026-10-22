# Fuzzing and permanent regression policy

## Scope and campaign records

The maintained fuzz targets exercise text parsing and PPFIF import. They check
memory safety and the validity of resulting objects. They are not numerical
oracles for every accepted value and do not replace exact or MPFR-based tests.

| Harness | Input bound | Additional scope |
| --- | --- | --- |
| `fuzz_text_parser` | 4,096 bytes | Precisions 2 through 256; six rounding modes; automatic base or bases 2 through 62 selected by the input. |
| `fuzz_fpif_import` | 8,192 bytes | Linux `fmemopen`, an initially zero destination with 128-bit precision. |

The qualifier's `--fuzz-max-len` is an additional generator bound. It cannot
override these harness limits; text inputs larger than 4,096 bytes are ignored
by the text harness. Resource limits outside the harness envelope need ordinary
deterministic regression tests.

Every retained campaign must identify the exact source snapshot, harness
source hashes, compiler and dependency versions, sanitizer settings, distinct
positive seeds, duration, per-input timeout, maximum input length, initial
corpus hashes, command lines, exit status, and raw logs. Record a final
execution count only when it is present in the retained log. A partial log
supports a lower bound, not an invented total. Failed, interrupted, and
not-started runs remain visible beside any later successful retry.

Use `tools/qualify_release.py --only fuzz` with its output directory outside
the source tree. The runner preserves the source snapshot and logs, keeps
generated corpus data separate from the immutable input seeds, and records
whether the source changed during execution. Hash any retained generated
corpus or failure artifact when collecting the campaign evidence.

## Routine and release campaigns

The dedicated GitHub workflow provides a weekly scheduled campaign and manual
dispatch, with 3,600 seconds per target, two distinct recorded seeds, a
30-second per-input timeout, and an 8,192-byte generator limit. The two targets
run concurrently. Its workflow artifact retains receipts, logs, generated
corpora, and failure artifacts for 90 days. Maintainers must preserve release
evidence and admitted regressions beyond that temporary retention period.
Scheduled execution uses the default branch; a workflow present only on a
delivery branch is not evidence that the schedule has run.

For a frozen release candidate, run a separately identified campaign of at
least 600 seconds per target together with the ordinary compiler, sanitizer,
and deterministic regression checks. Preserve the exact candidate snapshot
and results. A release campaign is bounded evidence for that snapshot; it is
not a substitute for sustained execution over time.

A workflow definition establishes the procedure. It does not establish a run
history. A scheduler, billing, dependency, or runner-start failure is recorded
as not executed. Promotion claims about sustained fuzzing require an actual
retained history of completed campaigns and triage; merely adding this policy
or running one campaign does not close that requirement.

When a runner starts, the workflow also records the source commit, run and
attempt identifiers, and step outcomes. A skipped campaign is `NOT_RUN`; a
campaign step that ran without leaving a usable qualification receipt is
`UNVERIFIED`. A billing or scheduler failure before any job starts must be
documented from the hosting service's run status, because no in-job evidence
step can execute in that case.

## Corpus admission and regression retention

The permanent input corpus is `fuzz/corpus/text` and `fuzz/corpus/fpif`.
Historical audit corpora remain historical evidence; they are not automatically
included by the qualification runner. Admit a new permanent entry when it
reproduces a defect or exercises a documented boundary not already represented
by an existing entry. Keep this active corpus small and purposeful.

For each defect or new boundary:

1. Preserve the original failing bytes, their SHA-256, failing source revision,
   seed, complete command, sanitizer output, and environment before attempting
   minimization. Do not overwrite the original failure with a successful retry.
2. Minimize a copy with the same harness and options. Confirm that the minimized
   input still triggers the same defect on the affected revision. Record its
   SHA-256 and the relationship to the original artifact. A changed symptom is
   a separate finding.
3. Add the minimal reproducer to the appropriate permanent corpus and record
   its purpose, source issue or review record, and hash in the change review.
   Use a descriptive filename. Do not include credentials, private input data,
   or unrelated generated corpus files.
4. Add an ordinary deterministic regression test when the defect has a stable
   assertion, particularly for return values, flags, rollback, or numerical
   results that the fuzz harness does not check. Show the affected revision
   failing and the fix passing where practical.
5. Replay the admitted corpus on the fixed implementation, then run the
   relevant sanitizer and bounded campaign. Preserve both the original failure
   and the verified repair evidence.

A failure reproducer must not be deleted merely because it stops increasing
coverage. Deduplication requires a recorded reason and preservation of an
equivalent regression. If a reproducer no longer fits a harness limit, retain
the bytes and move the assertion to an appropriate deterministic test rather
than silently dropping the case. Bulk corpus minimization must operate on
copies and must not remove the only permanent reproducer of a known defect.

## Triage

Treat a sanitizer diagnostic, invalid object, crash, or incorrect asserted
result as a defect until resolved. Reproduce timeouts with the original input
and recorded timeout; distinguish an expected bounded rejection, an expensive
but permitted computation, infrastructure contention, and unintended unbounded
work. Increasing a timeout alone does not establish a fix. Likewise, an
allocation failure or documented numerical rejection is not automatically a
bug, but the promised destination, flag, and cleanup behavior must still hold.

Every unresolved finding needs an identified affected path, reproducer, impact,
and disposition. Security-relevant cases follow [Security policy](../SECURITY.md)
before public disclosure. The release record must say which findings remain
open and which campaign scopes actually completed.
