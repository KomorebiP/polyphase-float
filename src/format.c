/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "ppf_internal.h"

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>

#if defined(EOVERFLOW)
#define PPF_SET_OVERFLOW_ERRNO() (errno = EOVERFLOW)
#else
#define PPF_SET_OVERFLOW_ERRNO() ((void)0)
#endif

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
    int failed;
    ppf_context_t *ctx;
} byte_builder_t;

static void builder_init(byte_builder_t *b, ppf_context_t *ctx) {
    memset(b, 0, sizeof(*b));
    b->ctx = ppf_resolve_context(ctx);
}

static void builder_clear(byte_builder_t *b) {
    ppf_context_t *ctx;
    if (!b) return;
    ctx = b->ctx;
    ppf_context_free_internal(ctx, b->data, b->capacity);
    memset(b, 0, sizeof(*b));
}

static int builder_reserve(byte_builder_t *b, size_t extra) {
    size_t need, cap;
    unsigned char *tmp;
    if (!b || !b->ctx || b->failed) return -1;
    if (extra > SIZE_MAX - b->length - 1u) {
        b->failed = 1;
        ppf_raise(b->ctx, PPF_FLAG_ERANGE);
        return -1;
    }
    need = b->length + extra + 1u;
    if (need <= b->capacity) return 0;
    cap = b->capacity ? b->capacity : 128u;
    while (cap < need) {
        size_t next = cap <= SIZE_MAX / 2u ? cap * 2u : need;
        if (next <= cap) { cap = need; break; }
        cap = next;
    }
    tmp = (unsigned char *)ppf_context_realloc_internal(
        b->ctx, b->data, b->capacity, cap);
    if (!tmp) { b->failed = 1; return -1; }
    b->data = tmp;
    b->capacity = cap;
    return 0;
}

static int builder_append(byte_builder_t *b, const void *data, size_t size) {
    if (builder_reserve(b, size) != 0) return -1;
    if (size) memcpy(b->data + b->length, data, size);
    b->length += size;
    b->data[b->length] = 0;
    return 0;
}

static int builder_append_char(byte_builder_t *b, unsigned char ch) {
    return builder_append(b, &ch, 1u);
}

static int builder_append_repeat(byte_builder_t *b, unsigned char ch,
                                 size_t count) {
    if (builder_reserve(b, count) != 0) return -1;
    memset(b->data + b->length, ch, count);
    b->length += count;
    b->data[b->length] = 0;
    return 0;
}

static char *context_strdup(ppf_context_t *ctx, const char *text) {
    size_t bytes;
    char *copy;
    if (!ctx || !text) return NULL;
    bytes = strlen(text) + 1u;
    copy = (char *)ppf_context_malloc_internal(ctx, bytes);
    if (copy) memcpy(copy, text, bytes);
    return copy;
}

static void context_string_free(ppf_context_t *ctx, char *text) {
    size_t bytes;
    if (!text) return;
    bytes = strlen(text) + 1u;
    ppf_context_free_internal(ctx, text, bytes);
}

typedef enum {
    LEN_NONE = 0, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_J, LEN_Z, LEN_T, LEN_CAP_L,
    LEN_GMP_F, LEN_GMP_Q, LEN_GMP_M, LEN_GMP_N, LEN_GMP_Z,
    LEN_PPF_P, LEN_PPF_R
} length_kind_t;

typedef struct {
    int alt;
    int zero;
    int left;
    int plus;
    int space;
    int group;
    int width_set;
    int width;
    int precision_set;
    int precision;
    length_kind_t length;
    ppf_rnd_t rounding;
    char conversion;
} format_spec_t;

static int append_decimal_int(char *out, size_t cap, size_t *used, int value) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%d", value);
    if (n < 0 || (size_t)n >= sizeof(tmp) || *used > cap - (size_t)n - 1u)
        return -1;
    memcpy(out + *used, tmp, (size_t)n);
    *used += (size_t)n;
    out[*used] = 0;
    return 0;
}

static int build_native_spec(char out[128], const format_spec_t *s,
                             length_kind_t override_length) {
    size_t u = 0;
    length_kind_t len = override_length == LEN_NONE ? s->length : override_length;
    out[u++] = '%';
    if (s->alt) out[u++] = '#';
    if (s->zero && !s->left) out[u++] = '0';
    if (s->left) out[u++] = '-';
    if (s->plus) out[u++] = '+';
    if (s->space && !s->plus) out[u++] = ' ';
    if (s->group) out[u++] = '\'';
    out[u] = 0;
    if (s->width_set && append_decimal_int(out, 128, &u, s->width) != 0)
        return -1;
    if (s->precision_set) {
        if (u + 1u >= 128) return -1;
        out[u++] = '.'; out[u] = 0;
        if (append_decimal_int(out, 128, &u, s->precision) != 0) return -1;
    }
    switch (len) {
        case LEN_HH: if (u + 2 >= 128) return -1; out[u++]='h';out[u++]='h';break;
        case LEN_H: if (u + 1 >= 128) return -1; out[u++]='h';break;
        case LEN_L: if (u + 1 >= 128) return -1; out[u++]='l';break;
        case LEN_LL: if (u + 2 >= 128) return -1; out[u++]='l';out[u++]='l';break;
        case LEN_J: if (u + 1 >= 128) return -1; out[u++]='j';break;
        case LEN_Z: if (u + 1 >= 128) return -1; out[u++]='z';break;
        case LEN_T: if (u + 1 >= 128) return -1; out[u++]='t';break;
        case LEN_CAP_L: if (u + 1 >= 128) return -1; out[u++]='L';break;
        case LEN_GMP_F: if (u + 1 >= 128) return -1; out[u++]='F';break;
        case LEN_GMP_Q: if (u + 1 >= 128) return -1; out[u++]='Q';break;
        case LEN_GMP_M: if (u + 1 >= 128) return -1; out[u++]='M';break;
        case LEN_GMP_N: if (u + 1 >= 128) return -1; out[u++]='N';break;
        case LEN_GMP_Z: if (u + 1 >= 128) return -1; out[u++]='Z';break;
        default: break;
    }
    if (u + 1 >= 128) return -1;
    out[u++] = s->conversion;
    out[u] = 0;
    return 0;
}

