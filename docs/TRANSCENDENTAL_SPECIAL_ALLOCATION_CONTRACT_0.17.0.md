# Polyphase Float 0.17.0 transcendental and special allocation contract

## Scope

This contract covers project-controlled allocations reached through 79 public
logarithmic, exponential, power, trigonometric, hyperbolic, and special-function
entry points. The governed surface begins at `ppf_log` and ends at `ppf_ai` in
the generated semantic ledger.

The contract applies to ordinary destinations, supported in-place calls,
paired numerical outputs, the auxiliary sign output of `ppf_lgamma`, and
application-managed external significand storage. It governs both serial
execution and the OpenMP trigonometric packet path.

It does not intercept allocations performed internally by GMP. It also does
not claim recovery from process termination, operating-system failure, an
invalid public object, or an allocator that violates the callback contract.

## Public result transaction

Each governed public function evaluates into independently owned staging
storage with the destination precision. The public destination is committed
only after the implementation returns and no new project-allocation failure
has been observed.

An unrecovered project-allocation failure obeys the following rule:

1. the function returns `PPF_TERNARY_ERROR`
2. `PPF_FLAG_ALLOC` is sticky on the resolved context
3. every initialized numerical destination remains byte-for-byte unchanged
4. paired numerical outputs either both commit or neither commits
5. the `ppf_lgamma` sign output remains unchanged
6. all project-owned staging and work buffers are released
7. a later non-failing call remains usable even when the allocation flag was already set

Destination preservation includes the significand pointer, allocator
provenance, external-storage status, capacity, used-limb count, precision,
exponent, sign, value category, and limb contents.

## Fixed-size commit

The commit step performs no allocation. It validates that the staging object
has the same precision and sufficient storage, then copies the normalized
significand and scalar value fields into the caller's existing allocation.
This preserves caller ownership and does not replace an external significand
with library-owned storage.

Paired results are both validated before either fixed-size copy occurs.
Invalid same-output use continues to follow the existing public aliasing
contract and is not converted into a supported overlap pattern.

## Nested allocation evidence

The transaction records both the context allocation flag and the internal
project-allocation failure epoch. The epoch prevents a nested context or a
recovered internal path from erasing evidence that a project allocation failed.
A preexisting sticky allocation flag is preserved without causing a later
successful operation to be rejected.

A thread-local active transaction context is available only to private work
array helpers. It is not part of the public ABI.

## Work-array provenance

Project C arrays in the fast logarithm and exponential kernels now use
allocator-provenance buffers. Bernoulli arrays used by gamma, digamma, and zeta
paths use the same mechanism.

The trigonometric binary-splitting implementation allocates serial workspaces
through the active transaction context. In the OpenMP path, packet nodes,
merge scratch, and every packet workspace are allocated and initialized before
the parallel region. Public allocator callbacks are therefore not invoked by
those workers on the governed path.

The packet-width calculation avoids addition-based unsigned overflow before
workspace sizing. This is a resource-bound correction and does not alter the
trigonometric series or rounding rule.

## Numerical boundary

Version 0.17.0 changes the allocation and commit boundary, not the mathematical
formulas, interval constructions, rounding modes, exponent-range handling, or
special-value rules of the governed functions.

The following remain outside this contract:

- allocations performed internally by GMP
- project allocations in the 137 public functions still marked incomplete in the semantic ledger
- bounded iteration and argument-domain limits recorded for 112 functions
- failures in external systems after an irreversible operation begins
- platform behavior outside the currently validated Linux and WSL baseline
