/*
 * SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#include "ppf_internal.h"

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Interchange metadata is not a request to the general object initializer.
   Bound each imported payload and significand allocation independently. */
#ifndef PPF_FPIF_MAX_IMPORT_BYTES
#define PPF_FPIF_MAX_IMPORT_BYTES ((size_t)64u * 1024u * 1024u)
#endif

#define PPF_FPIF_READ_CHUNK_BYTES ((size_t)4096u)

static int write_all(FILE *stream, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    while (size != 0) {
        size_t n = fwrite(p, 1, size, stream);
        if (n == 0) return -1;
        p += n;
        size -= n;
    }
    return 0;
}

static int read_all(FILE *stream, void *data, size_t size) {
    unsigned char *p = (unsigned char *)data;
    while (size != 0) {
        size_t n = fread(p, 1, size, stream);
        if (n == 0) return -1;
        p += n;
        size -= n;
    }
    return 0;
}

static void store_u64_be(unsigned char out[8], uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        out[7u - i] = (unsigned char)(value >> (8u * i));
}

static uint64_t load_u64_be(const unsigned char in[8]) {
    uint64_t out = 0;
    for (unsigned i = 0; i < 8; ++i)
        out = (out << 8) | in[i];
    return out;
}

static void store_u32_be(unsigned char out[4], uint32_t value) {
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)(value >> 16);
    out[2] = (unsigned char)(value >> 8);
    out[3] = (unsigned char)value;
}