static int parse_nonnegative_int(const char **cursor, int *value) {
    const char *p = *cursor;
    unsigned long long v = 0;
    int any = 0;
    while (*p >= '0' && *p <= '9') {
        any = 1;
        v = v * 10u + (unsigned)(*p - '0');
        if (v > INT_MAX) return -1;
        ++p;
    }
    if (!any) return 0;
    *value = (int)v;
    *cursor = p;
    return 1;
}

static int parse_spec(const char **cursor, format_spec_t *s, va_list *ap) {
    const char *p = *cursor;
    memset(s, 0, sizeof(*s));
    s->rounding = PPF_RNDN;
    for (;;) {
        if (*p == '#') s->alt = 1;
        else if (*p == '0') s->zero = 1;
        else if (*p == '-') s->left = 1;
        else if (*p == '+') s->plus = 1;
        else if (*p == ' ') s->space = 1;
        else if (*p == '\'') s->group = 1;
        else break;
        ++p;
    }
    if (*p == '*') {
        int w = va_arg(*ap, int);
        ++p;
        s->width_set = 1;
        /* Its positive magnitude cannot fit the supported int width. */
        if (w == INT_MIN) {
            ppf_set_erangeflag();
            PPF_SET_OVERFLOW_ERRNO();
            return -1;
        }
        if (w < 0) { s->left = 1; s->zero = 0; s->width = -w; }
        else s->width = w;
    } else {
        int rc = parse_nonnegative_int(&p, &s->width);
        if (rc < 0) return -1;
        s->width_set = rc > 0;
    }
    if (*p == '.') {
        ++p;
        s->precision_set = 1;
        if (*p == '*') {
            int q = va_arg(*ap, int);
            ++p;
            if (q < 0) s->precision_set = 0;
            else s->precision = q;
        } else {
            int rc = parse_nonnegative_int(&p, &s->precision);
            if (rc < 0) return -1;
            if (rc == 0) s->precision = 0;
        }
    }
    if (p[0] == 'h' && p[1] == 'h') { s->length = LEN_HH; p += 2; }
    else if (p[0] == 'l' && p[1] == 'l') { s->length = LEN_LL; p += 2; }
    else {
        switch (*p) {
            case 'h': s->length=LEN_H; ++p; break;
            case 'l': s->length=LEN_L; ++p; break;
            case 'j': s->length=LEN_J; ++p; break;
            case 'z': s->length=LEN_Z; ++p; break;
            case 't': s->length=LEN_T; ++p; break;
            case 'L': s->length=LEN_CAP_L; ++p; break;
            case 'F':
                if (p[1] && strchr("aAeEfFgG", p[1])) {
                    s->length=LEN_GMP_F; ++p;
                }
                break;
            case 'Q': s->length=LEN_GMP_Q; ++p; break;
            case 'M': s->length=LEN_GMP_M; ++p; break;
            case 'N': s->length=LEN_GMP_N; ++p; break;
            case 'Z': s->length=LEN_GMP_Z; ++p; break;
            case 'P': s->length=LEN_PPF_P; ++p; break;
            case 'R':
                s->length=LEN_PPF_R; ++p;
                switch (*p) {
                    case 'U': s->rounding=PPF_RNDU; ++p; break;
                    case 'D': s->rounding=PPF_RNDD; ++p; break;
                    case 'Y': s->rounding=PPF_RNDA; ++p; break;
                    case 'Z': s->rounding=PPF_RNDZ; ++p; break;
                    case 'N': s->rounding=PPF_RNDN; ++p; break;
                    case '*': s->rounding=(ppf_rnd_t)va_arg(*ap,int); ++p; break;
                    default: break;
                }
                if (!ppf_rounding_valid(s->rounding)) return -1;
                break;
            default: break;
        }
    }
    if (*p == '\0' || *p == '$') return -1;
    s->conversion = *p++;
    *cursor = p;
    return 0;
}

static const char *decimal_point_string(void) {
    const char *p = localeconv()->decimal_point;
    return (p && *p) ? p : ".";
}

static int numeric_sign(const ppf_t *x) {
    return x && x->sign < 0;
}

static int append_exponent(byte_builder_t *b, char letter,
                           long long exponent, int min_digits) {
    char digits[64];
    unsigned long long magnitude;
    int n;
    char sign = '+';
    if (exponent < 0) {
        sign = '-';
        magnitude = (unsigned long long)(-(exponent + 1)) + 1u;
    } else magnitude = (unsigned long long)exponent;
    n = snprintf(digits, sizeof(digits), "%llu", magnitude);
    if (n < 0 || (size_t)n >= sizeof(digits)) return -1;
    if (builder_append_char(b, (unsigned char)letter) != 0 ||
        builder_append_char(b, (unsigned char)sign) != 0 ||
        ((int)n < min_digits && builder_append_repeat(
             b, '0', (size_t)(min_digits - n)) != 0) ||
        builder_append(b, digits, (size_t)n) != 0)
        return -1;
    return 0;
}

