# Types and named constants

Include the installed public header to obtain these declarations. The listings below are reference material. The installed header is the authoritative declaration source. These definitions summarize the governed public types. Public layout compatibility does not make direct field mutation an ordinary application interface. Use lifecycle and setter functions unless you are implementing the documented direct-storage protocol.

## Scalar types

```c
#if PPF_HAVE_DECIMAL_FLOAT
__extension__ typedef _Decimal128 ppf_decimal128_t;
#endif
#if PPF_HAVE_DECIMAL_FLOAT
__extension__ typedef _Decimal64 ppf_decimal64_t;
#endif
typedef int64_t ppf_exp_t;
typedef uint32_t ppf_flags_t;
#if PPF_HAVE_FLOAT128
__extension__ typedef __float128 ppf_float128_t;
#endif
typedef uint64_t ppf_prec_t;
```

The optional decimal and 128-bit binary types exist only when their installed-header capability conditions are satisfied. They do not make those C extensions mandatory for the independent core.

## Enumerations

### PPF_ABI_ENDIAN_UNKNOWN

```c
enum {
    PPF_ABI_ENDIAN_UNKNOWN = 0,
    PPF_ABI_ENDIAN_LITTLE = 1,
    PPF_ABI_ENDIAN_BIG = 2
};
```

### PPF_ABI_FEATURE_FLOAT128

```c
enum {
    PPF_ABI_FEATURE_FLOAT128 = 1u << 0,
    PPF_ABI_FEATURE_DECIMAL_FLOAT = 1u << 1,
    PPF_ABI_FEATURE_TLS = 1u << 2,
    PPF_ABI_FEATURE_THREAD_EXIT_CLEANUP = 1u << 3
};
```

### PPF_FLAG_UNDERFLOW

```c
enum {
    PPF_FLAG_UNDERFLOW = 1u << 0,
    PPF_FLAG_OVERFLOW = 1u << 1,
    PPF_FLAG_DIVBY0 = 1u << 2,
    PPF_FLAG_NAN = 1u << 3,
    PPF_FLAG_INEXACT = 1u << 4,
    PPF_FLAG_ERANGE = 1u << 5,
    PPF_FLAG_ALLOC = 1u << 6,
    PPF_FLAGS_ALL = (1u << 7) - 1u
};
```

### PPF_NAN_KIND

```c
enum {
    PPF_NAN_KIND = 0,
    PPF_INF_KIND = 1,
    PPF_ZERO_KIND = 2,
    PPF_REGULAR_KIND = 3
};
```

### PPF_PREC_MIN

```c
enum {
    PPF_PREC_MIN = 2
};
```

### PPF_STORAGE_OWNED

```c
enum {
    PPF_STORAGE_OWNED = 0u,
    PPF_STORAGE_EXTERNAL = 1u << 0
};
```

### ppf_constant_id_t

```c
typedef enum {
    PPF_CONSTANT_PI = 1,
    PPF_CONSTANT_LOG2 = 2,
    PPF_CONSTANT_EULER = 3,
    PPF_CONSTANT_CATALAN = 4
} ppf_constant_id_t;
```

### ppf_free_cache_t

```c
typedef enum {
    PPF_FREE_LOCAL_CACHE = 1u << 0,
    PPF_FREE_GLOBAL_CACHE = 1u << 1
} ppf_free_cache_t;
```

### ppf_interval_status_t

```c
typedef enum {
    PPF_INTERVAL_ERROR = -1,
    PPF_INTERVAL_UNDECIDED = 0,
    PPF_INTERVAL_DECIDED = 1
} ppf_interval_status_t;
```

### ppf_kind_t

```c
typedef enum {
    PPF_KIND_REGULAR = 0,
    PPF_KIND_ZERO = 1,
    PPF_KIND_INF = 2,
    PPF_KIND_NAN = 3
} ppf_kind_t;
```

### ppf_product_algorithm_t

```c
typedef enum {
    PPF_PRODUCT_AUTO = 0,
    PPF_PRODUCT_FULL = 1,
    PPF_PRODUCT_KARATSUBA3 = 2,
    PPF_PRODUCT_TOOM3 = 3,
    PPF_PRODUCT_WINDOW = 4
} ppf_product_algorithm_t;
```

### ppf_product_operation_t

```c
typedef enum {
    PPF_PRODUCT_OP_MULTIPLY = 1,
    PPF_PRODUCT_OP_SQUARE = 2,
    PPF_PRODUCT_OP_FMA = 3,
    PPF_PRODUCT_OP_REC_SQRT = 4
} ppf_product_operation_t;
```

### ppf_rnd_t

```c
typedef enum {
    PPF_RND_DEFAULT = -1,
    PPF_RNDN = 0,
    PPF_RNDZ = 1,
    PPF_RNDU = 2,
    PPF_RNDD = 3,
    PPF_RNDA = 4,
    PPF_RNDF = 5
} ppf_rnd_t;
```

## Precision, range, and ABI constants

Version and digest strings identify a build or generated contract. They are not mathematical parameters and do not attest to the bytes of a loaded binary.

