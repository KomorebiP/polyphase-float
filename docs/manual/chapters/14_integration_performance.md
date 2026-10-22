# Application integration and performance measurement

Integrate the installed library first, then tune a representative workload.
This sequence separates packaging mistakes from numerical mistakes and gives
measurements a clear dependency configuration. The production core uses GMP
for integer machinery and does not require MPFR. The optional MPFR adapter is
a separate library for applications that need that boundary.

## Build an installation you can identify

Use CMake for an installed package. A serial core build is a useful starting
point because it has fewer optional dependencies and is easy to reproduce:

```sh
cmake -S . -B build/core \
  -DCMAKE_BUILD_TYPE=Release \
  -DPPF_OPENMP=OFF \
  -DPPF_MPFR_SUPPORT=OFF
cmake --build build/core -j2
cmake --install build/core --prefix "$HOME/.local"
```

Run these commands from the Polyphase source directory. `-j2` controls build
parallelism, not the number of workers used by a numerical operation. The
installation prefix is an application deployment choice; it need not be the
example shown here. On the supported Linux baseline, development headers and
libraries for GMP, a C11 compiler, POSIX threads, and the platform math library
are required. Install the development files for the architecture actually
being compiled, rather than relying only on a runtime shared library.

Both shared and static library forms are normally built. Use
`PPF_BUILD_SHARED=OFF` or `PPF_BUILD_STATIC=OFF` when only one form is wanted,
leaving at least one enabled. Use a fresh build directory when changing the
compiler, sanitizer configuration, or major dependency settings. A stale
CMake cache can otherwise continue to select an old compiler or dependency
path despite changes in the shell environment.

Native instruction tuning is disabled by default. Enable
`PPF_ENABLE_NATIVE_OPTIMIZATIONS` only when the deployment machines support
the instructions selected for the build machine. A binary that performs well
locally but fails with an illegal instruction on a recipient's computer is
not a successful portable optimization.

## Consume exported CMake targets

In a separate application directory, use an installed package rather than
copying internal source files into the application:

```cmake
cmake_minimum_required(VERSION 3.23)
project(polyphase_application LANGUAGES C)
find_package(PolyphaseFloat CONFIG REQUIRED COMPONENTS core)
add_executable(polyphase_application main.c)
target_link_libraries(polyphase_application PRIVATE PolyphaseFloat::shared)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix when configuring that
application. It should not point at the Polyphase source tree. The exported
target supplies include paths and its declared link dependencies. Select
`PolyphaseFloat::static` for the archive form, provided that form was installed.
This is preferable to maintaining a handwritten list of transitive
dependencies for each OpenMP and platform configuration.

Request `COMPONENTS core` when the application does not use MPFR. An
unqualified package request can load an installed adapter as well. Request
`COMPONENTS mpfr` when the adapter is required; the package then loads the
core and exposes the installed `PolyphaseFloat::mpfr_shared` and/or
`PolyphaseFloat::mpfr_static` targets. A missing required component should fail
configuration early instead of becoming an unexplained link error later.

An installed header being found does not prove that the intended library is
loaded at runtime. Configure the platform loader for the installed shared
library directory using the application's deployment policy. Build-tree
runtime paths are convenient during development but are not a replacement
for a distributable application's installation layout. When diagnosing an
unexpected version, compare runtime identity with the headers and package
configuration selected during compilation.

## Using pkg-config or Make

The core package name is `polyphase-float`; the adapter package name is
`polyphase-float-mpfr`. For a small application with installed dependencies:

```sh
cc -std=c11 main.c -o my_app \
  $(pkg-config --cflags --libs polyphase-float)
