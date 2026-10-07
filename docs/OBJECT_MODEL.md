# Floating object model

A regular finite value is stored as

\[
x=(-1)^s M2^{e-p},
\]

where `p` is the precision, `e` is the binary exponent, and `M` is an owned
little-endian limb integer satisfying

\[
2^{p-1}\le M<2^p.
\]

The top precision bit is always set. Bits above the declared precision are
always zero. This makes every regular value normalized and gives it one
canonical representation at a fixed precision.

Zero, infinity, and NaN are separate value kinds. Each retains a sign bit. No
subnormal value is stored. A context range maps results below the minimum
exponent to signed zero or the minimum normal value according to the rounding
mode.

## Object ownership

Each object owns its significand allocation unless it was initialized with the
custom-storage interface. It stores the callbacks needed to release its owned
allocation, so clearing an object does not depend on the lifetime of the
context structure that created it. The allocator's opaque state must still
remain valid.

`ppf_limbs_write` and `ppf_commit_regular` provide a controlled direct-limb path
for the arithmetic layer. An object is deliberately left uncommitted while its
limbs are being written. It becomes a regular value only after normalization
and exponent checks pass.

## Allocation failure state

`ppf_init2_ctx` leaves an all-zero object when its allocation fails. For the
covered context-owned operations in 0.10.9, an initialized destination remains
byte-for-byte unchanged after an unrecovered allocation failure. Context flags
may still change.

## Precision

Precision belongs to the object. `ppf_set_prec` changes storage and leaves a
deterministic NaN after a successful precision change. A failed reallocation
preserves the original object. `ppf_prec_round` preserves the value and rounds
it to the new precision. Widening is exact.

## Exact dyadic assignment

`ppf_set_z_2exp` accepts the exact value

\[
z2^k
\]

and performs one rounding to the object precision. It implements nearest-even,
downward, upward, toward zero, away from zero, and deterministic faithful
rounding. The return value is negative, zero, or positive according to whether
the stored value is below, equal to, or above the exact input.

## Range handling

Every context carries `emin` and `emax`. Overflow produces infinity or the
largest finite value according to sign and rounding direction. Underflow
produces signed zero or the minimum normal value. Nearest underflow uses the
midpoint between zero and the minimum normal value, with the exact halfway case
rounded toward zero.

The context records underflow, overflow, divide-by-zero, NaN, inexact, range,
and allocation flags.

## Concurrency

A fully initialized `ppf_t` may be shared read-only while no thread mutates,
resizes, swaps, clears, or reinitializes it. Every destination requires
exclusive ownership for the duration of an operation. Completed divisor plans
may be shared for application while their lifetime is externally protected.
See `THREAD_SAFETY.md` for the complete ownership boundary.
