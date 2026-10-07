# GNU MPFR 4.2.2 API and Calculation Inventory

This is an implementation-oriented checklist derived from the GNU MPFR 4.2.2 manual (March 2025). It groups every lowercase `mpfr_*` symbol in the manual's function/type index. The list includes ordinary functions, function-like macros, conditional interfaces, compatibility aliases, experimental entries, and deprecated entries.

## Numerical contract to reproduce

A compatible implementation needs more than matching function names. It needs:

- arbitrary per-object binary precision;
- a sign, normalized arbitrary-precision significand, and exponent;
- signed zero, signed infinity, and one NaN representation;
- exact-operation-then-round semantics at the destination precision;
- rounding modes: nearest/ties-even, toward negative infinity, toward positive infinity, toward zero, away from zero, and faithful rounding;
- a ternary result indicating exact / above exact / below exact;
- exception flags for underflow, overflow, divide-by-zero, NaN, inexact, and range error;
- deterministic special-value, exponent-range, and aliasing behavior;
- optional emulation of subnormals through range handling rather than native MPFR subnormal objects.

## Public data types

`mpfr_t`, `mpfr_ptr`, `mpfr_srcptr`, `mpfr_prec_t`, `mpfr_exp_t`, `mpfr_rnd_t`, `mpfr_flags_t`, `mpfr_float128`

## Function and function-like symbol inventory

### Initialization and precision (10)

`mpfr_init2`, `mpfr_inits2`, `mpfr_clear`, `mpfr_clears`, `mpfr_init`, `mpfr_inits`

`mpfr_set_default_prec`, `mpfr_get_default_prec`, `mpfr_set_prec`, `mpfr_get_prec`


### Assignment and parsing (25)

`mpfr_set`, `mpfr_set_ui`, `mpfr_set_si`, `mpfr_set_uj`, `mpfr_set_sj`, `mpfr_set_flt`

`mpfr_set_d`, `mpfr_set_ld`, `mpfr_set_float128`, `mpfr_set_decimal64`, `mpfr_set_decimal128`, `mpfr_set_z`

`mpfr_set_q`, `mpfr_set_f`, `mpfr_set_ui_2exp`, `mpfr_set_si_2exp`, `mpfr_set_uj_2exp`, `mpfr_set_sj_2exp`

`mpfr_set_z_2exp`, `mpfr_set_str`, `mpfr_strtofr`, `mpfr_set_nan`, `mpfr_set_inf`, `mpfr_set_zero`

`mpfr_swap`


### Combined initialization and assignment (9)

`mpfr_init_set`, `mpfr_init_set_ui`, `mpfr_init_set_si`, `mpfr_init_set_d`, `mpfr_init_set_ld`, `mpfr_init_set_z`

`mpfr_init_set_q`, `mpfr_init_set_f`, `mpfr_init_set_str`


### Conversion and fit tests (28)

`mpfr_get_flt`, `mpfr_get_d`, `mpfr_get_ld`, `mpfr_get_float128`, `mpfr_get_decimal64`, `mpfr_get_decimal128`

`mpfr_get_si`, `mpfr_get_ui`, `mpfr_get_sj`, `mpfr_get_uj`, `mpfr_get_d_2exp`, `mpfr_get_ld_2exp`

`mpfr_frexp`, `mpfr_get_z_2exp`, `mpfr_get_z`, `mpfr_get_q`, `mpfr_get_f`, `mpfr_get_str_ndigits`

`mpfr_get_str`, `mpfr_free_str`, `mpfr_fits_ulong_p`, `mpfr_fits_slong_p`, `mpfr_fits_uint_p`, `mpfr_fits_sint_p`

`mpfr_fits_ushort_p`, `mpfr_fits_sshort_p`, `mpfr_fits_uintmax_p`, `mpfr_fits_intmax_p`


### Arithmetic (54)

`mpfr_add`, `mpfr_add_ui`, `mpfr_add_si`, `mpfr_add_d`, `mpfr_add_z`, `mpfr_add_q`

