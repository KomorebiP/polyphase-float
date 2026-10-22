# Independently certified constants

## Common evaluation path

The four public constants use the same architecture:

\[
\text{requested precision and rounding mode}
\longrightarrow
\text{certified dyadic interval}
\longrightarrow
\text{independent endpoint rounding}
\longrightarrow
\texttt{ppf\_t}.
\]

A builder returns integers \(L,U\) and an exponent \(e\) satisfying

\[
L2^e \le c \le U2^e.
\]

The shared evaluator rounds both endpoints. If they select the same binary
value, prove the same ternary direction, and agree on underflow and overflow
flags, that result is returned. Otherwise
the working precision grows by at least ten percent plus 64 bits and the
constant-specific calculation is repeated.

Version 0.16.0 performs the final rounding into a staged object. The caller's
destination is committed only after successful construction and rounding.

## Pi

The pi calculation uses the exact Chudnovsky transfer law

\[
P=P_LP_R,
\qquad
Q=Q_LQ_R,
\qquad
T=T_LQ_R+P_LT_R.
\]

The public 0.16.0 constant builder uses the exact non-adic binary-splitting
execution path. This retains the same Chudnovsky identity and certificate while
excluding optional standalone p-adic paths that are not yet allocation-atomic.

If the fixed-point result is the integer \(v\) at scale \(S\), the formal
certificate used here is

\[
(v-4)2^{-S}<\pi<(v+4)2^{-S}.
\]

The radius covers the omitted Chudnovsky tail, square-root truncation, and the
final nearest-integer quotient. The tail test is checked before an interval is
accepted.

## Logarithm of two

The exact identity is

\[
\log 2
=2\operatorname{atanh}\!\left(\frac13\right)
=\frac23\sum_{k=0}^{\infty}
 \frac{1}{(2k+1)9^k}.
\]

Let

\[
r_k=\frac{2k+1}{9(2k+3)}.
\]

The packet leaves are

\[
P_k=2k+1,
\qquad
Q_k=9(2k+3),
\qquad
T_k=Q_k,
\]

and use the same transfer law as the pi packets. At the root, \(T/Q\) is the
finite product-sum and \(P/Q\) is the first omitted product. Since every later
ratio is smaller than \(1/9\),

\[
0<\log 2-\frac{2T}{3Q}<\frac{3P}{4Q}.
\]

Equivalently,

\[
\frac{8T}{12Q}<\log 2<\frac{8T+9P}{12Q}.
\]

The rational endpoints are converted outward to a dyadic interval and passed
to the common rounding layer. Packet arrays and every worker scratch region
are allocated before OpenMP execution.

## Euler's constant and Catalan's constant

These builders use the certified special-interval machinery at widened
precision, then convert the completed enclosure to dyadic integer endpoints.
The widened context is cloned from the caller so project work allocations obey
the caller allocator. Allocation failure in the nested context is propagated
to the caller before any destination or cache commit.

## Caches

Each constant owns a mutex-protected exact interval cache. The cache stores an
enclosure rather than only one rounded value, so it can answer lower precisions
and different rounding modes.

A second cache stores the completed `ppf_t` result separately for each rounding
mode at the most recently requested precision. Exact and rounded cache updates
are constructed privately and committed by swap. A failed growth attempt
leaves the last valid cache unchanged.

## Threading

The pi calculation exposes exact binary-splitting parallelism. The log 2
calculation partitions its term interval into balanced packet ranges and
merges them in parallel.

Cache access is thread-safe. Construction for one specific constant is
serialized by its cache mutex. Different constants own different caches and
locks.

## Allocation boundary

The 0.16.0 fail-at-every-project-allocation matrix governs the public constant
family and its C work arrays. It does not intercept GMP internal allocation.
See `CONSTANT_ALLOCATION_CONTRACT_0.16.0.md`.

## Honest performance boundary

The development host showed a large cold-computation advantage for pi. The
first log 2 engine was slower than MPFR with one worker but faster in the
four-worker single-call comparison.

Two cancellation experiments were rejected:

- reducing every packet by a full three-way GCD
- reducing the root rational by a final large GCD and replacing the tail with a closed bound

Both were exact and substantially slower. The cost of large GCDs exceeded the
saved arithmetic. They remain compile-time experiments rather than production
defaults.
