# Governance

Polyphase Float is currently maintainer-led.

## Decision authority

The maintainer has final responsibility for releases, API policy, licensing,
security handling, and acceptance of changes. Technical decisions should be
supported by reproducible tests, proofs where applicable, and benchmark data
when performance is claimed.

## Change requirements

A change may be merged only when its scope is understood and the applicable
checks pass. Changes to numerical semantics, public ABI, serialized formats,
or benchmark methodology require explicit review and documentation.

## Project claims

No contributor may silently strengthen a project claim. The repository must
continue to distinguish:

1. implemented behavior;
2. behavior covered by tests;
3. behavior supported by a mathematical argument;
4. externally replicated behavior;
5. experimental or conjectural work.

## Future governance

If the contributor base grows, maintainers may adopt a multi-maintainer review
policy and publish a formal voting or succession procedure. Until then, the
version-control history is the authoritative record of decisions.
