# Numerical limits and completion obligations

This reference explains when a calculation may be rejected, when an internal
shortcut gives way to another algorithm, and what evidence supports those
choices. It applies to the source accompanying this document, with default
compile-time settings. It is not a guarantee that every valid input finishes
within a fixed amount of memory or time.

The numerical gate for a stable 1.0 release is **not yet complete**. The source
limits have been inventoried and several boundaries have direct tests, but
some optimized enclosure arguments and cap-exhaustion tests remain open.
[NUMERICAL_ARGUMENTS.md](NUMERICAL_ARGUMENTS.md) records those dependencies.
The release version and qualification status are defined by the release
manifest, not by this document's inclusion in a 1.0 preparation workspace.

## What a limit means for a caller

A mathematical domain error, a finite implementation budget, and allocation
failure are different events. A real function can be mathematically defined
at an input that the current evaluator cannot certify within its budget.
Check the return value and context flags before using the result. Numerical
rejection commonly returns `PPF_TERNARY_ERROR`, sets `PPF_FLAG_ERANGE`, and
writes NaN. The transaction/allocation contract has separate rules, including
preserving the destination on an allocation failure; see
[ALLOCATION_FAILURE_CONTRACT.md](ALLOCATION_FAILURE_CONTRACT.md),
and the relevant function's semantic-contract entry.

Increasing destination precision does not necessarily cure a rejection: it
also increases the requested work. Increasing the exponent range does not
remove a series, recurrence, or construction limit. Exact cases can bypass a
limit that applies only to a general evaluator. In particular, an exact zero
or infinity rule is handled before some ordinary finite-input checks.

| Limit visible to callers | Default behavior | Important qualification |
|---|---|---|
| Airy Ai at finite `abs(x) > 500` | Numerical rejection | The comparison is against the exact input, including values just beyond 500. |
| Bessel J/Y general finite evaluation with `abs(n) > 100000` | Numerical rejection | NaN, zero, and infinity cases are handled earlier. |
| Upper incomplete Gamma with finite `x > 0` and `a < -100000` | Numerical rejection before recurrence setup | `x = 0` delegates to Gamma; `x = +Inf` returns zero. Passing the bound does not guarantee that all subsequent work succeeds. |
| Ordinary trigonometric reduction requiring more than 1000000 extra bits | Numerical rejection if no earlier exact route applies | Exact scaled-angle identities and grids have separate routes. |
| General-transcendental or special-function dyadic endpoint alignment exceeding 1000000 extra bits | Retry or numerical rejection | This is an intermediate-integer construction bound, not a maximum destination precision. General transcendental `m1` subtraction has the same bound on alignment with one. |
| Very large text powers or digit requests | Exact conversion, conservative range classification, or rejection, depending on branch | The default exact-power limit is 2000000. It is not a maximum input-string length. |
| FPIF import metadata requiring more than 64 MiB for a payload or rounded limb storage | Rejection before the corresponding allocation | Successful admission still depends on the stream, allocator, and representation checks. |
| An interval remains ambiguous after the evaluator's retry budget | Numerical rejection or documented fallback | A budget does not prove that all correctly rounded results are reachable. |

The 1000000-bit alignment limit is intentionally local. Ordinary exact
arithmetic, exact conversion, and GMP temporaries do not share a universal
allocation cap. Applications must budget input size, precision, and lifetime
as well as inspecting return values. A bounded test harness does not change
the library's allocation contract.

## Reading the audit tables

`p` is destination precision, `q` is working precision, and a *certificate*
is an interval containing the exact result together with enough information
to decide its rounded value, ternary direction, and range flags.

The classifications are:

- **Admission**: reject a general path before work outside its supported budget.
- **Retry**: bound the number of attempts to obtain a decisive certificate.
- **Series/planner**: bound terms, shifts, or reduction work; acceptance still
  requires the mathematical tail or interval condition.
- **Dispatch**: select a faster method or exact shortcut; declining it does
  not by itself reject the public operation.
- **Representation/structure**: a type width, storage layout, finite input
  traversal, or machine-format definition, rather than a convergence limit.

Evidence labels keep distinct claims separate. **Boundary checked** means the
stated finite cases were executed with a receipt. **Argument recorded** means
an enclosure or admission argument is written, subject to its listed
primitives and external identities. **Open** names work still required.
A named test is a reproducible entry point, not a claim that it has exercised
every branch or that a newer source snapshot has passed an older run.

