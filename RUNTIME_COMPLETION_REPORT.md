# Runtime and Compatibility Completion

This block closes the final 38 lowercase functions missing from the supplied 349-symbol inventory.

## Added

### Rounding control and certification

- `ppf_can_round`
- `ppf_print_rnd_mode`
- `ppf_round_nearest_away`

`ppf_can_round` constructs the exact error enclosure implied by the source rounding mode and checks whether the target rounding is settled. Its ordinary and faithful decisions are differentially tested against MPFR across all six source and target rounding modes.

`ppf_round_nearest_away` evaluates a supplied operation at one extra bit, then resolves ties away from zero while preserving range and flag behavior.

### Random distributions

- `ppf_urandomb`
- `ppf_urandom`
- `ppf_nrandom`
- `ppf_grandom`
- `ppf_erandom`

The two uniform generators are bitstream-compatible with MPFR for the same GMP random state in the tested configurations. Normal and exponential deviates use independent exact-real sampling with adaptive binary-prefix enclosures and certified final rounding; they are distribution-compatible rather than MPFR-bitstream-compatible.

### Version and build metadata

- `ppf_get_version`
- `ppf_get_patches`
- `ppf_buildopt_tls_p`
- `ppf_buildopt_float128_p`
- `ppf_buildopt_decimal_p`
- `ppf_buildopt_gmpinternals_p`
- `ppf_buildopt_sharedcache_p`
- `ppf_buildopt_tune_case`

### Memory and cache controls

- `ppf_free_cache`
- `ppf_free_cache2`
- `ppf_free_pool`
- `ppf_mp_memory_cleanup`

The controls release thread-local scratch arenas, product-planner scratch, certified constant caches, and transcendental reduction caches according to the requested local/global mask.

### MPF compatibility

- `ppf_set_prec_raw`
- `ppf_eq`
- `ppf_reldiff`
- `ppf_mul_2exp`
- `ppf_div_2exp`

### External/custom storage

- `ppf_custom_get_size`
- `ppf_custom_init`
- `ppf_custom_init_set`
- `ppf_custom_get_kind`
- `ppf_custom_get_significand`
- `ppf_custom_get_exp`
- `ppf_custom_move`

Externally backed objects do not allocate, reallocate, or free the application-owned significand. Operations may overwrite the supplied storage but resizing beyond its declared precision is rejected.

### Conditional formats

- `ppf_set_float128` / `ppf_get_float128`
- `ppf_set_decimal64` / `ppf_get_decimal64`
- `ppf_set_decimal128` / `ppf_get_decimal128`

The baseline supports IEEE binary128 and BID decimal formats when the compiler exposes `_Float128`/`__float128`, `_Decimal64`, and `_Decimal128`. The conversion is exact before the final requested rounding.

## Completion result

- Direct inventory counterparts: **349/349**
- Shared-library exports: **423**
- Additional function-like macro: `ppf_round_nearest_away`
- Production MPFR imports: **0**
- CTest targets: **48/48 passed**

## Honest boundary

This completes the public lowercase API inventory on the supported baseline. It does not by itself establish the same maturity as MPFR across decades of platform tuning. The remaining work is release hardening: licensing, ABI freeze, broader compilers and architectures, continuous fuzzing, external replication, documentation consolidation, and returning to the known performance backlogs.