`mpfr_sub`, `mpfr_ui_sub`, `mpfr_sub_ui`, `mpfr_si_sub`, `mpfr_sub_si`, `mpfr_d_sub`

`mpfr_sub_d`, `mpfr_z_sub`, `mpfr_sub_z`, `mpfr_sub_q`, `mpfr_mul`, `mpfr_mul_ui`

`mpfr_mul_si`, `mpfr_mul_d`, `mpfr_mul_z`, `mpfr_mul_q`, `mpfr_sqr`, `mpfr_div`

`mpfr_ui_div`, `mpfr_div_ui`, `mpfr_si_div`, `mpfr_div_si`, `mpfr_d_div`, `mpfr_div_d`

`mpfr_div_z`, `mpfr_div_q`, `mpfr_sqrt`, `mpfr_sqrt_ui`, `mpfr_rec_sqrt`, `mpfr_cbrt`

`mpfr_rootn_ui`, `mpfr_rootn_si`, `mpfr_root`, `mpfr_neg`, `mpfr_abs`, `mpfr_dim`

`mpfr_mul_2ui`, `mpfr_mul_2si`, `mpfr_div_2ui`, `mpfr_div_2si`, `mpfr_fac_ui`, `mpfr_fma`

`mpfr_fms`, `mpfr_fmma`, `mpfr_fmms`, `mpfr_hypot`, `mpfr_sum`, `mpfr_dot`


### Comparison and classification (26)

`mpfr_cmp`, `mpfr_cmp_ui`, `mpfr_cmp_si`, `mpfr_cmp_d`, `mpfr_cmp_ld`, `mpfr_cmp_z`

`mpfr_cmp_q`, `mpfr_cmp_f`, `mpfr_cmp_ui_2exp`, `mpfr_cmp_si_2exp`, `mpfr_cmpabs`, `mpfr_cmpabs_ui`

`mpfr_nan_p`, `mpfr_inf_p`, `mpfr_number_p`, `mpfr_zero_p`, `mpfr_regular_p`, `mpfr_sgn`

`mpfr_greater_p`, `mpfr_greaterequal_p`, `mpfr_less_p`, `mpfr_lessequal_p`, `mpfr_equal_p`, `mpfr_lessgreater_p`

`mpfr_unordered_p`, `mpfr_total_order_p`


### Logarithms, exponentials and powers (24)

`mpfr_log`, `mpfr_log_ui`, `mpfr_log2`, `mpfr_log10`, `mpfr_log1p`, `mpfr_log2p1`

`mpfr_log10p1`, `mpfr_exp`, `mpfr_exp2`, `mpfr_exp10`, `mpfr_expm1`, `mpfr_exp2m1`

`mpfr_exp10m1`, `mpfr_pow`, `mpfr_powr`, `mpfr_pow_ui`, `mpfr_pow_si`, `mpfr_pow_uj`

`mpfr_pow_sj`, `mpfr_pown`, `mpfr_pow_z`, `mpfr_ui_pow_ui`, `mpfr_ui_pow`, `mpfr_compound_si`


### Trigonometric functions (25)

`mpfr_cos`, `mpfr_sin`, `mpfr_tan`, `mpfr_cosu`, `mpfr_sinu`, `mpfr_tanu`

`mpfr_cospi`, `mpfr_sinpi`, `mpfr_tanpi`, `mpfr_sin_cos`, `mpfr_sec`, `mpfr_csc`

`mpfr_cot`, `mpfr_acos`, `mpfr_asin`, `mpfr_atan`, `mpfr_acosu`, `mpfr_asinu`

`mpfr_atanu`, `mpfr_acospi`, `mpfr_asinpi`, `mpfr_atanpi`, `mpfr_atan2`, `mpfr_atan2u`

`mpfr_atan2pi`


### Hyperbolic functions (10)