The argument IDs below refer to sections A01–A15 in
[NUMERICAL_ARGUMENTS.md](NUMERICAL_ARGUMENTS.md). Tests are under `tests/`.
The detailed source-hash and execution receipts belong to the accompanying
release qualification record. Source function names are used as durable
locators because line numbers change during maintenance.

## Representation, conversion, and core arithmetic

| ID and source locator | Limit or policy; consequence | Argument and test entry points | Evidence and remaining obligation |
|---|---|---|---|
| L01 `include/polyphase_float.h`; `object.c:ppf_precision_valid` | Minimum precision 2; unsigned 64-bit precision type, with `SIZE_MAX/2` and limb-size overflow checks. Exponent type is signed 64-bit; permitted context limits are `INT64_MIN/4` through `INT64_MAX/4`. | A01; `test_object`, `test_bounded_conversion` | Representation checks are explicit. A representable precision is not an allocation promise. Exhaustive platform-width coverage remains open. |
| L02 `assignment.c:PPF_TEXT_POWER_LIMIT` | 2000000: bounds selected exact radix powers, output powers/digits, and large parsed-exponent handling. Beyond the parsed-exponent limit, long-double estimates with a guard may classify range; otherwise rejection. | A03; `test_text_bounds`, `test_assignment_conversion`, `test_bounded_conversion` | Radix and `SIZE_MAX` text-length regressions exist. These do **not** test the ±2000000 power boundary. A rigorous platform-specific error bound for the `log2l` estimate and cancellation near that boundary remains open. |
| L03 `assignment.c:PPF_TEXT_FAST_MAX_ATTEMPTS` | 8 fast attempts; 192 guard bits; minimum fast precision 256; one-pass extra digits 16. Decline/fallback is distinct from public rejection. | A03; `test_format`, `test_assignment_conversion` | Exact digit/rounding comparison is the required certificate. Forced eighth-attempt exhaustion and all fast-format variants are not established by these tests. |
| L04 `io.c:PPF_FPIF_MAX_IMPORT_BYTES` | 64 MiB per admitted payload/limb-storage requirement; 4096-byte read chunks are an I/O choice. | A03; `test_fpif_resource_bounds`, `test_io` | Metadata over the bound and short streams are tested without large allocation. Exact equal/below-bound payload completion is not asserted. |
| L05 `arithmetic.c:PPF_INTERVAL_ATTEMPTS` | 24 attempts, 24 guard bits, 64-bit scale increments. Sum-term interval exhaustion can fall back to exact common alignment; square-root internal use has its own failure path. | A01–A02; `test_interval_allocation`, `test_arithmetic_range`, `test_exhaustive_small` | Enclosure finalization and small exact arithmetic have extensive finite evidence. No universal memory bound follows: exact alignment may be much larger than input significands. Forced 24-attempt cases remain open. |
| L06 `arithmetic.c:PPF_ADD_DIRECT_EXTRA_BITS`, `PPF_FMA_DIRECT_EXTRA_BITS`; `remainder.c:PPF_DIRECT_SHIFT_EXTRA` | 4096 for addition/remainder and 8192 for FMA select direct alignment versus another exact/interval route; not public exponent limits. | A02; `test_arithmetic`, `test_arithmetic_variants`, `test_utilities_remainder` | Dispatch argument recorded. Systematic equality/neighbor tests of every threshold and carry path remain open. |
| L07 `roots.c:PPF_ROOT_ATTEMPTS` | 10 attempts per root/reciprocal-root certificate branch; root guard 112, reciprocal-root guard 48; scale grows by 64 bits per retry. Applies to rootn/root/cbrt and reciprocal-square-root routes, plus the mpz hypot fallback. The optimized reciprocal-square-root path can fall back to another bounded branch; 10 is not a whole-call total. `sqrt`/`sqrt_ui` use L05 instead. | A02; `test_roots`, `test_release_boundaries` | Exact floor/remainder argument recorded. Large-precision tests exist; forced tenth-attempt exhaustion is not demonstrated. |
| L08 `roots.c:PPF_RECSQRT_*` | Newton entry 32768 bits; input/correction/work guards 12/8/12, recursion slack 10; parallel entry 4096 limbs and Toom entry 60000 limbs. | A02; `test_roots`, `test_product_planner` | These are dispatch/error-budget choices, not finite root-domain restrictions. Full refinement and truncated-product invariant proof remains open. |
| L09 `divisor_plan.c:PPF_PLAN_*`; `window_product.c:PPF_WINDOW_GUARD_LIMBS` | Reciprocal plan selected for 262144–2097152 bits; quotient extra bits 2; product window guard 2 limbs. | A02; `test_divisor_plan`, `test_product_planner`, `test_rounding_width` | Floor-reciprocal correction argument recorded; carry/sticky sufficiency of every optimized product window needs a complete source-linked proof. |
| L10 `product_planner.c`, `arithmetic.c`, `toom3.c` | Parallel multiplication 1536 limbs; AUTO square Toom3 8192 limbs; reciprocal-square-root profile 32768 bits; Toom parallel evaluation 40000 limbs and short interpolation 100000 limbs. Workers limited by graph (full/rank3/rank5/window). | A02; `test_product_planner`, `test_product_profile`; `PRODUCT_AND_FUSED_PLANNER.md` | Performance dispatch, not convergence caps. Profile/worker boundaries need their own coverage; a benchmark crossover is not a correctness argument. |
| L11 `remainder.c:PPF_QUOTIENT_BITS` | Low quotient result uses `sizeof(long)*CHAR_BIT-1` bits, with a sign; enormous shifts use modular arithmetic. | A02; `test_utilities_remainder`, `test_precision_remainder_failure` | Semantic width and exact modular method, not an iteration limit. Narrow-long platform behavior needs explicit qualification. |

