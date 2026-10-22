# Repository build status

The private repository baseline was checked at commit
`0ec253fd7e0547aac13def718ea19b92787bf36d`. All 47 production source files
match the frozen 0.18.0 source manifest. The current candidate therefore uses
the newer audited 0.19.0 source plus the integrated 0.20.0 repairs.

The remote temporary-build and private CodeQL SARIF changes are preserved.
Its export-audit fixtures are retained and adapted to the centralized
capability helper. The original staging status is preserved unchanged under
docs/history/REPOSITORY_BUILD_STATUS_0.18.md.

Run docs/QUALIFICATION.md for current execution evidence. This document does
not assert a new remote commit, hosted workflow result, tag or public release.