static int apply_float_width(byte_builder_t *out, const byte_builder_t *body,
                             const format_spec_t *s, int negative,
                             int special, size_t base_prefix) {
    unsigned char sign = 0;
    size_t full, padding = 0, sign_len = 0;
    if (negative) sign = '-';
    else if (s->plus) sign = '+';
    else if (s->space) sign = ' ';
    if (sign) sign_len = 1;
    if (body->length > SIZE_MAX - sign_len) return -1;
    full = body->length + sign_len;
    if (s->width_set && s->width > 0 && (size_t)s->width > full)
        padding = (size_t)s->width - full;
    if (s->left) {
        if (sign && builder_append_char(out, sign) != 0) return -1;
        if (builder_append(out, body->data, body->length) != 0) return -1;
        return builder_append_repeat(out, ' ', padding);
    }
    if (s->zero && !special) {
        if (sign && builder_append_char(out, sign) != 0) return -1;
        if (base_prefix > body->length) return -1;
        if (base_prefix && builder_append(out, body->data, base_prefix) != 0)
            return -1;
        if (builder_append_repeat(out, '0', padding) != 0) return -1;
        return builder_append(out, body->data + base_prefix,
                              body->length - base_prefix);
    }
    if (builder_append_repeat(out, ' ', padding) != 0) return -1;
    if (sign && builder_append_char(out, sign) != 0) return -1;
    return builder_append(out, body->data, body->length);
}

static void trim_fractional_zeros(byte_builder_t *b, size_t point_pos,
                                  size_t exponent_pos, int keep_point) {
    size_t end = exponent_pos;
    while (end > point_pos + 1u && b->data[end - 1u] == '0') --end;
    if (end == point_pos + 1u && !keep_point) --end;
    if (end != exponent_pos) {
        memmove(b->data + end, b->data + exponent_pos,
                b->length - exponent_pos);
        b->length -= exponent_pos - end;
        b->data[b->length] = 0;
    }
}

static int format_special_body(byte_builder_t *body, const ppf_t *x,
                               char conversion) {
    int upper = conversion >= 'A' && conversion <= 'Z';
    const char *word = ppf_nan_p(x) ? (upper ? "NAN" : "nan")
                                    : (upper ? "INF" : "inf");
    return builder_append(body, word, 3u);
}

static int get_unsigned_digits(char **digits_out, ppf_exp_t *exp_out,
                               int base, size_t digits, const ppf_t *x,
                               ppf_rnd_t rnd, ppf_context_t *ctx) {
    char *raw = ppf_get_str(NULL, exp_out, base, digits, x, rnd, ctx);
    char *copy;
    const char *body;
    if (!raw) return -1;
    body = raw[0] == '-' ? raw + 1 : raw;
    copy = context_strdup(ctx, body);
    ppf_free_str(raw);
    if (!copy) return -1;
    *digits_out = copy;
    return 0;
}

static int format_scientific_body(byte_builder_t *body, const ppf_t *x,
                                  const format_spec_t *s, ppf_context_t *ctx) {
    size_t after = s->precision_set ? (size_t)s->precision :
                   ppf_get_str_ndigits(10, x->precision) - 1u;
    size_t total;
    char *digits = NULL;
    ppf_exp_t exponent;
    const char *point = decimal_point_string();
    char letter = s->conversion;
    if (after > SIZE_MAX - 1u) return -1;
    if (ppf_zero_p(x)) {
        if (builder_append_char(body, '0') != 0 ||
            ((after || s->alt) &&
             builder_append(body, point, strlen(point)) != 0) ||
            (after && builder_append_repeat(body, '0', after) != 0) ||
            append_exponent(body, letter, 0, 2) != 0)
            return -1;
        return 0;
    }
    total = after + 1u;
    if (get_unsigned_digits(&digits, &exponent, 10, total, x,
                            s->rounding, ctx) != 0)
        return -1;
    if (builder_append(body, digits, 1u) != 0 ||
        ((after || s->alt) && builder_append(body, point, strlen(point)) != 0) ||
        (after && builder_append(body, digits + 1, after) != 0) ||
        append_exponent(body, letter, (long long)exponent - 1, 2) != 0) {
        context_string_free(ctx, digits); return -1;
    }
    context_string_free(ctx, digits);
    return 0;
}

static int round_scaled_decimal_integer(mpz_ptr rounded, const ppf_t *x,
                                        size_t fractional_digits,
                                        ppf_rnd_t rnd, ppf_context_t *ctx) {
    mpq_t q;
    mpz_t magnitude, denominator, remainder, scale;
    int sign, increment = 0, cmp = 0;
    if (!x || !rounded || !ppf_number_p(x)) return -1;
    if (ppf_zero_p(x)) { mpz_set_ui(rounded, 0); return 0; }
    if (!ppf_regular_p(x) || fractional_digits > ULONG_MAX) return -1;
    mpq_init(q); mpz_inits(magnitude, denominator, remainder, scale, NULL);
    ppf_get_q(q, x, ctx);
    sign = mpz_sgn(mpq_numref(q)) < 0 ? -1 : 1;
    mpz_abs(magnitude, mpq_numref(q));
    mpz_set(denominator, mpq_denref(q));
    mpz_ui_pow_ui(scale, 10u, (unsigned long)fractional_digits);
    mpz_mul(magnitude, magnitude, scale);
    mpz_fdiv_qr(rounded, remainder, magnitude, denominator);
    if (mpz_sgn(remainder) != 0) {
        switch (rnd) {
            case PPF_RNDZ: increment = 0; break;
            case PPF_RNDA: increment = 1; break;
            case PPF_RNDU: increment = sign > 0; break;
            case PPF_RNDD: increment = sign < 0; break;
            case PPF_RNDF: increment = 0; break;
            case PPF_RNDN:
            default:
                mpz_mul_2exp(remainder, remainder, 1u);
                cmp = mpz_cmp(remainder, denominator);
                increment = cmp > 0 || (cmp == 0 && mpz_odd_p(rounded));
                break;
        }
        ppf_raise(ctx, PPF_FLAG_INEXACT);
    }
    if (increment) mpz_add_ui(rounded, rounded, 1u);
    if (sign < 0) mpz_neg(rounded, rounded);
    mpz_clears(magnitude, denominator, remainder, scale, NULL);
    mpq_clear(q);
    return 0;
}

