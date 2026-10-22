# Troubleshooting and diagnostic workflow

Begin with the failing call, its actual return convention, and the context
used by that call. Record the input values and their precisions before
changing code or increasing precision. Many apparent arithmetic failures are
caused by reading the wrong flags, changing a destination's precision in a way
that discards its value, or loading a library different from the one used at
compile time.

## First distinguish status from rounding

An arithmetic ternary result is negative when the stored result lies below
the exact result, zero when exact, and positive when above, subject to the
operation's documented special cases. `PPF_TERNARY_ERROR` is a distinct error
sentinel. Therefore a condition such as `if (ppf_add(...))` treats ordinary
inexact arithmetic as failure. Check the sentinel first, then interpret the
ternary relation if needed.

Other interfaces use other conventions. `ppf_set_str` returns zero on
successful complete conversion, while `ppf_strtofr` returns a ternary relation
or its error sentinel and also reports an end pointer. PPFIF import/export use
zero for success. Formatting returns a byte count or a negative failure
result. Stream text functions return counts. A generic wrapper must preserve
these distinctions instead of converting every nonzero integer to an error.

Flags are sticky and describe the context on which they were raised. Clear
the relevant flags before an isolated diagnostic call, read them immediately
afterward, and preserve them before unrelated output formatting can change
another context. A normal return together with `PPF_FLAG_ALLOC` can describe a
successful fallback after a preferred allocation was refused.

## Numerical symptoms

| Symptom | What to examine | Practical next step |
|---|---|---|
| A correct-looking result is reported as an error | Was a nonzero ternary result treated as failure? | Compare with `PPF_TERNARY_ERROR`; report inexactness separately |
| A value unexpectedly becomes NaN after changing precision | Was `ppf_set_prec` used? | Use `ppf_prec_round` when the old value must be retained and rounded |
| Increasing an input's precision does not improve the result | What is the destination precision, and how were the inputs originally obtained? | Increase working and destination precision deliberately; reconstruct inputs from exact source data |
| Decimal input differs from its decimal spelling | Is the value exactly representable in binary at this precision? | Inspect inexactness and rounding; use an exact integer/rational source when appropriate |
| Printing fewer digits seems to change the result | Is the discrepancy only in the formatted representation? | Compare stored values independently of their display strings |
| A result overflows or becomes tiny unexpectedly | Is the intended context range active? | Record `emin`, `emax`, and the explicit/default context choice before the call |
| Flags appear unrelated to the current operation | Were earlier sticky flags retained, or was the wrong context queried? | Clear and inspect the same context around the operation |
| A special function returns an error on finite input | Is the input admitted, and could the bounded evaluator resolve it? | Check the function's domain and construction limits; retain the flags and exact input for diagnosis |
| Faithful results differ in the last bit | Is the comparison expecting a unique nearest result? | Check the faithful-rounding contract or choose a specified directed/nearest mode |

More precision cannot repair information lost before the value entered the
library. Passing a decimal literal through a C `double` first fixes it to that
binary approximation. Reading the original string or constructing the exact
rational gives the high-precision calculation the intended starting data.
Similarly, a later high-precision destination cannot undo an earlier rounded
intermediate. Identify the first irreversible rounding step before adding
precision everywhere.

For a suspected numerical defect, reduce the calculation while retaining the
same rounding mode, precision, range, and special-value categories. Compare
out-of-place and permitted in-place forms when aliasing is involved. If an
independent reference is used, match precision and exponent policy on both
sides and distinguish a reference disagreement from a proven library defect.
Keep the original failing input even if a smaller reproducer is found.

## Text, files, and ownership symptoms

| Symptom | Likely check | Practical next step |
|---|---|---|
| A parsed field accepts an unwanted suffix | Was the prefix parser used without checking `endptr`? | Use `ppf_set_str`, or validate the suffix explicitly |
| Text output cannot be read back | Base, exponent marker, locale, or too few digits may differ | Define the text grammar and locale; use PPFIF for saved objects |
| `ppf_get_str` digits look too large by a power of the base | The separate exponent may have been ignored | Place the radix point before the first digit and apply the returned exponent |
| A formatted buffer is silently incomplete | Was the `snprintf` required length checked? | Treat a nonnegative length at least as large as capacity as truncation |
| Freeing a returned string crashes | Which interface allocated it? | Pair `ppf_get_str` with `ppf_free_str`, and `ppf_asprintf` with `free` |
| An imported object's precision changes | PPFIF restores encoded precision | Import into a temporary, then assign into the desired working precision |
| A small binary record is rejected for resources | The encoded precision can require large significand storage | Check the 64 MiB default import bounds and the producer's record metadata |
| Retrying failed input starts at the wrong place | Input consumption is not rolled back with the number | Reposition a seekable stream or restore an application-owned input buffer |