static uint32_t load_u32_be(const unsigned char in[4]) {
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

static const uint32_t crc32_table[256] = {
    UINT32_C(0x00000000), UINT32_C(0x77073096), UINT32_C(0xee0e612c), UINT32_C(0x990951ba), UINT32_C(0x076dc419), UINT32_C(0x706af48f), UINT32_C(0xe963a535), UINT32_C(0x9e6495a3),
    UINT32_C(0x0edb8832), UINT32_C(0x79dcb8a4), UINT32_C(0xe0d5e91e), UINT32_C(0x97d2d988), UINT32_C(0x09b64c2b), UINT32_C(0x7eb17cbd), UINT32_C(0xe7b82d07), UINT32_C(0x90bf1d91),
    UINT32_C(0x1db71064), UINT32_C(0x6ab020f2), UINT32_C(0xf3b97148), UINT32_C(0x84be41de), UINT32_C(0x1adad47d), UINT32_C(0x6ddde4eb), UINT32_C(0xf4d4b551), UINT32_C(0x83d385c7),
    UINT32_C(0x136c9856), UINT32_C(0x646ba8c0), UINT32_C(0xfd62f97a), UINT32_C(0x8a65c9ec), UINT32_C(0x14015c4f), UINT32_C(0x63066cd9), UINT32_C(0xfa0f3d63), UINT32_C(0x8d080df5),
    UINT32_C(0x3b6e20c8), UINT32_C(0x4c69105e), UINT32_C(0xd56041e4), UINT32_C(0xa2677172), UINT32_C(0x3c03e4d1), UINT32_C(0x4b04d447), UINT32_C(0xd20d85fd), UINT32_C(0xa50ab56b),
    UINT32_C(0x35b5a8fa), UINT32_C(0x42b2986c), UINT32_C(0xdbbbc9d6), UINT32_C(0xacbcf940), UINT32_C(0x32d86ce3), UINT32_C(0x45df5c75), UINT32_C(0xdcd60dcf), UINT32_C(0xabd13d59),
    UINT32_C(0x26d930ac), UINT32_C(0x51de003a), UINT32_C(0xc8d75180), UINT32_C(0xbfd06116), UINT32_C(0x21b4f4b5), UINT32_C(0x56b3c423), UINT32_C(0xcfba9599), UINT32_C(0xb8bda50f),
    UINT32_C(0x2802b89e), UINT32_C(0x5f058808), UINT32_C(0xc60cd9b2), UINT32_C(0xb10be924), UINT32_C(0x2f6f7c87), UINT32_C(0x58684c11), UINT32_C(0xc1611dab), UINT32_C(0xb6662d3d),
    UINT32_C(0x76dc4190), UINT32_C(0x01db7106), UINT32_C(0x98d220bc), UINT32_C(0xefd5102a), UINT32_C(0x71b18589), UINT32_C(0x06b6b51f), UINT32_C(0x9fbfe4a5), UINT32_C(0xe8b8d433),
    UINT32_C(0x7807c9a2), UINT32_C(0x0f00f934), UINT32_C(0x9609a88e), UINT32_C(0xe10e9818), UINT32_C(0x7f6a0dbb), UINT32_C(0x086d3d2d), UINT32_C(0x91646c97), UINT32_C(0xe6635c01),
    UINT32_C(0x6b6b51f4), UINT32_C(0x1c6c6162), UINT32_C(0x856530d8), UINT32_C(0xf262004e), UINT32_C(0x6c0695ed), UINT32_C(0x1b01a57b), UINT32_C(0x8208f4c1), UINT32_C(0xf50fc457),
    UINT32_C(0x65b0d9c6), UINT32_C(0x12b7e950), UINT32_C(0x8bbeb8ea), UINT32_C(0xfcb9887c), UINT32_C(0x62dd1ddf), UINT32_C(0x15da2d49), UINT32_C(0x8cd37cf3), UINT32_C(0xfbd44c65),
    UINT32_C(0x4db26158), UINT32_C(0x3ab551ce), UINT32_C(0xa3bc0074), UINT32_C(0xd4bb30e2), UINT32_C(0x4adfa541), UINT32_C(0x3dd895d7), UINT32_C(0xa4d1c46d), UINT32_C(0xd3d6f4fb),
    UINT32_C(0x4369e96a), UINT32_C(0x346ed9fc), UINT32_C(0xad678846), UINT32_C(0xda60b8d0), UINT32_C(0x44042d73), UINT32_C(0x33031de5), UINT32_C(0xaa0a4c5f), UINT32_C(0xdd0d7cc9),
    UINT32_C(0x5005713c), UINT32_C(0x270241aa), UINT32_C(0xbe0b1010), UINT32_C(0xc90c2086), UINT32_C(0x5768b525), UINT32_C(0x206f85b3), UINT32_C(0xb966d409), UINT32_C(0xce61e49f),
    UINT32_C(0x5edef90e), UINT32_C(0x29d9c998), UINT32_C(0xb0d09822), UINT32_C(0xc7d7a8b4), UINT32_C(0x59b33d17), UINT32_C(0x2eb40d81), UINT32_C(0xb7bd5c3b), UINT32_C(0xc0ba6cad),
    UINT32_C(0xedb88320), UINT32_C(0x9abfb3b6), UINT32_C(0x03b6e20c), UINT32_C(0x74b1d29a), UINT32_C(0xead54739), UINT32_C(0x9dd277af), UINT32_C(0x04db2615), UINT32_C(0x73dc1683),
    UINT32_C(0xe3630b12), UINT32_C(0x94643b84), UINT32_C(0x0d6d6a3e), UINT32_C(0x7a6a5aa8), UINT32_C(0xe40ecf0b), UINT32_C(0x9309ff9d), UINT32_C(0x0a00ae27), UINT32_C(0x7d079eb1),
    UINT32_C(0xf00f9344), UINT32_C(0x8708a3d2), UINT32_C(0x1e01f268), UINT32_C(0x6906c2fe), UINT32_C(0xf762575d), UINT32_C(0x806567cb), UINT32_C(0x196c3671), UINT32_C(0x6e6b06e7),
    UINT32_C(0xfed41b76), UINT32_C(0x89d32be0), UINT32_C(0x10da7a5a), UINT32_C(0x67dd4acc), UINT32_C(0xf9b9df6f), UINT32_C(0x8ebeeff9), UINT32_C(0x17b7be43), UINT32_C(0x60b08ed5),
    UINT32_C(0xd6d6a3e8), UINT32_C(0xa1d1937e), UINT32_C(0x38d8c2c4), UINT32_C(0x4fdff252), UINT32_C(0xd1bb67f1), UINT32_C(0xa6bc5767), UINT32_C(0x3fb506dd), UINT32_C(0x48b2364b),
    UINT32_C(0xd80d2bda), UINT32_C(0xaf0a1b4c), UINT32_C(0x36034af6), UINT32_C(0x41047a60), UINT32_C(0xdf60efc3), UINT32_C(0xa867df55), UINT32_C(0x316e8eef), UINT32_C(0x4669be79),
    UINT32_C(0xcb61b38c), UINT32_C(0xbc66831a), UINT32_C(0x256fd2a0), UINT32_C(0x5268e236), UINT32_C(0xcc0c7795), UINT32_C(0xbb0b4703), UINT32_C(0x220216b9), UINT32_C(0x5505262f),
    UINT32_C(0xc5ba3bbe), UINT32_C(0xb2bd0b28), UINT32_C(0x2bb45a92), UINT32_C(0x5cb36a04), UINT32_C(0xc2d7ffa7), UINT32_C(0xb5d0cf31), UINT32_C(0x2cd99e8b), UINT32_C(0x5bdeae1d),
    UINT32_C(0x9b64c2b0), UINT32_C(0xec63f226), UINT32_C(0x756aa39c), UINT32_C(0x026d930a), UINT32_C(0x9c0906a9), UINT32_C(0xeb0e363f), UINT32_C(0x72076785), UINT32_C(0x05005713),
    UINT32_C(0x95bf4a82), UINT32_C(0xe2b87a14), UINT32_C(0x7bb12bae), UINT32_C(0x0cb61b38), UINT32_C(0x92d28e9b), UINT32_C(0xe5d5be0d), UINT32_C(0x7cdcefb7), UINT32_C(0x0bdbdf21),
    UINT32_C(0x86d3d2d4), UINT32_C(0xf1d4e242), UINT32_C(0x68ddb3f8), UINT32_C(0x1fda836e), UINT32_C(0x81be16cd), UINT32_C(0xf6b9265b), UINT32_C(0x6fb077e1), UINT32_C(0x18b74777),
    UINT32_C(0x88085ae6), UINT32_C(0xff0f6a70), UINT32_C(0x66063bca), UINT32_C(0x11010b5c), UINT32_C(0x8f659eff), UINT32_C(0xf862ae69), UINT32_C(0x616bffd3), UINT32_C(0x166ccf45),
    UINT32_C(0xa00ae278), UINT32_C(0xd70dd2ee), UINT32_C(0x4e048354), UINT32_C(0x3903b3c2), UINT32_C(0xa7672661), UINT32_C(0xd06016f7), UINT32_C(0x4969474d), UINT32_C(0x3e6e77db),
    UINT32_C(0xaed16a4a), UINT32_C(0xd9d65adc), UINT32_C(0x40df0b66), UINT32_C(0x37d83bf0), UINT32_C(0xa9bcae53), UINT32_C(0xdebb9ec5), UINT32_C(0x47b2cf7f), UINT32_C(0x30b5ffe9),
    UINT32_C(0xbdbdf21c), UINT32_C(0xcabac28a), UINT32_C(0x53b39330), UINT32_C(0x24b4a3a6), UINT32_C(0xbad03605), UINT32_C(0xcdd70693), UINT32_C(0x54de5729), UINT32_C(0x23d967bf),
    UINT32_C(0xb3667a2e), UINT32_C(0xc4614ab8), UINT32_C(0x5d681b02), UINT32_C(0x2a6f2b94), UINT32_C(0xb40bbe37), UINT32_C(0xc30c8ea1), UINT32_C(0x5a05df1b), UINT32_C(0x2d02ef8d),
};

static uint32_t crc32_update(uint32_t crc, const unsigned char *data,
                             size_t size) {
    if (size == 0) return crc;
    if (!data) return 0;
    crc = ~crc;
    for (size_t i = 0; i < size; ++i)
        crc = crc32_table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    return ~crc;
}

size_t ppf_out_str(FILE *stream, int base, size_t digits,
                   const ppf_t *op, ppf_rnd_t rnd,
                   ppf_context_t *ctx) {
    char *raw = NULL;
    ppf_exp_t exponent = 0;
    size_t count = 0, sign_chars, body_len, point_len;
    const char *body, *point;
    char expbuf[64];
    int exp_len;
    char prefix;

    if (!stream) stream = stdout;
    if (!op) return 0;
    if (ppf_nan_p(op)) {
        const char *s = "@NaN@";
        ppf_raise(ppf_resolve_context(ctx), PPF_FLAG_NAN);
        return write_all(stream, s, 5) == 0 ? 5 : 0;
    }
    if (ppf_inf_p(op)) {
        const char *s = op->sign < 0 ? "-@Inf@" : "@Inf@";
        size_t n = strlen(s);
        return write_all(stream, s, n) == 0 ? n : 0;
    }
    if (ppf_zero_p(op)) {
        const char *s = op->sign < 0 ? "-0" : "0";
        size_t n = strlen(s);
        return write_all(stream, s, n) == 0 ? n : 0;
    }

    raw = ppf_get_str(NULL, &exponent, base, digits, op, rnd, ctx);
    if (!raw) return 0;
    sign_chars = raw[0] == '-' ? 1u : 0u;
    body = raw + sign_chars;
    body_len = strlen(body);
    if (body_len == 0 || exponent == INT64_MIN) goto fail;
    point = localeconv()->decimal_point;
    if (!point || !*point) point = ".";
    point_len = strlen(point);
    prefix = (base >= -10 && base <= 10) ? 'e' : '@';
    exp_len = snprintf(expbuf, sizeof(expbuf), "%lld",
                       (long long)(exponent - 1));
    if (exp_len < 0 || (size_t)exp_len >= sizeof(expbuf)) goto fail;
    if (sign_chars > SIZE_MAX - 1u - point_len ||
        sign_chars + 1u + point_len > SIZE_MAX - (body_len - 1u) ||
        sign_chars + 1u + point_len + body_len - 1u >
            SIZE_MAX - 1u - (size_t)exp_len)
        goto fail;
    count = sign_chars + 1u + point_len + (body_len - 1u) +
            1u + (size_t)exp_len;
    if ((sign_chars && write_all(stream, "-", 1) != 0) ||
        write_all(stream, body, 1) != 0 ||
        write_all(stream, point, point_len) != 0 ||
        (body_len > 1u && write_all(stream, body + 1, body_len - 1u) != 0) ||
        write_all(stream, &prefix, 1) != 0 ||
        write_all(stream, expbuf, (size_t)exp_len) != 0) {
        count = 0;
    }
    ppf_free_str(raw);
    return count;

fail:
    ppf_free_str(raw);
    return 0;
}

size_t ppf_inp_str(ppf_t *rop, FILE *stream, int base,
                   ppf_rnd_t rnd, ppf_context_t *ctx) {
    char *word = NULL;
    size_t used = 0, cap = 0, count = 0;
    int c;

    ctx = ppf_resolve_context(ctx);
    if (!rop || !ctx) return 0;
    if (!stream) stream = stdin;
    do {
        c = fgetc(stream);
        if (c == EOF) return 0;
        if (isspace((unsigned char)c)) ++count;
    } while (isspace((unsigned char)c));

    for (;;) {
        if (used + 1u >= cap) {
            size_t next = cap ? cap * 2u : 64u;
            char *tmp;
            if (next <= cap) {
                ppf_raise(ctx, PPF_FLAG_ERANGE);
                ppf_context_free_internal(ctx, word, cap);
                return 0;
            }
            tmp = (char *)ppf_context_realloc_internal(
                ctx, word, cap, next);
            if (!tmp) {
                ppf_context_free_internal(ctx, word, cap);
                return 0;
            }
            word = tmp;
            cap = next;
        }
        word[used++] = (char)c;
        ++count;
        c = fgetc(stream);
        if (c == EOF) break;
        if (isspace((unsigned char)c)) {
            (void)ungetc(c, stream);
            break;
        }
    }
    word[used] = '\0';
    if (ppf_set_str(rop, word, base, rnd, ctx) != 0) {
        ppf_context_free_internal(ctx, word, cap);
        return 0;
    }
    ppf_context_free_internal(ctx, word, cap);
    return count;
}

/* PPFIF01 is a canonical, endian-independent stream format.  It is not the
   private byte representation used by GNU MPFR's experimental FPIF calls. */
static const unsigned char ppfif_magic[8] = {
    'P','P','F','I','F',1,'\r','\n'
};

enum { PPFIF_HEADER_BYTES = 36 };

int ppf_fpif_export(FILE *stream, const ppf_t *op) {
    unsigned char *record = NULL;
    unsigned char *header, *payload, *crcbuf;
    uint64_t payload_size = 0;
    uint32_t crc;
    size_t payload_bytes = 0, record_bytes;
    ppf_allocator_t allocator;

    if (!stream) stream = stdout;
    if (!op || !ppf_validate(op)) return -1;
    allocator = op->allocator;
    if (!allocator.malloc_fn || !allocator.free_fn) {
        ppf_set_erangeflag();
        return -1;
    }
    if (ppf_regular_p(op)) {
        if (op->precision > UINT64_MAX - 7u) {
            ppf_set_erangeflag();
            return -1;
        }
        payload_size = (uint64_t)((op->precision + 7u) / 8u);
        if (payload_size > SIZE_MAX) {
            ppf_set_erangeflag();
            return -1;
        }
        payload_bytes = (size_t)payload_size;
    }
    if (payload_bytes > SIZE_MAX - PPFIF_HEADER_BYTES - 4u) {
        ppf_set_erangeflag();
        return -1;
    }
    record_bytes = PPFIF_HEADER_BYTES + payload_bytes + 4u;
    record = (unsigned char *)allocator.malloc_fn(allocator.opaque,
                                                  record_bytes);
    if (!record) {
        ppf_note_allocation_failure_internal();
        ppf_set_allocflag();
        return -1;
    }
    memset(record, 0, record_bytes);
    header = record;
    payload = record + PPFIF_HEADER_BYTES;
    crcbuf = payload + payload_bytes;

    if (ppf_regular_p(op)) {
        for (size_t i = 0; i < payload_bytes; ++i) {
            uint64_t bit = (uint64_t)(payload_bytes - 1u - i) * 8u;
            size_t limb = (size_t)(bit / PPF_LIMB_BITS);
            unsigned shift = (unsigned)(bit % PPF_LIMB_BITS);
            if (limb >= op->used_limbs) {
                allocator.free_fn(allocator.opaque, record, record_bytes);
                ppf_set_erangeflag();
                return -1;
            }
            payload[i] =
                (unsigned char)((op->limbs[limb] >> shift) & 0xffu);
        }
    }

    memcpy(header, ppfif_magic, sizeof(ppfif_magic));
    header[8] = (unsigned char)op->kind;
    header[9] = op->sign < 0 ? 1u : 0u;
    store_u64_be(header + 12, (uint64_t)op->precision);
    {
        uint64_t encoded_exp = op->exponent < 0
            ? UINT64_MAX - (uint64_t)(-(op->exponent + 1))
            : (uint64_t)op->exponent;
        store_u64_be(header + 20, encoded_exp);
    }
    store_u64_be(header + 28, payload_size);
    crc = crc32_update(0, header, PPFIF_HEADER_BYTES);
    if (payload_bytes) crc = crc32_update(crc, payload, payload_bytes);
    store_u32_be(crcbuf, crc);

    if (write_all(stream, record, record_bytes) != 0) {
        allocator.free_fn(allocator.opaque, record, record_bytes);
        return -1;
    }
    allocator.free_fn(allocator.opaque, record, record_bytes);
    return 0;
}

int ppf_fpif_import(ppf_t *op, FILE *stream, ppf_context_t *ctx) {
    unsigned char header[PPFIF_HEADER_BYTES], crcbuf[4];
    unsigned char *payload = NULL;
    size_t payload_capacity = 0;
    uint64_t precision64, payload64;
    ppf_exp_t exponent;
    ppf_kind_t kind;
    int sign;
    uint32_t actual_crc, expected_crc;
    ppf_t temp;
    int temp_ready = 0, rc = -1;

    ctx = ppf_resolve_context(ctx);
    if (!op || !ctx) return -1;
    if (!stream) stream = stdin;
    if (read_all(stream, header, sizeof(header)) != 0) return -1;
    if (memcmp(header, ppfif_magic, sizeof(ppfif_magic)) != 0 ||
        header[10] != 0 || header[11] != 0 || header[8] > PPF_KIND_NAN ||
        header[9] > 1u)
        return -1;
    kind = (ppf_kind_t)header[8];
    sign = header[9] ? -1 : 1;
    precision64 = load_u64_be(header + 12);
    {
        uint64_t encoded_exp = load_u64_be(header + 20);
        exponent = (encoded_exp & (UINT64_C(1) << 63))
            ? -1 - (ppf_exp_t)(UINT64_MAX - encoded_exp)
            : (ppf_exp_t)encoded_exp;
    }
    payload64 = load_u64_be(header + 28);
    if (!ppf_precision_valid((ppf_prec_t)precision64) ||
        precision64 > UINT64_MAX - 7u) return -1;
    if ((kind == PPF_KIND_REGULAR &&
         payload64 != (precision64 + 7u) / 8u) ||
        (kind != PPF_KIND_REGULAR && payload64 != 0u) ||
        payload64 > SIZE_MAX)
        return -1;

    /* A short input must never trigger an unbounded allocation from a claimed
       precision.  The significand bound also applies to special records,
       whose encoded payload is empty but whose object storage is not. */
    if (payload64 > (uint64_t)PPF_FPIF_MAX_IMPORT_BYTES ||
        ppf_limbs_for_precision((ppf_prec_t)precision64) >
            (size_t)PPF_FPIF_MAX_IMPORT_BYTES / sizeof(mp_limb_t)) {
        ppf_raise(ctx, PPF_FLAG_ERANGE);
        return -1;
    }

    if (payload64) {
        size_t received = 0;
        payload_capacity = (size_t)payload64 < PPF_FPIF_READ_CHUNK_BYTES
            ? (size_t)payload64 : PPF_FPIF_READ_CHUNK_BYTES;
        payload = (unsigned char *)ppf_context_malloc_internal(ctx, payload_capacity);
        if (!payload) return -1;
        while (received < (size_t)payload64) {
            if (received == payload_capacity) {
                size_t next = payload_capacity <= SIZE_MAX / 2u
                    ? payload_capacity * 2u : (size_t)payload64;
                unsigned char *grown;
                if (next > (size_t)payload64) next = (size_t)payload64;
                grown = (unsigned char *)ppf_context_realloc_internal(
                    ctx, payload, payload_capacity, next);
                if (!grown) goto done;
                payload = grown;
                payload_capacity = next;
            }
            if (read_all(stream, payload + received,
                         payload_capacity - received) != 0) goto done;
            received = payload_capacity;
        }
    }
    if (payload64 != 0u && payload == NULL) goto done;
    if (read_all(stream, crcbuf, sizeof(crcbuf)) != 0) goto done;
    expected_crc = load_u32_be(crcbuf);
    actual_crc = crc32_update(0, header, sizeof(header));
    if (payload64) actual_crc = crc32_update(actual_crc, payload,
                                             (size_t)payload64);
    if (actual_crc != expected_crc) goto done;

    if (ppf_init2_ctx(&temp, (ppf_prec_t)precision64, ctx) != 0) goto done;
    temp_ready = 1;
    if (kind == PPF_KIND_ZERO) ppf_set_zero(&temp, sign);
    else if (kind == PPF_KIND_INF) ppf_set_inf(&temp, sign);
    else if (kind == PPF_KIND_NAN) ppf_set_nan(&temp, sign);
    else {
        size_t need = ppf_limbs_for_precision((ppf_prec_t)precision64);
        if (!payload) goto done;
        unsigned top_bits = (unsigned)(precision64 & 7u);
        if (need == 0 || ppf_reserve(&temp, (ppf_prec_t)precision64) != 0)
            goto done;
        if (top_bits != 0u &&
            (payload[0] & (unsigned char)~((1u << top_bits) - 1u)) != 0u)
            goto done;
        memset(temp.limbs, 0, need * sizeof(*temp.limbs));
        for (size_t i = 0; i < (size_t)payload64; ++i) {
            uint64_t bit = (uint64_t)((size_t)payload64 - 1u - i) * 8u;
            size_t limb = (size_t)(bit / PPF_LIMB_BITS);
            unsigned shift = (unsigned)(bit % PPF_LIMB_BITS);
            if (limb >= need) goto done;
            temp.limbs[limb] |= (mp_limb_t)payload[i] << shift;
        }
        temp.used_limbs = need;
        temp.exponent = exponent;
        temp.sign = sign;
        temp.kind = PPF_KIND_REGULAR;
        ppf_zero_unused_bits(&temp);
        if (((temp.limbs[(precision64 - 1u) / PPF_LIMB_BITS] >>
              ((precision64 - 1u) % PPF_LIMB_BITS)) & 1u) == 0u ||
            !ppf_validate(&temp))
            goto done;
    }
    ppf_swap(op, &temp);
    rc = 0;

done:
    if (temp_ready) ppf_clear(&temp);
    ppf_context_free_internal(ctx, payload, payload_capacity);
    return rc;
}