static int format_fixed_body(byte_builder_t *body, const ppf_t *x,
                             const format_spec_t *s, ppf_context_t *ctx) {
    size_t after = s->precision_set ? (size_t)s->precision : 6u;
    mpz_t scaled;
    char *text = NULL;
    const char *digits;
    size_t len, point_pos;
    const char *point = decimal_point_string();
    int negative;
    mpz_init(scaled);
    if (round_scaled_decimal_integer(scaled, x, after, s->rounding, ctx) != 0) {
        mpz_clear(scaled); return -1;
    }
    negative = mpz_sgn(scaled) < 0;
    mpz_abs(scaled, scaled);
    text = mpz_get_str(NULL, 10, scaled);
    if (!text) {
        ppf_set_allocflag();
        mpz_clear(scaled);
        return -1;
    }
    digits = text;
    len = strlen(digits);
    if (after == 0) {
        if (builder_append(body, digits, len) != 0 ||
            (s->alt && builder_append(body, point, strlen(point)) != 0)) {
            free(text); mpz_clear(scaled); return -1;
        }
    } else if (len <= after) {
        if (builder_append_char(body, '0') != 0 ||
            builder_append(body, point, strlen(point)) != 0 ||
            builder_append_repeat(body, '0', after - len) != 0 ||
            builder_append(body, digits, len) != 0) {
            free(text); mpz_clear(scaled); return -1;
        }
    } else {
        point_pos = len - after;
        if (builder_append(body, digits, point_pos) != 0 ||
            builder_append(body, point, strlen(point)) != 0 ||
            builder_append(body, digits + point_pos, after) != 0) {
            free(text); mpz_clear(scaled); return -1;
        }
    }
    (void)negative; /* sign is taken from the original exact input. */
    free(text); mpz_clear(scaled);
    return 0;
}

static int append_fixed_from_digits(byte_builder_t *body, const char *digits,
                                    size_t n, ppf_exp_t exponent,
                                    int alt) {
    const char *point = decimal_point_string();
    size_t point_len = strlen(point);
    if (exponent <= 0) {
        uint64_t zeros = (uint64_t)(-(exponent + 1)) + 1u;
        if (zeros > SIZE_MAX) return -1;
        if (builder_append_char(body, '0') != 0 ||
            (builder_append(body, point, point_len) != 0) ||
            builder_append_repeat(body, '0', (size_t)zeros) != 0 ||
            builder_append(body, digits, n) != 0)
            return -1;
    } else if ((uint64_t)exponent >= n) {
        uint64_t zeros = (uint64_t)exponent - n;
        if (zeros > SIZE_MAX) return -1;
        if (builder_append(body, digits, n) != 0 ||
            builder_append_repeat(body, '0', (size_t)zeros) != 0 ||
            (alt && builder_append(body, point, point_len) != 0))
            return -1;
    } else {
        size_t split = (size_t)exponent;
        if (builder_append(body, digits, split) != 0 ||
            builder_append(body, point, point_len) != 0 ||
            builder_append(body, digits + split, n - split) != 0)
            return -1;
    }
    return 0;
}

static int format_general_body(byte_builder_t *body, const ppf_t *x,
                               const format_spec_t *s, ppf_context_t *ctx) {
    size_t significant = s->precision_set ?
        (s->precision == 0 ? 1u : (size_t)s->precision) : 6u;
    char *digits = NULL;
    ppf_exp_t exponent;
    long long scientific_exp;
    int use_scientific;
    size_t point_pos = SIZE_MAX, exponent_pos;
    const char *point = decimal_point_string();
    char eletter = s->conversion == 'G' ? 'E' : 'e';
    if (ppf_zero_p(x)) {
        if (builder_append_char(body, '0') != 0) return -1;
        if (s->alt) {
            if (builder_append(body, point, strlen(point)) != 0 ||
                (significant > 1u &&
                 builder_append_repeat(body, '0', significant - 1u) != 0))
                return -1;
        }
        return 0;
    }
    if (get_unsigned_digits(&digits, &exponent, 10, significant, x,
                            s->rounding, ctx) != 0)
        return -1;
    scientific_exp = (long long)exponent - 1;
    use_scientific = scientific_exp < -4 ||
                     scientific_exp >= (long long)significant;
    if (use_scientific) {
        if (builder_append(body, digits, 1u) != 0) goto fail;
        if (significant > 1u || s->alt) {
            point_pos = body->length;
            if (builder_append(body, point, strlen(point)) != 0) goto fail;
            if (significant > 1u &&
                builder_append(body, digits + 1, significant - 1u) != 0)
                goto fail;
        }
        exponent_pos = body->length;
        if (append_exponent(body, eletter, scientific_exp, 2) != 0) goto fail;
        if (!s->alt && point_pos != SIZE_MAX)
            trim_fractional_zeros(body, point_pos, exponent_pos, 0);
    } else {
        if (append_fixed_from_digits(body, digits, significant, exponent,
                                     s->alt) != 0)
            goto fail;
        if (!s->alt) {
            const unsigned char *dp = (const unsigned char *)point;
            size_t dp_len = strlen(point);
            for (size_t i = 0; i + dp_len <= body->length; ++i) {
                if (memcmp(body->data + i, dp, dp_len) == 0) {
                    point_pos = i;
                    exponent_pos = body->length;
                    trim_fractional_zeros(body, point_pos, exponent_pos, 0);
                    break;
                }
            }
        }
    }
    context_string_free(ctx, digits);
    return 0;
fail:
    context_string_free(ctx, digits);
    return -1;
}