```

If the prefix is outside the system search path, add the directory containing
the installed `.pc` files to `PKG_CONFIG_PATH`. Library directories can be
nested or architecture-specific; use the actual installation layout instead
of assuming that every system installs into a directory literally named
`lib`. The installed pkg-config metadata is designed to follow a relocated
complete prefix when relative install directories are used.

`pkg-config --static --libs polyphase-float` adds the private dependency flags
needed by static linking. It does not itself force the linker to select an
archive when a shared library is also available. Use the explicit CMake static
target, a static-only installation, or a deliberate linker selection when
that distinction matters. Fully static linking of all system dependencies is
a separate platform question from selecting Polyphase's static archive.

The source Makefile supports development commands such as `make test`,
`make audit`, and named `bin/` targets. CMake is the canonical installation
route. Keep independent compiler or sanitizer builds in separate directories
or clean copies; reusing old objects with different instrumentation can
produce misleading failures. A successful source-tree test does not by itself
exercise downstream discovery of an installed package.

## Crossing the MPFR boundary

Build and install the adapter with official MPFR development headers when an
application needs `<polyphase_float_mpfr.h>`. Some local validation modes can
use a minimal compatibility declaration set with an existing MPFR shared
library. That arrangement does not supply the installable adapter contract
for downstream applications. Prefer an explicit `PPF_MPFR_SUPPORT=ON` when
the adapter is required, so missing development dependencies are reported at
configuration time.

The three conversion directions have distinct precision policies:

| Interface | Precision policy | Return convention |
|---|---|---|
| `ppf_import_mpfr_exact` | Adopts the MPFR source precision for exact import | `0` on success, `-1` on failure |
| `ppf_set_mpfr` | Keeps the initialized Polyphase destination precision and applies the requested rounding and context range | Numerical ternary; check `PPF_TERNARY_ERROR` before its sign |
| `ppf_get_mpfr` | Rounds into the initialized MPFR destination's precision using an MPFR rounding mode | MPFR numerical ternary |

All arguments must be valid, and destinations must satisfy their respective
initialization requirements. A return value is not a substitute for these
preconditions. Exact import may change the destination precision before a
later failure; it does not promise to preserve the previous destination on
every failure.
Check the function-specific return convention rather than assuming that every
adapter call returns the same kind of status. MPFR's own flags and exponent
policy remain part of the MPFR side of the boundary. A Polyphase context is
not a replacement for MPFR's runtime state.

The adapter also provides two enumeration translators:
`ppf_rounding_from_mpfr` maps an MPFR rounding mode to its corresponding
Polyphase mode, and `ppf_rounding_to_mpfr` maps in the opposite direction.
Both translate the six concrete modes: nearest, toward zero, upward,
downward, away from zero, and faithful. Both fall back to nearest for an
unrecognized value; they do not return an error indicator.
In particular, `ppf_rounding_to_mpfr(PPF_RND_DEFAULT)` yields `MPFR_RNDN`.
It does not consult a Polyphase context. If the intended policy is a
context's current default, resolve that policy to its concrete rounding
mode before translating it.

The complete
[systems_mpfr_bridge.c](../../../examples/manual/systems_mpfr_bridge.c)
example imports an exact MPFR value `5/4`, verifies adoption of its 128-bit
precision, multiplies by `8` in Polyphase, and exports the exact result `10`
to a 53-bit MPFR destination. The small exact values make the example's
precision changes visible without confusing them with unavoidable rounding.

For migration of a larger calculation, convert at a well-defined boundary,
perform a substantial sequence of operations in one representation, and
convert back when needed. Alternating representations at every elementary
operation can add allocation, conversion, and repeated-rounding costs. Measure
the complete application path if frequent conversion is unavoidable. Do not
attribute all of that path's cost to either arithmetic engine alone.

## Make the workload stable before tuning

Start by reusing initialized objects and choosing realistic working
precisions. Repeated initialization and teardown inside a tight loop may
dominate a small arithmetic operation. Separate object setup, cache warmup,
plan construction, and timed application according to what the real workload
does. A reusable divisor plan is valuable only when enough applications
amortize its construction and storage costs.

Preserve the intended numerical calculation while optimizing it. A fused
operation and a sequence with an intermediate rounding are different
computations. Choosing fewer printed digits improves presentation costs but
does not reduce the precision of an already computed result. Likewise,
raising a worker count is a scheduling request, not an assurance of lower
latency. Record which question a measurement is intended to answer before
changing several variables at once.

The portable product planner selects algorithms from operation, size, and
worker conditions. Optional machine-specific profiles can change those
choices; explicit algorithm requests have their own selection semantics.
Keep any custom profile with the build record. A profile chosen on one CPU
or one cache-placement policy is not automatically suitable for another.

## Interpret measurements at their actual scope

A useful timing record identifies the operation, inputs or generation rule,
precision, rounding mode, worker count, build flags, dependency builds,
hardware, and correctness check. Retain raw samples rather than only a best
time. State whether the result measures a cold call, a warmed cache, a planned
application, or an end-to-end task including setup and conversion.

Compare equal questions. A many-worker latency measurement and a one-worker
measurement answer a scaling question; they are not automatically an
equal-resource throughput comparison. Include plan construction when claiming
benefit for a one-off division. Include output formatting when the application
spends most of its time writing results. If system load or CPU placement
changes between runs, an observed difference may have several causes.

The repository's benchmark pipeline retains correctness-gated samples and
uses independent process launches as the experimental units. Multiple samples
inside one process describe that process's variability; they do not establish
independent reproducibility. A correctness gate checks the tested values and
conditions, not every mathematical input. Report performance as a result for
the measured configuration and workload, with its uncertainty and omitted
costs stated. No benchmark smoke test alone establishes a general speedup.
