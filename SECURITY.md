# Security policy

## Supported versions

The project is pre-1.0. Security fixes are applied to the current development
branch and the most recent tagged release candidate. Older snapshots are not
maintained unless explicitly stated.

## Reporting a vulnerability

Do not disclose a suspected vulnerability in a public issue before maintainers
have had a reasonable opportunity to assess it. Use the repository's private
security-advisory mechanism when available. If no private channel is configured,
open a minimal issue that requests private contact without including exploit
details, secrets, private data, or a proof of concept.

Include:

- affected version or commit;
- platform, compiler, and build options;
- a minimal reproducer;
- expected and observed behavior;
- memory-safety, denial-of-service, correctness, or supply-chain impact;
- whether the issue is already public.

Numerical mis-rounding, corrupted serialization, allocator misuse, unchecked
size arithmetic, and thread-safety failures are treated as security-relevant
when they can affect untrusted inputs or application decisions.