## Shared certificates and elementary functions

| ID and source locator | Limit or policy; consequence | Argument and test entry points | Evidence and remaining obligation |
|---|---|---|---|
| L12 `special_common.c:PPF_SPECIAL_MAX_ATTEMPTS` | 12 attempts, initial guard 96; builder or conversion failure may retry, then numerical rejection. | A01; `test_release_boundaries` (`never_decidable` builder) | Boundary checked: exactly 12 undecided builder calls in all five named directed/nearest modes. This does not force every function-specific exhaustion branch. |
| L13 `special_common.c:ppf_special_iv_to_dyadic` | Each nonzero endpoint shift must fit `ULONG_MAX` and be ≤1000000. Reject null, nonfinite, or reversed intervals. A zero endpoint does not set the common exponent. | A01; focused exact dyadic-boundary probe | Boundary checked at 999999, 1000000, 1000001 for both signs, huge zero-ended intervals, and invalid intervals: 12 cases. Rejection preserves output integers/exponent. No claim about all interval arithmetic helpers follows. |
| L14 `transcendental.c:PPF_TRANS_MAX_ALIGNMENT_BITS` | 1000000 endpoint-alignment bits; also bounds finite `m1` endpoint subtraction against one. Failure retries/rejects. | A01, A04; `test_release_boundaries` | Extreme-input regressions passed. The separate static converter's exact three-point cap boundary and every `m1` fallback branch still need direct boundary coverage. |
| L15 `transcendental.c:PPF_TRANS_MAX_ATTEMPTS` | 12 general attempts, initial guard 80; increasing precision, then rejection. | A01, A04; `test_transcendental`, `test_transcendental_range` | Value/ternary comparisons exist. Forced twelfth-attempt exhaustion is open. |
| L16 `transcendental.c:PPF_TRANS_MAX_STEPS`; reduced exp/log kernels | Term planner capped at 10000000, normally `8*ceil(sqrt(q))+256`; log root count limited to 4096. An unsettled series fails. | A04; `test_transcendental_complete`, `test_transcendental_large` | Geometric-tail argument recorded. Exact term-cap exhaustion and maximum root-count boundary are untested here. The planner is not a convergence proof. |
| L17 `transcendental.c` exact-log recognizers and exact dyadic-root recognition | Exact power recognizers reuse the 10000000 shift/power budget; dyadic noninteger-power recognition has depth ≤4096 before fallback. | A04; `test_transcendental_complete`, `test_transcendental_full` | Algebraic exactness recorded. Tests at recognizer caps and fallback equivalence remain open. |
| L18 `transcendental.c:PPF_TRANS_EXACT_POWER_FACTOR`, `PPF_TRANS_EXACT_POWER_SLACK` | Exact odd-significand power admitted only within a precision-dependent budget based on `8*p+65536`; otherwise general interval power. | A04; `test_transcendental_full`, `test_release_boundaries` | Dispatch bounds large integer construction; it does not bound all downstream work. Exact equality/neighbor cost cases across precisions remain open. |
| L19 `transcendental.c:exp_scaled_constant` | Exact integer `exp2` uses exponent setting directly. Other exact integer exp/m1 shortcuts use `p+64` binary bits or `floor((p+64)/4)` decimal exponents, then fallback. Long-index reduction is admitted before integer construction. | A04; `test_release_boundaries` | Boundary checked at binary 117/118 and decimal 29/30 for `p=53`, both signs, all five modes; compact huge exponents tested with bounded allocation. This is finite evidence, not every precision. |
| L20 `transcendental.c` range preflight; `special_gamma.c` Gamma range preflight | Conservative range comparisons bypass clearly overflowing/tiny exp/exp2/exp10 values; Gamma first obtains a directed log-magnitude interval. Gamma preflight starts only at stored exponent ≥40, with 128-bit work. | A04, A07; `test_release_boundaries`; prior 85-case range-context probe | Inequalities and signed output choices reviewed. Rejected/ambiguous preflight falls through; no claim that every extreme Gamma or `m1` input succeeds. |
| L21 `transcendental_fast.c:PPF_FAST_TRANS_ATTEMPTS` | 5 attempts, guard 48, max scaling shift 10000000; unresolved fast path declines to the general evaluator. | A05; `test_transcendental_large`, `test_transcendental_random` | Source-linked error invariants for every fast variant and forced fifth-attempt/shift boundaries remain open. Fallback does not validate an incorrectly accepted interval. |
| L22 `transcendental_fast.c` series planners | Horner/ball/block4/block8 atanh typically cap `16*ceil(sqrt(q))+1024`; block16 uses `20*ceil(sqrt(q))+2048`; several log/exp kernels use `12*ceil(sqrt(q))+512`; all are capped at 10000000. Exact binary-splitting term planners use separate majorants. | A05; `test_transcendental_large` | Recorded as planner budgets. Per-variant truncation, integer rounding, and midpoint-radius proof obligations remain open. No forced ten-million-term test was run. |
| L23 `transcendental_fast.c` root/bit-burst policies | Ordinary root-count bands 12/26/32/36/40/48; exact log clamps roots to 4096; bit-burst root admission also checks 4096. `PPF_FAST_LOG_BITBURST_ROOTS=1000000` selects the automatic root policy; `PPF_FAST_EXP_BITBURST_HALVINGS=1000000` is declared but unused in this snapshot. Neither is a million-iteration cap. | A05; `LOG_EXP_POWER.md` | Classification verified in source. Override builds need separate evidence; default tests do not cover arbitrary macro overrides. |
| L24 `transcendental_fast.c` optional strategies | Exp bit-burst threshold 768, direct exp2 16384, log bit-burst 75000; AGM/Newton thresholds `UINT64_MAX`, table disabled by default (8 table bits/16 slots); atanh method 0 selects automatic policy, rectangular width 28. Optional AGM budget is `bit_length(q)+12`. | A05; `test_transcendental_large` | Dispatch/storage settings. Optional or disabled algorithms are not certified merely because they compile. Their proof/override qualification remains open. |

