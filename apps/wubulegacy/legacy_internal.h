/* legacy_internal.h -- shared helpers for the wubulegacy decoders.
 * Static-inline only (header-local); no separate translation unit needed and
 * no duplication across xls_biff.c / doc_bin.c / ppt_bin.c. */

#ifndef WUBULEGACY_INTERNAL_H
#define WUBULEGACY_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static inline uint16_t lg_rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t lg_rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Read an entire file into memory. Returns malloc'd buffer (caller frees) and
 * sets *len; NULL on error. */
static inline uint8_t *lg_slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long s = ftell(f);
    if (s < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t *d = malloc((size_t)s ? (size_t)s : 1);
    if (!d) { fclose(f); return NULL; }
    if (fread(d, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(d); return NULL; }
    fclose(f);
    *len = (size_t)s;
    return d;
}

/* Append one BMP code point as UTF-8 to *p (advances *p). The caller must have
 * reserved at least 3 bytes. Surrogate/astral handling is out of scope for the
 * legacy formats (BMP covers all realistic legacy content). */
static inline void lg_put_utf8(uint32_t cp, char **p) {
    char *o = *p;
    if (cp < 0x80) {
        *o++ = (char)cp;
    } else if (cp < 0x800) {
        *o++ = (char)(0xC0 | (cp >> 6));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *o++ = (char)(0xE0 | (cp >> 12));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    }
    *p = o;
}

/* Decode `nchars` characters at `src` into a fresh malloc'd UTF-8 string.
 * highbyte=0: one byte per char (code point = byte). highbyte=1: 2 bytes LE per
 * char. Returns NULL on alloc failure. */
static inline char *lg_u16_to_utf8(const uint8_t *src, size_t nchars, int highbyte) {
    char *out = malloc(nchars * 3 + 1);
    if (!out) return NULL;
    char *p = out;
    for (size_t i = 0; i < nchars; i++) {
        uint32_t cp = highbyte ? lg_rd16(src + i * 2) : src[i];
        lg_put_utf8(cp, &p);
    }
    *p = '\0';
    return out;
}

#endif /* WUBULEGACY_INTERNAL_H */