static int format_hex_zero_precision_body(byte_builder_t *body,
                                          const ppf_t *x,
                                          const format_spec_t *s,
                                          int uppercase,
                                          ppf_context_t *ctx) {
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char *alphabet = uppercase ? upper_digits : lower_digits;
    const char *prefix = uppercase ? "0X" : "0x";
    char exponent_letter = uppercase ? 'P' : 'p';
    mpz_t exact, magnitude, rounded, remainder;
    ppf_exp_t exponent_out;
    ppf_prec_t precision;
    int negative, increment = 0;

    if (!body || !x || !s || !ctx) return -1;
    if (builder_append(body, prefix, 2u) != 0) return -1;
    if (ppf_zero_p(x)) {
        if (builder_append_char(body, '0') != 0 ||
            (s->alt && builder_append(body, decimal_point_string(),
                                      strlen(decimal_point_string())) != 0) ||
            append_exponent(body, exponent_letter, 0, 1) != 0)
            return -1;
        return 0;
    }
    if (!ppf_regular_p(x)) return -1;

    precision = x->precision;
    if (precision > (ppf_prec_t)ULONG_MAX) return -1;
    exponent_out = x->exponent - 4;
    negative = x->sign < 0;
    mpz_inits(exact, magnitude, rounded, remainder, NULL);
    (void)ppf_get_z_2exp(exact, x);
    mpz_abs(magnitude, exact);

    if (precision <= 4u) {
        mpz_mul_2exp(rounded, magnitude,
                     (mp_bitcnt_t)(4u - (unsigned long)precision));
        mpz_set_ui(remainder, 0u);
    } else {
        mp_bitcnt_t discarded = (mp_bitcnt_t)(precision - 4u);
        mpz_fdiv_q_2exp(rounded, magnitude, discarded);
        mpz_fdiv_r_2exp(remainder, magnitude, discarded);
        if (mpz_sgn(remainder) != 0) {
            switch (s->rounding) {
                case PPF_RNDZ: increment = 0; break;
                case PPF_RNDA: increment = 1; break;
                case PPF_RNDU: increment = !negative; break;
                case PPF_RNDD: increment = negative; break;
                case PPF_RNDF: increment = 0; break;
                case PPF_RNDN:
                default: {
                    int half = mpz_tstbit(magnitude, discarded - 1u);
                    int above = 0;
                    if (discarded > 1u && half) {
                        mp_bitcnt_t first = mpz_scan1(magnitude, 0u);
                        above = first < discarded - 1u;
                    }
                    increment = half && (above || mpz_odd_p(rounded));
                    break;
                }
            }
            ppf_raise(ctx, PPF_FLAG_INEXACT);
        }
    }
    if (increment) mpz_add_ui(rounded, rounded, 1u);
    if (!mpz_fits_ulong_p(rounded)) {
        mpz_clears(exact, magnitude, rounded, remainder, NULL);
        return -1;
    }
    {
        unsigned long digit = mpz_get_ui(rounded);
        if (digit == 16u) {
            digit = 1u;
            exponent_out += 4;
        }
        if (digit > 15u || builder_append_char(body, alphabet[digit]) != 0 ||
            (s->alt && builder_append(body, decimal_point_string(),
                                      strlen(decimal_point_string())) != 0) ||
            append_exponent(body, exponent_letter,
                            (long long)exponent_out, 1) != 0) {
            mpz_clears(exact, magnitude, rounded, remainder, NULL);
            return -1;
        }
    }
    mpz_clears(exact, magnitude, rounded, remainder, NULL);
    return 0;
}

static int format_power_base_body(byte_builder_t *body, const ppf_t *x,
                                  const format_spec_t *s, int base,
                                  const char *prefix, char exponent_letter,
                                  ppf_context_t *ctx) {
    size_t digits_count;
    char *digits = NULL;
    ppf_exp_t exponent;
    size_t body_len;
    long long exponent_out;
    int exact_default = !s->precision_set;
    if (ppf_zero_p(x)) {
        size_t after = s->precision_set ? (size_t)s->precision : 0u;
        if ((prefix && builder_append(body, prefix, strlen(prefix)) != 0) ||
            builder_append_char(body, '0') != 0 ||
            ((after || s->alt) &&
             builder_append(body, decimal_point_string(),
                            strlen(decimal_point_string())) != 0) ||
            (after && builder_append_repeat(body, '0', after) != 0) ||
            append_exponent(body, exponent_letter, 0, 1) != 0)
            return -1;
        return 0;
    }
    if (base == 16 || base == -16) {
        if (s->precision_set && s->precision == 0)
            return format_hex_zero_precision_body(body, x, s,
                                                  base < 0, ctx);
    }
    if (s->precision_set) {
        if ((size_t)s->precision > SIZE_MAX - 1u) return -1;
        digits_count = (size_t)s->precision + 1u;
    } else digits_count = ppf_get_str_ndigits(base, x->precision);
    if (get_unsigned_digits(&digits, &exponent, base, digits_count, x,
                            s->rounding, ctx) != 0)
        return -1;
    body_len = strlen(digits);
    if (exact_default) {
        while (body_len > 1u && digits[body_len - 1u] == '0') --body_len;
    }
    if (prefix && builder_append(body, prefix, strlen(prefix)) != 0) goto fail;
    if (builder_append(body, digits, 1u) != 0) goto fail;
    if (body_len > 1u || s->alt) {
        if (builder_append(body, decimal_point_string(),
                           strlen(decimal_point_string())) != 0)
            goto fail;
        if (body_len > 1u &&
            builder_append(body, digits + 1, body_len - 1u) != 0)
            goto fail;
    }
    exponent_out = (base == 16 || base == -16)
        ? 4ll * ((long long)exponent - 1ll)
        : (long long)exponent - 1ll;
    if (append_exponent(body, exponent_letter, exponent_out, 1) != 0)
        goto fail;
    context_string_free(ctx, digits);
    return 0;
fail:
    context_string_free(ctx, digits);
    return -1;
}