## Trigonometric functions and random generation

| ID and source locator | Limit or policy; consequence | Argument and test entry points | Evidence and remaining obligation |
|---|---|---|---|
| L25 `trig_hyperbolic.c:PPF_TRIG_MAX_ATTEMPTS` | 16 general attempts, initial guard 96, then numerical rejection. | A01, A06; `test_trig_hyperbolic`, `test_trig_hyperbolic_special` | General interval identities recorded. Forced sixteenth-attempt exhaustion remains open. |
| L26 `trig_hyperbolic.c:PPF_TRIG_MAX_REDUCTION_EXTRA` | 1000000 extra reduction bits for ordinary finite arguments; exact scaled-angle routes can bypass ordinary reduction. | A06; `test_trig_hyperbolic_special`, `test_trig_hyperbolic_large` | Reduction must retain all possible quadrants. Exact cap-neighbor admission and difficult interval/quadrant boundary cases need a dedicated receipt. |
| L27 `trig_hyperbolic.c:PPF_TRIG_MAX_STEPS`, `PPF_TRIG_FIXED_SCALED_MAX_PREC` | General series capped at 10000000; fixed scaled route selected through precision 12000. | A06; `test_trig_hyperbolic_large`, `test_trig_hyperbolic_random` | Series tail and dispatch are separate obligations. Full cap exhaustion not demonstrated. |
| L28 `trig_fast.c:PPF_FAST_TRIG_MAX_TERMS` and binary-splitting planners | 1000000 maximum terms/term-search iterations; some exact point routes decline above 4096 terms (sin/cos/hyperbolic) or 16384 (atan). Dense admission includes `abits>1024 && 8*abits>q`. | A05–A06; `test_trig_hyperbolic_large` | Default dense-BS experiment is disabled. Active fixed-point/ball variants still need complete source-level error proofs and cap-boundary tests. |
| L29 `trig_fast.c` reduction, guards, block storage | Guard 112, tail guard 40; fixed atan reductions 5; full atan reduction count clamped to [4,64]; target bits 8 and halving bias 4. Blocks of 4/8/16, maximum blocks 36/32, and fixed arrays are algorithm organization. | A05–A06; `test_trig_hyperbolic_large`, `test_trig_hyperbolic_alias` | Error budgets require proof, while block widths are not public argument restrictions. Finite tests do not establish every carry/radius bound. |
| L30 `trig_fast.c:ppf_fast_atan_internal`; `inverse_angle_fast.c:PPF_FAST_ATAN_HALLEY_ATTEMPTS` | Fast atan: 5 attempts. Halley: 4 attempts, minimum precision 12000, guard 192, 96 additional bits per attempt; precision schedule stored in 40 entries. Decline permits fallback. | A05–A06; `test_trig_hyperbolic_large` | Approximate seed is not a certificate. The residual enclosure and final rounding must be proved; full correction/schedule proof and forced exhaustion remain open. |
| L31 `random.c:PPF_RANDOM_MAX_ATTEMPTS`; `prefix_nonzero` | 12 certification attempts, guard 96, extra precision/prefix blocks of 64 bits. A zero prefix permits at most 64 further blocks before failure. | A14; `test_random_runtime` | Seeded tests cover finite outputs. No forced all-zero-prefix/cap exhaustion evidence or complete distribution-with-rejection argument is claimed. |

