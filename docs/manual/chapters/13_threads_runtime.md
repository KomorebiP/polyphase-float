# Threads, runtime policy, and cache lifetime

The simplest concurrency arrangement gives each worker its own context and
destination objects, while sharing only inputs that remain immutable until
all workers finish. This arrangement works whether the library was built with
internal OpenMP support or with its serial numerical paths. Application
threads and internal numerical workers are separate layers of concurrency.

## Share values, own mutations

| Resource | Concurrent-use rule |
|---|---|
| Initialized number used only as an input | May be shared while its storage and value remain unchanged |
| Destination number | Requires exclusive ownership during mutation |
| Explicit context | Requires one-thread-at-a-time ownership or an application lock |
| Thread-default context | Private to its calling thread |
| Completed divisor plan | May be applied concurrently with separate destinations and contexts |
| GMP random state | Requires exclusive ownership or an application lock |
| Constant and transcendental caches | Internally synchronized; purges may wait for active users |

An input is no longer read-only when another thread resizes it, swaps it,
clears it, edits its limbs, or uses it as an aliased output. Keep those actions
outside the readers' lifetime. `const ppf_t *` helps express the intended use,
but it cannot stop a different pointer from mutating the same object. The
application must enforce the ownership rule.

An explicit context is mutable even when every numerical operand is
read-only. An operation can update its flags or use its scratch arena. Passing
one shared context to otherwise independent calculations therefore needs
synchronization. Giving each worker a context is usually simpler than locking
around every arithmetic call. If the workers need common settings, initialize
their contexts from a stable policy or clone a context before starting the
concurrent work. A clone copies flags as well as policy, so clear inherited
flags when a clean diagnostic interval is wanted.

The complete
[systems_threads.c](../../../examples/manual/systems_threads.c) example starts
two POSIX threads. They both read one initialized value, `7`, and independently
square it into private destinations. Each checks that its result is `49`.
Only the main thread prints, after joining both workers, so the example's
output is deterministic. The shared input is cleared after the joins. The
same pattern scales to a larger worker pool: worker-local context, local
output, immutable shared input, and an explicit completion boundary.

## Default contexts and flags

The convenience default context is thread-local. Changing one thread's
default precision, exponent range, rounding mode, or flags does not change
another thread's defaults. This is useful for a small program, but it also
means that a worker cannot report its numerical flags merely by having the
main thread inspect the main thread's default context.

Collect the status and flags in the worker that performed the operation, then
publish an application result record after the work is complete. Such a record
can contain the operation status, a copied flag mask, and the resulting value
or a reference whose ownership is transferred to the receiving thread. Do not
publish a pointer to a worker's stack-allocated context or to an object that
the worker clears before the receiver finishes reading it.

Formatting functions without a context argument use the calling thread's
default context. A worker using an explicit numerical context can therefore
have two relevant diagnostic sources: its explicit calculation context and
the default context used for formatting. Keeping presentation in a separate
stage can make this distinction easier to manage.

## Choosing the level of parallelism

The CMake option `PPF_OPENMP` accepts `OFF`, `ON`, and `AUTO`. `OFF` builds
without the OpenMP runtime dependency. `ON` requires usable OpenMP support for
the chosen compiler. `AUTO` enables support when the compiler and runtime
probe succeeds. These options do not change the ownership rules for objects,
contexts, or custom allocator state.

Operations exposing a worker-count argument receive a request, not a promise
to launch that many workers. The selected algorithm may have fewer useful
tasks; small inputs may use a serial route; and nested parallel execution can
be limited. A request of many workers is therefore not evidence that the
operation used the whole machine. Consult operation statistics where the
interface provides them and retain the build configuration when measuring
parallel behavior.

The constant-family default worker setting is process-wide and atomic. The
trigonometric default worker preference is thread-local. Configure these
preferences deliberately, preferably before the relevant workload starts.
Changing a process-wide preference during a timing campaign makes the
campaign harder to interpret even when the change is safely synchronized.
Explicit worker-count forms are useful when one call needs a policy different
from the surrounding application's default.

Avoid oversubscription. For a server processing many independent requests,
several application workers each requesting a large OpenMP team may compete
for the same cores and memory bandwidth. Begin with one parallel layer, then
measure whether nested or mixed execution improves the actual workload. A
throughput-oriented server and a single very-high-precision calculation can
need different policies. Neither a worker count nor an environment setting
alone guarantees CPU affinity or exclusive access to a core.

## Reproducible results and reproducible execution

Specify precision, rounding mode, exponent range, and input values explicitly
when reproducibility matters. Record the library and dependency builds too.
Numerical reproducibility is different from repeating the same algorithm,
cache state, schedule, allocation count, or elapsed time. A planner can choose
an exact computational route that differs from another route while producing
the same rounded numerical result.

Faithful rounding needs particular care: its contract permits an allowed
neighboring result rather than the unique result selected by a specified
directed or nearest mode. Do not use a byte-for-byte comparison across
implementations as the sole validity test for faithful rounding. Likewise,
special-function acceptance remains subject to documented precision and
construction limits; a worker policy is not a guarantee that every possible
input can be certified within those limits.

For randomized calculations, keep a separate initialized GMP random state per
worker or lock a shared state. Choose and record the seeding scheme at the
application level. Reusing the same seed independently in every worker can
produce repeated streams, which may be undesirable even though the calls are
free of data races. Thread scheduling should not silently determine the
meaning of a reproducible experiment's sample numbering.

## Cache reuse and cleanup

Caches reduce repeated construction work for constants and related
calculations. They also retain memory after an individual function returns.
That retained memory is not automatically a leak: it can be intentionally
owned by a cache or a reusable thread workspace. Distinguish an increasing
unreachable allocation from a stable reusable allocation before changing an
application's memory policy.

`ppf_clear_constant_cache` releases a selected constant's cached value;
`ppf_free_constant_caches` clears the constant caches collectively.
`ppf_free_cache2(PPF_FREE_GLOBAL_CACHE)` covers the global-cache cleanup
interface. Purging shared caches is supported while other threads calculate,
but it takes the corresponding locks and may block. A later call can rebuild
what was removed. Periodic purging in a latency-sensitive request path can
therefore make both that request and subsequent requests slower.

`ppf_free_cache2(PPF_FREE_LOCAL_CACHE)` affects the calling thread's local
scratch pool and product workspaces. It cannot release another live worker's
local storage. If workers retain large temporary workspaces, schedule local
cleanup in those workers at a sensible boundary, such as after an unusually
large job or when returning a worker to an idle pool.

`ppf_thread_cleanup` provides explicit cleanup of heap-backed state owned by
the calling thread and may be called repeatedly. On the POSIX baseline,
ordinary thread exit also performs cleanup when the thread-specific destructor
key was successfully registered. `ppf_buildopt_thread_cleanup_p` reports that
key's availability. Explicit cleanup remains useful for controlled shutdown,
for the main thread, and before unloading the library.

Thread cleanup does not destroy application-owned numbers, explicit contexts,
divisor plans, or GMP random states. Clear those resources through their own
lifecycle interfaces. Before unloading a shared library, stop new calls, wait
for active workers to finish, release their resources and local state, and
only then unload it. Automatic thread cleanup cannot make a callback into an
already unloaded library safe.

Use runtime identity functions such as `ppf_get_version`, `ppf_abi_tag`, and
the build-option queries when constructing a diagnostic report. They help
identify the library actually loaded, which can differ from a header found
first on the compiler's include path. Keep these diagnostics separate from
numerical output that another program expects to parse.