`mpfr_cosh`, `mpfr_sinh`, `mpfr_tanh`, `mpfr_sinh_cosh`, `mpfr_sech`, `mpfr_csch`

`mpfr_coth`, `mpfr_acosh`, `mpfr_asinh`, `mpfr_atanh`


### Special functions and constants (24)

`mpfr_eint`, `mpfr_li2`, `mpfr_gamma`, `mpfr_gamma_inc`, `mpfr_lngamma`, `mpfr_lgamma`

`mpfr_digamma`, `mpfr_beta`, `mpfr_zeta`, `mpfr_zeta_ui`, `mpfr_erf`, `mpfr_erfc`

`mpfr_j0`, `mpfr_j1`, `mpfr_jn`, `mpfr_y0`, `mpfr_y1`, `mpfr_yn`

`mpfr_agm`, `mpfr_ai`, `mpfr_const_log2`, `mpfr_const_pi`, `mpfr_const_euler`, `mpfr_const_catalan`


### Stream and raw I/O (5)

`mpfr_out_str`, `mpfr_inp_str`, `mpfr_fpif_export`, `mpfr_fpif_import`, `mpfr_dump`


### Formatted output (10)

`mpfr_fprintf`, `mpfr_vfprintf`, `mpfr_printf`, `mpfr_vprintf`, `mpfr_sprintf`, `mpfr_vsprintf`

`mpfr_snprintf`, `mpfr_vsnprintf`, `mpfr_asprintf`, `mpfr_vasprintf`


### Integer rounding and remainder (19)

`mpfr_rint`, `mpfr_ceil`, `mpfr_floor`, `mpfr_round`, `mpfr_roundeven`, `mpfr_trunc`

`mpfr_rint_ceil`, `mpfr_rint_floor`, `mpfr_rint_round`, `mpfr_rint_roundeven`, `mpfr_rint_trunc`, `mpfr_frac`

`mpfr_modf`, `mpfr_fmod`, `mpfr_fmod_ui`, `mpfr_fmodquo`, `mpfr_remainder`, `mpfr_remquo`

`mpfr_integer_p`


### Rounding control and certification (7)

`mpfr_set_default_rounding_mode`, `mpfr_get_default_rounding_mode`, `mpfr_prec_round`, `mpfr_can_round`, `mpfr_min_prec`, `mpfr_print_rnd_mode`

`mpfr_round_nearest_away`


### Neighbors, min/max, random, exponent/sign and build information (23)

`mpfr_nexttoward`, `mpfr_nextabove`, `mpfr_nextbelow`, `mpfr_min`, `mpfr_max`, `mpfr_urandomb`

`mpfr_urandom`, `mpfr_nrandom`, `mpfr_grandom`, `mpfr_erandom`, `mpfr_get_exp`, `mpfr_set_exp`

`mpfr_signbit`, `mpfr_setsign`, `mpfr_copysign`, `mpfr_get_version`, `mpfr_get_patches`, `mpfr_buildopt_tls_p`

`mpfr_buildopt_float128_p`, `mpfr_buildopt_decimal_p`, `mpfr_buildopt_gmpinternals_p`, `mpfr_buildopt_sharedcache_p`, `mpfr_buildopt_tune_case`


### Exponent range and exception flags (34)

`mpfr_get_emin`, `mpfr_get_emax`, `mpfr_set_emin`, `mpfr_set_emax`, `mpfr_get_emin_min`, `mpfr_get_emin_max`

`mpfr_get_emax_min`, `mpfr_get_emax_max`, `mpfr_check_range`, `mpfr_subnormalize`, `mpfr_clear_underflow`, `mpfr_clear_overflow`

`mpfr_clear_divby0`, `mpfr_clear_nanflag`, `mpfr_clear_inexflag`, `mpfr_clear_erangeflag`, `mpfr_clear_flags`, `mpfr_set_underflow`

`mpfr_set_overflow`, `mpfr_set_divby0`, `mpfr_set_nanflag`, `mpfr_set_inexflag`, `mpfr_set_erangeflag`, `mpfr_underflow_p`