## Constants and special functions

| ID and source locator | Limit or policy; consequence | Argument and test entry points | Evidence and remaining obligation |
|---|---|---|---|
| L32 `constants.c:PPF_CONSTANT_MAX_ATTEMPTS` | 16 attempts; precision growth includes at least 64 bits. Cache/destination updates are staged. | A01, A08; `test_constants`, `test_constant_allocation_failure` | Cache and allocation evidence is distinct from mathematical enclosure. Forced sixteenth-attempt numerical exhaustion remains open. |
| L33 `constant_pi.c:build_pi_interval` | Up to 10 inner attempts; guard starts 192, minimum 128, doubles; fixed-point interval radius 4; tail admission checked before acceptance. | A08; `test_constants`, `test_independent_special_review` | Radius argument recorded with exact tail check. Full optimized packet-to-source proof and ten-attempt boundary evidence remain open. |
| L34 `constant_log2.c:PPF_LOG2_GUARD_BITS` | Guard 128; exact packet ratio denominator 9. Term count is precision-driven, not a fixed convergence cap. GCD/root-reduction experiments default 0. | A08; `test_constants` | Exact tail argument recorded. Packet arithmetic, parallel merges, and optional variants are separate dependencies. |
| L35 `special_common.c` Euler builder | Harmonic index `clamp(q/2+64,64,20000)`; correction planner `clamp(q/(2*log2(n))+12,8,192)`. At a truncation boundary the omitted correction still defines the hull. | A08; `test_special_constants`, `test_independent_special_review` | Positive-real digamma remainder argument recorded. Cap saturation may leave an undecidable interval; exact planner-boundary coverage remains open. |
| L36 `special_common.c` Catalan builder | `n<1000000`; geometric tail admitted before success. | A08; `test_special_constants` | Ratio is <1/4 for every n≥0, not merely eventually. Series identity remains a cited dependency; forced million-term exhaustion untested. |
| L37 `special_gamma.c` positive log-Gamma and digamma shifts | Log-Gamma target `clamp(q/8+12,16,100000)`; digamma target at least 16, without that target clamp. Both reject after more than 100000 shifts. | A07; `test_special_functions`, `test_special_interval_audit` | Directed recurrence and transformed argument recorded. Exact count boundary and extreme high-precision planner integer conversions remain open. |
| L38 `special_gamma.c` Bernoulli planners | `clamp(q/6+16,12,256)` corrections for log-Gamma/digamma. The first omitted correction supplies the hull even when the planner cap is reached. | A07; `test_special_interval_audit` | Conditional on the positive-real remainder theorem and outward endpoint arithmetic. Cap exhaustion need not mean invalid enclosure, but may prevent certification. |
| L39 `special_gamma.c` exact Gamma/Beta shortcuts | Integer factorial route through 100000; positive integer Beta shortcut when `ai+bi<=100000`. Other admissible inputs use interval evaluation. | A07; `test_special_values`, `test_special_functions` | Exact shortcuts, not universal function-domain bounds. Complete threshold-neighbor coverage remains open. |
| L40 `special_gamma.c:ppf_gamma_inc`, negative recurrence builder | For finite positive x, require `a>=-100000` before converting the recurrence count; builder still checks `ceil(-a)<=100000` and exact shifted scalar representation. | A07; `test_release_boundaries compact` | Boundary checked at−100000/−100001 through allocator admission, all five modes; enormous compact a tested safely. The boundary test deliberately does not execute 100000 recurrence steps. |
| L41 `special_gamma.c` positive incomplete-Gamma series | `n<1000000`; once future ratio<1, a positive geometric tail bounds the lower-gamma series. | A07; `test_special_edge_audit`, `test_special_interval_audit` | Remainder argument recorded; cancellation can force rejection. Million-term exhaustion untested. |
| L42 `special_misc.c` Ei and Li2 reduced series | `k=1; k<1000000`; these predicates mean at most 999999 loop iterations, not a promise of a million terms after initialization. | A09; `test_special_functions`, `test_special_edge_audit` | Positive geometric and alternating-tail obligations recorded. Precise descending-regime and roundoff source proof remains open for all branches. |
| L43 `special_misc.c:li2_interval_recursive` | Reject `depth>12`; transformed interval endpoints must stay directed and within the identity's region. | A09; `test_special_edge_audit`, `test_independent_special_review` | Existing reflected-argument fixes/tests cover selected cases. A complete descent bound and depth 12/13 branch coverage are still open. |
| L44 `special_misc.c` zeta Euler–Maclaurin | For s>1: `N=clamp(q/4+16,16,100000)`, correction planner clamped to [8,128]. | A10; `test_special_functions`, `test_special_large` | The positive-integral first-omitted-term argument is recorded in A10 and locally cross-reviewed. Full source mapping and cap boundaries remain open. |
| L45 `special_misc.c` Hasse zeta | For 0<s<1: `N=min(q+40,1024)`, numerator tail≤2^-N. | A10; `test_special_functions`, `test_special_random` | Integral majorant argument recorded here. The fixed 1024 cap can block high-precision completion. Source mapping, cancellation, and cap-boundary evidence remain open. |
| L46 `special_misc.c:zeta_interval` integer branch | Negative even integers return exact+0 using compact significand/exponent parity; negative odd Bernoulli shortcut requires n≤4095, otherwise another route. | A10; `test_release_boundaries compact` | Boundary/compact-path checked in 51 new cases together with Gamma admission. The Bernoulli 4095 boundary and all reflected large odd arguments remain open. |
| L47 `special_misc.c` erf/erfc series | `n<1000000`; alternating power-series hull; erfc subtracts inside the interval. | A09; `test_special_functions`, `test_special_edge_audit` | Full decreasing-term admission proof and high-cancellation boundary coverage remain open; smallness alone must not be used as a general alternating-series theorem. |
| L48 `special_bessel.c` AGM | At most 128 iterations; return bracket `[geometric.lower, arithmetic.upper]` only after width test. | A11; `test_special_functions`, `test_special_values` | Monotone-bracket argument recorded. Forced 128-iteration exhaustion not demonstrated. |
| L49 `special_bessel.c:ppf_jn`, `ppf_yn` | General finite branch requires unsigned order magnitude≤100000; early special cases precede this check. | A12; `test_special_edge_audit` | Extreme long orders safely rejected in existing tests. Exact 100000/100001 admission pair and successful full-boundary computation are not claimed. |
| L50 `special_bessel.c` J/Y series and order recurrence | J and Y1 use `k<1000000`; Y0 starts k=1. Y higher-order recurrence is bounded by the admitted order. Tail acceptance includes exact ratio tests. | A12; `test_special_edge_audit`, `test_special_interval_audit` | Tail repairs and selected independent rational checks recorded. All term-cap branches and narrow-`unsigned long` coefficient products require further coverage/proof. |
| L51 `special_bessel.c:ppf_ai` and Airy branch series | Exact `abs(x)<=500` admission; each interlaced branch uses `k<1000000` and a symmetric geometric tail. | A12; `test_release_boundaries`, `test_special_interval_audit` | Boundary checked at 500+2^-400 for both signs with allocation admission; selected Ai enclosures tested. This does not prove all points inside the admitted interval finish. |

