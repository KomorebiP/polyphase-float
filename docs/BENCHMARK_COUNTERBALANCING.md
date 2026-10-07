# Counterbalanced benchmark campaigns

Version 0.10.6 adds a publication-oriented campaign that measures `close` and
`spread` placement in paired process rounds.

## Design

Each pair of consecutive rounds contains one `close -> spread` order and one
`spread -> close` order. Which order appears first in the pair is selected from
a deterministic recorded seed. Extending a campaign does not change any earlier
round. Within every process round, benchmark conditions are shuffled
deterministically. A shared schedule group gives close and spread the same
condition positions, while the full binding-specific schedules and a
binding-independent order signature are retained.

This design addresses two distinct confounders:

1. binding order across process rounds;
2. fixed operation order within a process round;
3. unequal condition positions between the paired bindings.

## Evidence

The publication output contains `campaign_config.json`,
`counterbalance_schedule.jsonl`, `execution_log.jsonl`, the complete `close/`
and `spread/` campaigns, and a `paired_analysis/` directory. The paired analyzer
uses only matched process rounds. It reports the raw paired ratio, an
order-adjusted placement ratio, and a separate first-versus-second effect.

## Interpretation

A placement ratio above one in `close_over_spread_adjusted` means spread was
faster after balancing the two execution orders. A first-over-second ratio above
one means the second member of a pair tended to run faster. The latter must not
be misreported as a binding effect.

At least four paired rounds are required for a minimally balanced analysis.
Six or more are recommended for publication work, and narrow effects require
additional independent rounds.
