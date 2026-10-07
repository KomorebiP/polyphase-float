# Tested speed ideas that were rejected

These experiments are retained because they define useful boundaries.

## High-level precision-doubling Newton roots

A precision-doubling Newton implementation was tested for square root, reciprocal square root, cube root, and general roots. It was numerically correct, but each iteration allocated and rounded complete `ppf_t` temporaries. At one million bits it was substantially slower than both the existing exact integer-root enclosure and MPFR.

The failure does not reject Newton iteration itself. It rejects building Newton iteration from high-level rounded operations. A competitive version needs fused low-level iterations, reusable scratch storage, high products, and a final correction certificate.

The prototype source is preserved in:

```text
experiments/rejected_newton_roots.c
```

## Fully materialized direct FMA alignment

A direct-limb FMA path formed the full exact product, allocated a second full-width aligned accumulator, combined the addend, and rounded once. It was correct but slower at one and five million bits because the extra full-width memory traffic outweighed the conversion savings.

The next attempt should fuse product reconstruction directly into the signed phase accumulator instead of materializing both the product and the aligned sum.

## Equal-worker outer multiplication

Independent MPFR multiplications distributed over the same outer worker count are highly effective. In the development run, Polyphase's internally optimized arithmetic did not consistently beat MPFR when both processed the same independent batch with the same worker budget.

This is why internal-thread latency and equal-worker batch throughput are reported separately.

## General addition as a major speed target

Direct limb alignment removed most of the framework's previous overhead, but addition still trails MPFR. It is dominated by memory movement rather than expensive independent products. Further work may reach closer parity, but multi-fold gains are not a realistic default expectation for ordinary addition.