static int format_ppf_value(byte_builder_t *out, const format_spec_t *s,
                            const ppf_t *x) {
    byte_builder_t body;
    ppf_context_t *ctx = ppf_default_context();
    int special, negative, rc = -1;
    size_t prefix = 0;
    if (!x || !ctx) return -1;
    builder_init(&body, ctx);
    special = ppf_nan_p(x) || ppf_inf_p(x);
    negative = numeric_sign(x);
    if (special) {
        if (format_special_body(&body, x, s->conversion) != 0) goto done;
    } else {
        switch (s->conversion) {
            case 'e': case 'E':
                if (format_scientific_body(&body, x, s, ctx) != 0) goto done;
                break;
            case 'f': case 'F':
                if (format_fixed_body(&body, x, s, ctx) != 0) goto done;
                break;
            case 'g': case 'G':
                if (format_general_body(&body, x, s, ctx) != 0) goto done;
                break;
            case 'a':
                if (format_power_base_body(&body, x, s, 16, "0x", 'p', ctx) != 0)
                    goto done;
                prefix = 2u;
                break;
            case 'A':
                if (format_power_base_body(&body, x, s, -16, "0X", 'P', ctx) != 0)
                    goto done;
                prefix = 2u;
                break;
            case 'b':
                if (format_power_base_body(&body, x, s, 2, NULL, 'p', ctx) != 0)
                    goto done;
                break;
            default: goto done;
        }
    }
    if (apply_float_width(out, &body, s, negative, special, prefix) != 0)
        goto done;
    rc = 0;
done:
    builder_clear(&body);
    return rc;
}

static int append_snprintf_result(byte_builder_t *out, const char *spec,
                                  int needed, const void *unused) {
    (void)spec; (void)unused;
    if (needed < 0) return -1;
    return builder_reserve(out, (size_t)needed);
}

#define APPEND_NATIVE_TYPED(type, expression) do { \
    type ppf_format_value = (expression); \
    int need = snprintf(NULL, 0, native_spec, ppf_format_value); \
    if (append_snprintf_result(out, native_spec, need, NULL) != 0) return -1; \
    if (need > 0) { \
        char *dst = (char *)out->data + out->length; \
        int got = snprintf(dst, (size_t)need + 1u, native_spec, ppf_format_value); \
        if (got != need) return -1; \
        out->length += (size_t)need; out->data[out->length] = 0; \
    } \
} while (0)

static int store_standard_n(const format_spec_t *s, va_list *ap,
                            size_t count) {
    switch (s->length) {
        case LEN_HH: { signed char *p=va_arg(*ap,signed char*); if(!p)return -1;*p=(signed char)count;break; }
        case LEN_H: { short *p=va_arg(*ap,short*); if(!p)return -1;*p=(short)count;break; }
        case LEN_L: { long *p=va_arg(*ap,long*); if(!p)return -1;*p=(long)count;break; }
        case LEN_LL: { long long *p=va_arg(*ap,long long*); if(!p)return -1;*p=(long long)count;break; }
        case LEN_J: { intmax_t *p=va_arg(*ap,intmax_t*); if(!p)return -1;*p=(intmax_t)count;break; }
        case LEN_Z: { size_t *p=va_arg(*ap,size_t*); if(!p)return -1;*p=count;break; }
        case LEN_T: { ptrdiff_t *p=va_arg(*ap,ptrdiff_t*); if(!p)return -1;*p=(ptrdiff_t)count;break; }
        default: { int *p=va_arg(*ap,int*); if(!p)return -1;*p=(int)count;break; }
    }
    return 0;
}

static const char *errno_message_for_format(int error_number, int alternate,
                                            char fallback[32],
                                            int *precision_applies) {
    *precision_applies = 1;
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 35)
    if (alternate) {
        const char *name = strerrorname_np(error_number);
        if (name) return name;
        if (snprintf(fallback, 32u, "%d", error_number) < 0) return NULL;
        *precision_applies = 0;
        return fallback;
    }
#else
    (void)alternate;
#endif
#else
    (void)alternate;
#endif
    {
        const char *message = strerror(error_number);
        if (message) return message;
    }
    if (snprintf(fallback, 32u, "%d", error_number) < 0) return NULL;
    return fallback;
}

static int append_errno_message(byte_builder_t *out, const format_spec_t *s) {
    char fallback[32];
    int saved_errno = errno;
    int precision_applies;
    const char *message = errno_message_for_format(saved_errno, s->alt,
                                                    fallback,
                                                    &precision_applies);
    size_t length;
    size_t width;
    size_t padding;

    if (!message) return -1;
    length = strlen(message);
    if (precision_applies && s->precision_set &&
        (size_t)s->precision < length)
        length = (size_t)s->precision;
    width = s->width_set ? (size_t)s->width : 0u;
    padding = width > length ? width - length : 0u;

    if (!s->left && builder_append_repeat(out, ' ', padding) != 0)
        return -1;
    if (builder_append(out, message, length) != 0)
        return -1;
    if (s->left && builder_append_repeat(out, ' ', padding) != 0)
        return -1;

    errno = saved_errno;
    return 0;
}

