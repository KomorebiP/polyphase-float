# Failed or conditional constant experiments

## Full packet GCD reduction for log(2)

Every combined packet was divided by `gcd(P,Q,T)`. This reduced intermediate
integer sizes but required two large GCDs at every tree node.

At one million bits on the development host:

```text
normal packet recurrence, 1 worker: about 0.33 s
GCD-reduced recurrence, 1 worker: about 1.29 s
```

The experiment was rejected. It establishes that symbolic cancellation must
be represented structurally rather than discovered repeatedly with general
large-integer GCDs.

## Root-only rational reduction for log(2)

The root `T/Q` rational was reduced by a final GCD, and the omitted tail was
bounded separately by

\[
\frac{3}{4\,9^n(2n+1)}.
\]

The interval was correct, but the final multi-million-bit GCD dominated the
saved endpoint arithmetic.

At one million bits with one worker it took about 0.64 s, versus about 0.33 s
for the unreduced production path.

The experiment was rejected from automatic use.

## Cached retrieval

An exact interval cache alone still required endpoint rounding on every call.
A per-rounding-mode `ppf_t` cache reduced that cost substantially. Cached
retrieval remains slower than MPFR's mature cache because copying the
independent object and locking the cache are still measurable at large
precision.