## Storage and optional embedded algorithms

The following constants must not be mistaken for numerical domains. They are
included so that a search for fixed numbers has an explicit classification.

| ID and source locator | Classification | Obligation |
|---|---|---|
| L52 `context.c`, `ppf_internal.h`, `result_transaction.c` | 64 KiB default arena blocks,16 inline transaction limbs, ownership magic tags: allocation/storage layout. | Bounds, alignment, overflow, rollback, and lifetime tests; no mathematical convergence claim. |
| L53 `conditional_formats.c`, `assignment.c`, `format.c` | Binary/decimal format bit widths, exponent limits, radix range 2–62, character/digit parsing, varargs and format-field limits. | Exact conversion and safe integer arithmetic; optional platform formats require their own build qualification. |
| L54 `polyphase_chudnovsky_core.c` | Embedded standalone algorithm options: guards 80/64, cancellation blocks 64/256, adic blocks 8000, prime band 65535, parallel thresholds12M/20M bits, symbolic subtree4M bits, 64 carry buckets, affine ratio retry 12. | The public pi builder resets policy to the exact non-adic route. Standalone optional adic/symbolic/affine paths require separate proof and allocation qualification; they are not covered by the public-constant claim. Exact selected packet operations remain dependencies of A08. |
| L55 `abi.c`, `runtime_support.c`, `ppf_product_profile_build.h`, transaction/name headers | ABI tags, feature bits, generated profile rule count, wrapper declarations, finite table traversals. | Structural/system behavior. A zero-rule portable product profile is not an assertion about user-generated profiles. |