```c
#define PPF_ABI_EPOCH 1
#define PPF_ABI_INFO_VERSION 1
#define PPF_ABI_REVISION 0
#define PPF_ABI_TAG "polyphase-float-abi-1"
#define PPF_ALIASING_CONTRACT_VERSION 1
#define PPF_EMAX_DEFAULT ((ppf_exp_t) 1073741823)
#define PPF_EMAX_MAX ((ppf_exp_t)(INT64_MAX / 4))
#define PPF_EMIN_DEFAULT ((ppf_exp_t)-1073741823)
#define PPF_EMIN_MIN ((ppf_exp_t)(INT64_MIN / 4))
#define PPF_PREC_DEFAULT ((ppf_prec_t)53)
#define PPF_SEMANTIC_CONTRACT_VERSION 1
#define PPF_TERNARY_ERROR INT_MIN
```

## Public layout records

The allocator function-pointer types and advanced custom-storage callbacks are declared in `<polyphase_float.h>`. Keep callback signatures exact. The records below describe ABI shape, not a license to replace initialized objects with raw structure copies.

### ppf_abi_info_t

```c
typedef struct {
    uint32_t struct_size;
    uint32_t info_version;
    uint32_t abi_epoch;
    uint32_t abi_revision;
    uint32_t endian;
    uint32_t pointer_bits;
    uint32_t size_t_bits;
    uint32_t limb_bits;
    uint32_t feature_flags;
    uint32_t reserved32[7];
    uint64_t sizeof_ppf_allocator_t;
    uint64_t alignof_ppf_allocator_t;
    uint64_t sizeof_ppf_arena_mark_t;
    uint64_t alignof_ppf_arena_mark_t;
    uint64_t sizeof_ppf_context_t;
    uint64_t alignof_ppf_context_t;
    uint64_t sizeof_ppf_t;
    uint64_t alignof_ppf_t;
    uint64_t sizeof_ppf_product_plan_t;
    uint64_t alignof_ppf_product_plan_t;
    uint64_t sizeof_ppf_constant_stats_t;
    uint64_t alignof_ppf_constant_stats_t;
    uint64_t sizeof_ppf_abi_info_t;
    uint64_t alignof_ppf_abi_info_t;
    uint64_t offsetof_context_default_precision;
    uint64_t offsetof_context_emin;
    uint64_t offsetof_context_emax;
    uint64_t offsetof_context_default_rounding;
    uint64_t offsetof_context_flags;
    uint64_t offsetof_context_allocator;
    uint64_t offsetof_context_arena_head;
    uint64_t offsetof_context_arena_current;
    uint64_t offsetof_context_arena_block_bytes;
    uint64_t offsetof_number_limbs;
    uint64_t offsetof_number_allocated_limbs;
    uint64_t offsetof_number_used_limbs;
    uint64_t offsetof_number_precision;
    uint64_t offsetof_number_exponent;
    uint64_t offsetof_number_sign;
    uint64_t offsetof_number_kind;
    uint64_t offsetof_number_allocator;
    uint64_t offsetof_number_storage_flags;
    uint64_t reserved64[8];
} ppf_abi_info_t;
```

### ppf_allocator_t

```c
typedef struct {
    ppf_malloc_fn malloc_fn;
    ppf_realloc_fn realloc_fn;
    ppf_free_fn free_fn;
    void *opaque;
} ppf_allocator_t;
```

### ppf_arena_mark_t

```c
typedef struct {
    void *block;
    size_t used;
} ppf_arena_mark_t;
```

### ppf_constant_stats_t

```c
typedef struct {
    ppf_constant_id_t constant_id;
    ppf_prec_t requested_precision;
    ppf_prec_t computed_precision;
    ppf_prec_t cached_precision;
    unsigned long workers;
    unsigned long attempts;
    unsigned long terms;
    unsigned long packets;
    unsigned long guard_bits;
    unsigned long interval_width_bits;
    ppf_exp_t interval_exponent;
    int cache_hit;
    int ternary;
    double compute_seconds;
    double rounding_seconds;
    double total_seconds;
} ppf_constant_stats_t;
```

### ppf_context_t

```c
typedef struct {
    ppf_prec_t default_precision;
    ppf_exp_t emin;
    ppf_exp_t emax;
    ppf_rnd_t default_rounding;
    ppf_flags_t flags;
    ppf_allocator_t allocator;
    void *arena_head;
    void *arena_current;
    size_t arena_block_bytes;
} ppf_context_t;
```

### ppf_product_plan_t

```c
typedef struct {
    ppf_product_algorithm_t algorithm;
    unsigned long requested_workers;
    unsigned long effective_workers;
    int profile_rule_matched;
} ppf_product_plan_t;
```

### ppf_t

```c
typedef struct {
    mp_limb_t *limbs;
    size_t allocated_limbs;
    size_t used_limbs;
    ppf_prec_t precision;
    ppf_exp_t exponent;
    int sign;
    ppf_kind_t kind;
    ppf_allocator_t allocator;
    uint32_t storage_flags;
} ppf_t;
```