`mpfr_overflow_p`, `mpfr_divby0_p`, `mpfr_nanflag_p`, `mpfr_inexflag_p`, `mpfr_erangeflag_p`, `mpfr_flags_clear`

`mpfr_flags_set`, `mpfr_flags_test`, `mpfr_flags_save`, `mpfr_flags_restore`


### Memory and caches (4)

`mpfr_free_cache`, `mpfr_free_cache2`, `mpfr_free_pool`, `mpfr_mp_memory_cleanup`


### MPF compatibility (5)

`mpfr_set_prec_raw`, `mpfr_eq`, `mpfr_reldiff`, `mpfr_mul_2exp`, `mpfr_div_2exp`


### Custom storage interface (7)

`mpfr_custom_get_size`, `mpfr_custom_init`, `mpfr_custom_init_set`, `mpfr_custom_get_kind`, `mpfr_custom_get_significand`, `mpfr_custom_get_exp`

`mpfr_custom_move`


## Additional uppercase macros and enumerators

### Initialization/version macros

`MPFR_DECL_INIT`, `MPFR_VERSION`, `MPFR_VERSION_MAJOR`, `MPFR_VERSION_MINOR`, `MPFR_VERSION_PATCHLEVEL`, `MPFR_VERSION_STRING`, `MPFR_VERSION_NUM`

### Rounding modes

`MPFR_RNDN`, `MPFR_RNDD`, `MPFR_RNDU`, `MPFR_RNDZ`, `MPFR_RNDA`, `MPFR_RNDF`

### Exception masks

`MPFR_FLAGS_UNDERFLOW`, `MPFR_FLAGS_OVERFLOW`, `MPFR_FLAGS_DIVBY0`, `MPFR_FLAGS_NAN`, `MPFR_FLAGS_INEXACT`, `MPFR_FLAGS_ERANGE`, `MPFR_FLAGS_ALL`

### Custom-value kinds and cache controls

`MPFR_NAN_KIND`, `MPFR_INF_KIND`, `MPFR_ZERO_KIND`, `MPFR_REGULAR_KIND`, `MPFR_FREE_LOCAL_CACHE`, `MPFR_FREE_GLOBAL_CACHE`

## Recommended implementation order

1. Limb primitives, allocation, overlap-safe copying, normalization, and bit shifts.
2. Object model, precision/exponent metadata, signed special values, and classification.
3. One central rounding engine with guard/round/sticky information, carry propagation, exponent checks, ternary direction, and flags.
4. Assignment, conversion, comparison, addition/subtraction, multiplication/square, division, and power-of-two scaling.
5. Integer rounding, remainder operations, next-number operations, range checking, and subnormal emulation.
6. Square root, reciprocal square root, fused operations, exact sum, and dot product.
7. Logarithms, exponentials, and powers with a precision-escalation/correct-rounding loop.
8. Trigonometric and hyperbolic functions with rigorous argument reduction.
9. Gamma/zeta/error/Bessel/Airy and the remaining special functions.
10. Text conversion, formatted output, random distributions, caches, custom storage, and compatibility wrappers.

## Narrow high-performance subset for large-precision pi code

For Chudnovsky or related binary-splitting code, a practical first target is:

`mpfr_init2`, `mpfr_clear`, `mpfr_set_ui`, `mpfr_set_si`, `mpfr_set_z`, `mpfr_set_str`, `mpfr_get_str`, `mpfr_get_z`, `mpfr_cmp`, `mpfr_add`, `mpfr_sub`, `mpfr_mul`, `mpfr_sqr`, `mpfr_div`, `mpfr_sqrt`, `mpfr_mul_2ui`, `mpfr_div_2ui`, `mpfr_prec_round`, plus `MPFR_RNDN` and the inexact/range machinery.

That subset is not full MPFR compatibility, but it covers the operations normally on the critical path of arbitrary-precision pi evaluation and decimal export.