## Coverage of all production files

The review inventory covers all 47 files under `src/` in this snapshot: 37 C
translation units and 10 headers. Each was scanned for definitions, loop
bounds, comparison guards, retry/shift/reduction limits, and dispatch policies;
relevant numerical branches were read as described above. This inventory
coverage is **not** a line-by-line proof of every file.

| Source file | Limit rows | Classification |
|---|---|---|
| `src/abi.c` | L55 | ABI metadata; no numerical convergence loop. |
| `src/arithmetic.c` | L05–L06, L10 | Exact/interval arithmetic, 24-attempt helpers, alignment dispatch, product dispatch. |
| `src/arithmetic_root_transaction.c` | L52, L55 | Generated-style transactional wrappers; bounded by fixed API argument structure. |
| `src/arithmetic_variants.c` | L05–L06 | Delegation/composition and exact scalar variants; input-length traversal, not a new fixed series cap. |
| `src/assignment.c` | L02–L03, L53 | Text power admission and fast retries; scalar format limits and exact conversion. |
| `src/conditional_formats.c` | L53 | Optional binary/decimal formats; format-defined digits/exponents, platform-dependent surfaces. |
| `src/constant_internal.h` | L32–L34 | Constant interval/cache structures and declarations; no separate convergence cap. |
| `src/constant_log2.c` | L34 | Precision-driven exact packet sum and analytic tail; guard and optional dispatch flags. |
| `src/constant_pi.c` | L33, L54 | Ten inner attempts and exact radius/tail checks; resets embedded core policy. |
| `src/constants.c` | L32 | Sixteen common attempts and staged interval/rounded caches. |
| `src/context.c` | L01, L52 | Exponent validation, arena and allocation bookkeeping; structural capacities. |
| `src/divisor_plan.c` | L09 | Reciprocal-plan thresholds and exact quotient correction. |
| `src/format.c` | L02–L03, L53 | Format scanning, sizes, varargs, delegated digit conversion; no analytic-series cap. |
| `src/inverse_angle_fast.c` | L30 | Four certification attempts, precision schedule, Halley approximation and residual certificate. |
| `src/io.c` | L04 | FPIF metadata admission and bounded read chunks; stream traversal. |
| `src/limb_round.c` | L01 | Bit-width/carry/sticky/range arithmetic; loops traverse represented limbs. |
| `src/object.c` | L01 | Precision and limb-storage admission; copy/resize/validation traversal. |
| `src/polyphase_chudnovsky_core.c` | L33, L54 | Exact packet algorithms plus standalone optional policies; tree/term-driven structural loops. |
| `src/ppf_internal.h` | L01, L52, L55 | Internal representation/helpers, 16 inline transaction limbs, declarations. |
| `src/ppf_openmp_compat.h` | L55 | Serial/OpenMP compatibility definitions; no numerical budget. |
| `src/ppf_product_profile_build.h` | L10, L55 | Generated portable profile identity and zero rule count; dispatch metadata. |
| `src/product_planner.c` | L10 | Performance thresholds, worker graph bounds, profile dispatch. |
| `src/random.c` | L31 | Twelve certificates and 64 extra zero-prefix blocks; prefix width grows by 64 bits. |
| `src/range_flags.c` | L01 | Configured exponent boundary and subnormal/range flag semantics. |
| `src/remainder.c` | L06, L11 | Direct/modular dispatch; quotient-bit return width and exact modular arithmetic. |
| `src/result_transaction.c` | L52 | Inline/heap staged output and commit; limb-size traversal. |
| `src/roots.c` | L07–L08 | Ten attempts, optimized reciprocal-root policies, hypot direct alignment and fallback. |
| `src/round.c` | L01 | Exact quotient/remainder rounding and interval decisions; no fixed analytic iteration budget. |
| `src/runtime_support.c` | L55 | Feature/runtime information and cleanup; no numerical certificate builder. |
| `src/special_bessel.c` | L48–L51 | AGM 128, Bessel order/series limits, Ai 500 and series admission. |
| `src/special_common.c` | L12–L13, L35–L36 | Shared retries/conversion; Euler and Catalan planners and tails. |
| `src/special_gamma.c` | L20, L37–L41 | Gamma/digamma shifts and corrections; exact shortcuts, range preflight, incomplete-Gamma recurrence. |
| `src/special_internal.h` | L12–L13 | Shared special-interval declarations; no separate numerical cap. |
| `src/special_misc.c` | L42–L47 | Ei/Li2/zeta/erf planners, depth/term caps, compact integer classification. |
| `src/toom3.c` | L10 | Exact fixed-rank evaluation/interpolation; parallel and scratch strategy thresholds. |
| `src/transactional_arithmetic_root_names.h` | L55 | Name mapping for wrappers; no numerical loop. |
| `src/transactional_impl_prototypes.h` | L55 | Internal wrapper target prototypes; no numerical loop. |
| `src/transactional_special_names.h` | L55 | Name mapping for wrappers; no numerical loop. |
| `src/transactional_transcendental_names.h` | L55 | Name mapping for wrappers; no numerical loop. |
| `src/transactional_trig_names.h` | L55 | Name mapping for wrappers; no numerical loop. |
| `src/transcendental.c` | L14–L20 | Reference interval builders, retries, step/alignment/exact-power budgets. |
| `src/transcendental_fast.c` | L21–L24 | Fast fixed-point/ball variants and policy overrides; several distinct planners. |
| `src/transcendental_special_transaction.c` | L52, L55 | Staged wrappers and secondary outputs; no independent analytic cap. |
| `src/trig_fast.c` | L28–L30 | Fixed-point/ball trigonometry and atan attempts, reduction/term planners. |
| `src/trig_hyperbolic.c` | L25–L27 | General reduction, series, retry and exact-grid dispatch. |
| `src/utilities.c` | L01–L02 | Exact comparisons, integer/fractional decomposition, selection and neighbors; input-driven loops. |
| `src/window_product.c` | L09 | Product-window carry/sticky and two guard limbs; finite limb traversal. |