Do not diagnose a saved record solely from its extension. PPFIF has its own
identifier, layout, and integrity check. MPFR FPIF, a raw structure dump, and a
text number are different representations. A checksum mismatch is a reason
to reject a record; removing the check to make an import succeed conceals the
problem. Retain a bounded failing record and the producer's identity when
investigating interoperability.

## Allocation and concurrency symptoms

| Symptom | What to examine | Practical next step |
|---|---|---|
| Clearing one number damages another | Were live structures copied by assignment or `memcpy`? | Use initialized destinations with `ppf_set`, or exchange ownership with `ppf_swap` |
| A custom allocator fails during cleanup | Did its opaque state or pool expire too early? | Keep allocator state alive until all dependent objects, strings, and contexts are cleared |
| A failed resize destroys the original block | Does the custom reallocator preserve its input on failure? | Correct the callback contract before retrying numerical work |
| Memory remains after a large calculation | Is it an arena, thread workspace, or global cache? | Clean the relevant owner at a deliberate lifecycle boundary |
| Several threads occasionally produce corrupted state | Are a context, destination, random state, or raw buffer shared mutably? | Give workers private state or protect the whole affected operation with a lock |
| More workers make a workload slower | Are internal teams competing with application workers? | Measure one parallel layer first, then compare controlled configurations |
| Shutdown or library unload crashes | Can another thread still call the library or run its destructor? | Join workers and release their state before unloading |

An allocation flag does not identify every process allocation. If a GMP or
system allocation fails outside the recoverable project allocator domain,
the ordinary destination-preservation guarantee is not a complete recovery
strategy. Likewise, a custom allocator that returns an invalid pointer or
reports a false capacity breaks the caller contract. Start fault-injection
tests with a simple, correctly accounting allocator and a valid initialized
destination, as in the allocation example.

## Build and deployment symptoms

| Symptom | Practical next step |
|---|---|
| GMP header or library discovery fails | Install matching development files and check compiler architecture and search paths |
| A core-only consumer unexpectedly requests MPFR | Use `find_package` with `COMPONENTS core` |
| The adapter is unavailable after installation | Build it with official MPFR development headers and request `COMPONENTS mpfr` in the consumer |
| A static link has unresolved dependencies | Use the exported static target or pkg-config's static dependency flags; remember that selecting an archive is a separate step |
| A program links but cannot load a shared library | Correct its installed runtime-library search configuration |
| OpenMP configuration or linking fails | Select the matching compiler runtime, or build with `PPF_OPENMP=OFF` |
| An executable fails with an illegal instruction elsewhere | Check native compiler tuning and the target CPU instruction set |
| Tests or examples are absent from a build | Review CMake feature detection and build options, then list configured tests |

When a build behaves inconsistently after a compiler or sanitizer change,
configure a new directory before investigating numerical code. Run one small
installed consumer to distinguish package discovery and loader problems from
failures inside a larger application. Capture the actual library path and
runtime version, not only the version requested in a build script.

## Prepare a useful diagnostic record

A concise reproducer should initialize its own objects and contexts, contain
the exact input construction, perform the failing operation, check the result,
and clean up. Include the precision of every relevant object, the rounding
mode, exponent limits, return value, and flag mask. For a parser problem,
include the bytes or a safely bounded file, the base, and the first unconsumed
position. For an aliasing problem, state which pointers refer to the same
object. For a concurrency problem, identify which resources are shared and
the point at which readers finish.

Also record compiler and dependency versions, build options, optional
interfaces, worker settings, and whether the failure occurs in a fresh
process. Keep the original diagnostic output and distinguish an actual
failure from a test that was never executed. A missing executable permission,
an unavailable sanitizer runtime, or a timeout is useful evidence, but none
is a passing numerical result. These details make a report actionable without
requiring another reader to reconstruct the application's entire environment.
