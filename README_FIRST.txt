POLYPHASE FLOAT 0.20.0

Start with README.md, docs/COMPLETION_SCOPE_0.20.0.md and docs/QUALIFICATION.md.
Original code and project material retain LGPL-3.0-or-later licensing.

Build with official GMP development files. Add official MPFR development files
for differential tests and the optional adapter. The production core does not
link MPFR. GCC/Clang, CMake and POSIX threads are required on the tested Linux
ELF LP64/GMP64 baseline.

One-command offline qualification:
  python3 tools/qualify_release.py --output ../qualification --with-sanitizers

Keep qualification output outside this source tree. Inspect the receipt for
actual passes, failures and omitted checks. External replication and independent
mathematical review remain 1.0 gates. This is a pre-1.0 candidate.