static int append_standard(byte_builder_t *out, const format_spec_t *s,
                           va_list *ap) {
    char native_spec[128];
    if (s->conversion == 'n') return store_standard_n(s, ap, out->length);
    if (s->conversion == '%') return builder_append_char(out, '%');
    if (build_native_spec(native_spec, s, LEN_NONE) != 0) return -1;
    switch (s->conversion) {
        case 'd': case 'i':
            switch (s->length) {
                case LEN_L: APPEND_NATIVE_TYPED(long, va_arg(*ap,long)); break;
                case LEN_LL: APPEND_NATIVE_TYPED(long long, va_arg(*ap,long long)); break;
                case LEN_J: APPEND_NATIVE_TYPED(intmax_t, va_arg(*ap,intmax_t)); break;
                case LEN_Z: APPEND_NATIVE_TYPED(ssize_t, va_arg(*ap,ssize_t)); break;
                case LEN_T: APPEND_NATIVE_TYPED(ptrdiff_t, va_arg(*ap,ptrdiff_t)); break;
                default: APPEND_NATIVE_TYPED(int, va_arg(*ap,int)); break;
            } break;
        case 'o': case 'u': case 'x': case 'X':
            switch (s->length) {
                case LEN_L: APPEND_NATIVE_TYPED(unsigned long, va_arg(*ap,unsigned long)); break;
                case LEN_LL: APPEND_NATIVE_TYPED(unsigned long long, va_arg(*ap,unsigned long long)); break;
                case LEN_J: APPEND_NATIVE_TYPED(uintmax_t, va_arg(*ap,uintmax_t)); break;
                case LEN_Z: APPEND_NATIVE_TYPED(size_t, va_arg(*ap,size_t)); break;
                case LEN_T: APPEND_NATIVE_TYPED(uintptr_t, va_arg(*ap,uintptr_t)); break;
                default: APPEND_NATIVE_TYPED(unsigned int, va_arg(*ap,unsigned int)); break;
            } break;
        case 'c':
            if (s->length == LEN_L) APPEND_NATIVE_TYPED(wint_t, va_arg(*ap,wint_t));
            else APPEND_NATIVE_TYPED(int, va_arg(*ap,int));
            break;
        case 's':
            if (s->length == LEN_L) APPEND_NATIVE_TYPED(wchar_t *, va_arg(*ap,wchar_t*));
            else APPEND_NATIVE_TYPED(char *, va_arg(*ap,char*));
            break;
        case 'p': APPEND_NATIVE_TYPED(void *, va_arg(*ap,void*)); break;
        case 'a': case 'A': case 'e': case 'E': case 'f': case 'F':
        case 'g': case 'G':
            if (s->length == LEN_CAP_L)
                APPEND_NATIVE_TYPED(long double, va_arg(*ap,long double));
            else APPEND_NATIVE_TYPED(double, va_arg(*ap,double));
            break;
        case 'm':
            return append_errno_message(out, s);
        default: return -1;
    }
    return 0;
}

static int append_gmp(byte_builder_t *out, const format_spec_t *s,
                      va_list *ap) {
    char spec[128];
    char *tmp = NULL;
    int n = -1;
    if (s->conversion == 'n') {
        switch (s->length) {
            case LEN_GMP_Z: { mpz_ptr z=va_arg(*ap,mpz_ptr); if(!z)return -1;mpz_set_ui(z,(unsigned long)out->length);return 0; }
            case LEN_GMP_Q: { mpq_ptr q=va_arg(*ap,mpq_ptr); if(!q)return -1;mpq_set_ui(q,(unsigned long)out->length,1);return 0; }
            case LEN_GMP_F: { mpf_ptr f=va_arg(*ap,mpf_ptr); if(!f)return -1;mpf_set_ui(f,(unsigned long)out->length);return 0; }
            case LEN_GMP_M: { mp_limb_t *m=va_arg(*ap,mp_limb_t*); if(!m)return -1;*m=(mp_limb_t)out->length;return 0; }
            default: return -1;
        }
    }
    if (build_native_spec(spec, s, LEN_NONE) != 0) return -1;
    switch (s->length) {
        case LEN_GMP_Z: n=gmp_asprintf(&tmp,spec,va_arg(*ap,mpz_srcptr));break;
        case LEN_GMP_Q: n=gmp_asprintf(&tmp,spec,va_arg(*ap,mpq_srcptr));break;
        case LEN_GMP_F: n=gmp_asprintf(&tmp,spec,va_arg(*ap,mpf_srcptr));break;
        case LEN_GMP_M: n=gmp_asprintf(&tmp,spec,va_arg(*ap,mp_limb_t));break;
        case LEN_GMP_N: {
            mp_srcptr limbs=va_arg(*ap,mp_srcptr);
            mp_size_t size=va_arg(*ap,mp_size_t);
            n=gmp_asprintf(&tmp,spec,limbs,size);break;
        }
        default:return -1;
    }
    if (n < 0 || !tmp) {
        free(tmp);
        ppf_set_allocflag();
        return -1;
    }
    if (builder_append(out,tmp,(size_t)n)!=0){free(tmp);return -1;}
    free(tmp);
    return 0;
}

static int append_precision_value(byte_builder_t *out, const format_spec_t *s,
                                  va_list *ap) {
    format_spec_t copy = *s;
    char spec[128];
    const char *native_spec;
    ppf_prec_t value = va_arg(*ap, ppf_prec_t);
    if (!(s->conversion=='d'||s->conversion=='i'||s->conversion=='o'||
          s->conversion=='u'||s->conversion=='x'||s->conversion=='X'))
        return -1;
    copy.length = LEN_J;
    if (build_native_spec(spec,&copy,LEN_J)!=0)return -1;
    native_spec = spec;
    if (s->conversion=='d'||s->conversion=='i') {
        APPEND_NATIVE_TYPED(intmax_t, (intmax_t)value);
    } else {
        APPEND_NATIVE_TYPED(uintmax_t, (uintmax_t)value);
    }
    return 0;
}