The machine-readable review inventory records SHA-256 for every file and the
raw candidate lines used to make this classification. A candidate-line scan
can find a bound; it cannot decide whether a bound is mathematically sound.
Any future production change invalidates the affected source binding and its
dependent review until the change is assessed.

## Work still required for the numerical 1.0 gate

The following obligations remain material rather than editorial:

1. Complete source-linked enclosure/error proofs for active fixed-point,
   midpoint-radius, blocked/binary-split, product-window, and inverse-angle
   correction kernels. Treat enabled overrides separately from defaults.
2. Close the remainder and branch hypotheses listed in A07–A12, including
   zeta source mapping, implicit descending-regime tests, and transformed
   interval cases. An endpoint finalizer cannot repair a wrong builder.
3. Add bounded, forced-exhaustion or instrumented admission tests for each
   open retry/series/recursion cap. Document when a test checks admission
   rather than attempting prohibitively expensive work at the full cap.
4. Establish width-safe planner and coefficient arithmetic on every claimed
   platform. The LP64 evidence does not prove LLP64/32-bit-long behavior;
   products such as series indices squared deserve explicit bounds.
5. Qualify the exact release source after repairs, retain execution receipts,
   and obtain the independent review/reproductions required by the roadmap.
   Local source review and multiple compiler builds do not satisfy an
   external-reproduction requirement.

These open items limit the strength of a universal correctness/completion
claim. They do not negate the finite passing results or the specifically
reviewed admission repairs recorded in the qualification evidence.
