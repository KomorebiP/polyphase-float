# Progress After Runtime Completion

## Literal inventory

The supplied lowercase MPFR inventory is now closed:

```text
349 / 349 direct or conditional counterparts
100.00 percent
```

## Public surface

```text
423 exported ppf_* functions
1 additional function-like nearest-away macro
0 imported mpfr_* symbols in the production library
```

## Completed major families

- object model, memory ownership, contexts, ranges, flags, and rounding
- initialization, assignment, parsing, conversion, and fit tests
- complete arithmetic, comparison, root, utility, and remainder families
- certified constants and caches
- logarithm, exponential, power, trigonometric, hyperbolic, and special functions
- stream/raw I/O and formatted output
- rounding certification and nearest-away rounding
- random distributions
- version/build queries
- memory/cache cleanup
- MPF compatibility and custom storage
- conditional binary128 and decimal formats

## What “complete” now means

Every lowercase symbol in the supplied inventory has a `ppf_*` counterpart on the supported build, with conditional interfaces enabled when the compiler supplies their types. The library can be built and used without linking MPFR.

## What remains before a mature public replacement

- choose and publish a redistribution license
- freeze and document the ABI
- test additional compilers, operating systems, endianness, limb widths, and GMP releases
- add continuous property fuzzing and long-running stress tests
- obtain independent external review and replication
- consolidate user and developer documentation
- tune the slowest special, trigonometric, formatting, scalar-wrapper, random, and certification paths
- define explicit practical-domain and resource-limit guarantees for every advanced function

A fair engineering assessment is that the API-construction phase is complete, while production hardening and performance maturity remain substantial projects.