static int format_all(byte_builder_t *out, const char *format, va_list source) {
    const char *p = format, *literal = format;
    va_list ap;
    va_copy(ap, source);
    while (*p) {
        format_spec_t spec;
        if (*p != '%') { ++p; continue; }
        if (p > literal && builder_append(out,literal,(size_t)(p-literal))!=0)
            goto fail;
        ++p;
        if (*p == '%') {
            if (builder_append_char(out,'%')!=0) goto fail;
            ++p; literal=p; continue;
        }
        if (parse_spec(&p,&spec,&ap)!=0) goto fail;
        if (spec.length == LEN_PPF_R) {
            if (spec.conversion == 'n') {
                ppf_t *target=va_arg(ap,ppf_t*);
                if(!target||ppf_set_uj(target,(uintmax_t)out->length,PPF_RNDN,
                                      ppf_default_context())==PPF_TERNARY_ERROR)
                    goto fail;
            } else {
                const ppf_t *x=va_arg(ap,const ppf_t*);
                if(format_ppf_value(out,&spec,x)!=0)goto fail;
            }
        } else if (spec.length == LEN_PPF_P) {
            if(append_precision_value(out,&spec,&ap)!=0)goto fail;
        } else if (spec.length >= LEN_GMP_F && spec.length <= LEN_GMP_Z) {
            if(append_gmp(out,&spec,&ap)!=0)goto fail;
        } else {
            if(append_standard(out,&spec,&ap)!=0)goto fail;
        }
        if (out->length > INT_MAX) goto overflow;
        literal=p;
    }
    if (p > literal && builder_append(out,literal,(size_t)(p-literal))!=0)
        goto fail;
    if (out->length > INT_MAX) goto overflow;
    va_end(ap);
    return 0;
overflow:
    ppf_set_erangeflag();
    PPF_SET_OVERFLOW_ERRNO();
fail:
    va_end(ap);
    return -1;
}

int ppf_vasprintf(char **text, const char *format, va_list ap) {
    byte_builder_t b;
    ppf_context_t *ctx = ppf_default_context();
    ppf_flags_t saved_flags, produced_flags, retained_flags;
    char *result = NULL;
    size_t output_length = 0;
    int rc;

    if (text) *text = NULL;
    if (!text || !format || !ctx) return -1;
    saved_flags = ppf_flags_save();
    ppf_clear_flags();
    builder_init(&b, ctx);
    rc = format_all(&b, format, ap);
    if (rc == 0) {
        size_t bytes = b.length + 1u;
        output_length = b.length;
        result = (char *)malloc(bytes);
        if (!result) {
            ppf_raise(ctx, PPF_FLAG_ALLOC);
            rc = -1;
        } else if (b.data) {
            memcpy(result, b.data, bytes);
        } else {
            result[0] = 0;
        }
    }
    produced_flags = ppf_flags_save();
    retained_flags = produced_flags & PPF_FLAG_ALLOC;
    if (rc != 0) retained_flags |= produced_flags & PPF_FLAG_ERANGE;
    ppf_flags_restore(saved_flags | retained_flags, PPF_FLAGS_ALL);
    builder_clear(&b);
    if (rc != 0) {
        free(result);
        return -1;
    }
    *text = result;
    return (int)output_length;
}

int ppf_asprintf(char **text, const char *format, ...) {
    va_list ap; int rc;
    va_start(ap,format); rc=ppf_vasprintf(text,format,ap); va_end(ap);
    return rc;
}

int ppf_vsnprintf(char *buffer, size_t size, const char *format, va_list ap) {
    char *text=NULL; int rc=ppf_vasprintf(&text,format,ap);
    if(rc<0)return rc;
    if(size){size_t copy=(size_t)rc<size-1u?(size_t)rc:size-1u;if(buffer&&copy)memcpy(buffer,text,copy);if(buffer)buffer[copy]=0;}
    free(text); return rc;
}

int ppf_snprintf(char *buffer, size_t size, const char *format, ...) {
    va_list ap; int rc;
    va_start(ap,format); rc=ppf_vsnprintf(buffer,size,format,ap); va_end(ap);
    return rc;
}

int ppf_vsprintf(char *buffer, const char *format, va_list ap) {
    char *text=NULL; int rc;
    if(!buffer)return -1;
    rc=ppf_vasprintf(&text,format,ap);
    if(rc>=0)memcpy(buffer,text,(size_t)rc+1u);
    free(text); return rc;
}

int ppf_sprintf(char *buffer, const char *format, ...) {
    va_list ap; int rc;
    va_start(ap,format); rc=ppf_vsprintf(buffer,format,ap); va_end(ap);
    return rc;
}

int ppf_vfprintf(FILE *stream, const char *format, va_list ap) {
    char *text=NULL; int rc;
    if(!stream)stream=stdout;
    rc=ppf_vasprintf(&text,format,ap);
    if(rc<0)return rc;
    if((size_t)rc && fwrite(text,1,(size_t)rc,stream)!=(size_t)rc){free(text);return -1;}
    free(text); return rc;
}

int ppf_fprintf(FILE *stream, const char *format, ...) {
    va_list ap; int rc;
    va_start(ap,format); rc=ppf_vfprintf(stream,format,ap); va_end(ap);
    return rc;
}

int ppf_vprintf(const char *format, va_list ap) {
    return ppf_vfprintf(stdout,format,ap);
}

int ppf_printf(const char *format, ...) {
    va_list ap; int rc;
    va_start(ap,format); rc=ppf_vprintf(format,ap); va_end(ap);
    return rc;
}
