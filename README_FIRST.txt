POLYPHASE FLOAT 0.20.0

Start with README.md for the build, first example, and installation commands.
The supported baseline is Linux x86-64 ELF, LP64, and 64-bit GMP limbs, with
a C11 compiler, CMake 3.23 or newer, official GMP development files, and
POSIX threads.
OpenMP is optional. The production core does not require MPFR.

Reading guide:
  docs/BUILDING.md                 Build options and downstream use
  docs/COMPLETION_SCOPE_0.20.0.md   Supported inputs and failure behavior
  docs/TESTING.md                  Local tests and their interpretation
  docs/QUALIFICATION.md            Reproducible release validation
  docs/HISTORY_INDEX.md            Earlier reports and experiments

For full qualification, install official MPFR development files and the tools
listed in docs/QUALIFICATION.md, then run from this source directory:
  python3 tools/qualify_release.py --output ../qualification --with-sanitizers

Keep the output outside this source tree. Inspect the receipt for actual passes,
failures, and omitted checks. Version 0.20.0 retains ABI epoch 1 and SONAME .so.0;
the later 1.0 promotion requirements are recorded in ROADMAP.md.

License: LGPL-3.0-or-later. See LICENSE.md and THIRD_PARTY_NOTICES.md.
