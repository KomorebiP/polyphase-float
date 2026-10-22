# Glossary

**ABI.** The application binary interface: calling conventions, symbol names,
public object layouts, enum values, and related requirements that let an
already compiled application use a compatible library.

**Aliasing.** Two arguments refer to the same object or to overlapping storage.
An allowed single-output in-place operation does not imply that two distinct
outputs may be collapsed into one object.

**Allocator provenance.** The identity and state of the allocator that owns an
allocation. Return storage through its documented release function rather
than choosing a deallocator by the pointer's C type.

**Arbitrary precision.** The application chooses the finite significand
precision. This is not infinite precision, unlimited memory, or a guarantee
that every difficult operation will certify at every requested precision.

**Binary exponent.** The power of two that scales a binary significand. The
library's normalized exponent conventions and configured bounds determine
range behavior. A decimal output exponent uses its own stated radix.

**Borrowed storage.** Memory the caller may access subject to lifetime rules
but does not own and must not release. Changing the owning object can
invalidate an object-storage pointer.

**Certificate.** An enclosure or other exact mathematical evidence used to
justify a rounded result. Certification can remain undecided within the
available precision or resource budget.

**Context.** The configuration and state used by operations: defaults,
rounding, exponent bounds, sticky flags, allocator callbacks, and working
storage. An explicit mutable context requires exclusive access.

**Correct rounding.** The stored result equals the result of rounding the
exact mathematical value once to the selected representable set and rule.
This property is stronger than observing agreement at two working precisions.

**Destination precision.** The precision attached to the output object.
Passing a higher-precision context does not silently enlarge an existing
destination. Some named precision-changing operations have explicit rules.

**Directed rounding.** Rounding toward positive infinity, negative infinity,
zero, or away from zero. For a negative value, toward positive infinity and
away from zero point in different directions.

**Dyadic.** An integer or rational scaled by an integer power of two. Every
finite binary floating-point value is a dyadic rational.

**Exact input.** The mathematical value represented by the supplied data.
Importing a C `double` exactly preserves that binary `double`, including its
earlier rounding. It does not recover an intended decimal value.

**Exponent range.** The configured interval used by operations to classify
and settle their output. Range limits, storage limits, and precision limits
are separate constraints.

**Faithful rounding.** Selection of an allowed neighboring representable
value. It is not the same contract as the unique nearest or directed result,
and its ternary output must be interpreted under the faithful-mode rules.

**Flag.** A sticky diagnostic bit in context state. Flags can outlive the
operation that raised them. Clear or save them deliberately when attributing
a diagnostic to one computation.

**Fused operation.** An operation that combines its terms before a final
rounding, rather than rounding each intermediate expression separately.
Its result can differ from a sequence of individually rounded calls.

**GMP.** The GNU Multiple Precision Arithmetic Library. Polyphase uses its
integer and rational machinery. GMP's own allocation behavior is outside the
recoverable project-allocator rollback contract.

**Guard precision.** Extra working precision used inside an algorithm. It
does not by itself prove that a final result is correctly rounded.

**Inexact.** The stored numerical result differs from the exact value in the
operation's contract. Conversion and formatting can introduce their own
rounding steps.

**Interval settlement.** Deciding whether every exact value in a supplied
enclosure has a common rounded output, compatible ternary direction, and
the required common range-flag state. An undecided result asks for better
evidence, not a guessed midpoint.

**LP64.** The supported C data model in which `long` and pointers are 64 bits
while `int` is 32 bits. This differs from native 64-bit Windows' usual LLP64
model.

**MPFR.** A separate multiple-precision floating-point library. Polyphase's
optional adapter imports and exports its values; the independent production
core does not need it. Differential test agreement is finite execution
evidence, not a replacement for mathematical arguments.

**NaN.** Not a Number, a special value used for invalid numerical results and
some numerical-rejection paths. Inspect status and flags to understand the
particular outcome.

**Nearest, ties to even.** Select the nearest representable value. At an exact
midpoint choose the candidate whose retained least-significant significand
bit is even.

**Numerical rejection.** An operation cannot produce a certified result
under its resource or admission policy. It may return `PPF_TERNARY_ERROR`,
raise `PPF_FLAG_ERANGE`, and commit NaN. This differs from allocation rollback.

**Precision.** The number of bits available in the significand, including the
leading significant bit. The rough decimal information capacity is
`precision * log10(2)`, not a promise that a chosen decimal expression has
that many correct digits.

**PPFIF.** Polyphase's binary interchange format. Its import contract includes
admission limits and validation before committing the numerical destination.

**Remainder bound.** A bound on terms omitted from a finite series or
asymptotic expression. A decreasing term or a small last term is not alone
a universal remainder proof.

**Representable neighbor.** The adjacent value in the destination's
precision and range model. Spacing changes with binary exponent.

**SONAME.** The ELF shared-library name recorded for runtime loading. The
project's release version and its governed ABI SONAME need not have the same
major number.

**Status return.** A success or failure code, predicate, count, or other
non-ternary return. An `int` declaration alone does not identify a numerical
ternary. In particular, `ppf_set_str` and `ppf_strtofr` have different return
contracts.

**Sticky diagnostic.** A diagnostic that remains set until explicitly
cleared or restored. A successful later operation does not erase it.

**Subnormalization.** The explicit operation that implements the library's
reduced-precision range semantics when requested. An exact reduced-precision
result can still raise underflow under that operation's contract, while
inexactness depends on the rounding relation.

**Ternary.** A numerical return whose sign compares the stored result with
the exact result. Zero means exact, negative means below, and positive means
above, subject to the selected rounding contract. The error sentinel is not
a numerical direction.

**Thread default.** State owned by the calling thread and selected by APIs
that permit a null context. It is distinct from an application-owned explicit
context and from global synchronized caches.

**Transactional allocation failure.** An unrecovered project-allocation
failure preserves the prior numerical destination under the operation's
documented contract. This does not undo file I/O, kernel effects, GMP failure,
or arbitrary application callbacks.

**ULP.** A unit in the last retained place, measured at a specified precision
and exponent. It is not a fixed absolute distance over the whole number line.

**Working precision.** The precision used to construct intermediate evidence
or values. It may exceed the destination precision and may increase during
bounded certification attempts.
