/* wububase.c -- shared internal utilities (see wububase.h). Clean C11. */
#include "wububase.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------------- UTF-8 ---------------- */
int wububase_utf8_decode(const char *s, uint32_t *cp) {
    if (!s || !*s) return 0;
    const unsigned char *u = (const unsigned char *)s;
    unsigned char b0 = u[0];
    if (b0 < 0x80) { *cp = b0; return 1; }
    if ((b0 & 0xE0) == 0xC0) {                 /* 2-byte */
        if ((u[1] & 0xC0) != 0x80) { *cp = b0; return -1; }
        *cp = ((uint32_t)(b0 & 0x1F) << 6) | (u[1] & 0x3F);
        return 2;
    }
    if ((b0 & 0xF0) == 0xE0) {                 /* 3-byte */
        if ((u[1] & 0xC0) != 0x80 || (u[2] & 0xC0) != 0x80) { *cp = b0; return -1; }
        *cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) | (u[2] & 0x3F);
        return 3;
    }
    if ((b0 & 0xF8) == 0xF0) {                 /* 4-byte */
        if ((u[1] & 0xC0) != 0x80 || (u[2] & 0xC0) != 0x80 || (u[3] & 0xC0) != 0x80) { *cp = b0; return -1; }
        *cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(u[1] & 0x3F) << 12)
            | ((uint32_t)(u[2] & 0x3F) << 6) | (u[3] & 0x3F);
        return 4;
    }
    *cp = b0; return -1;                        /* invalid lead byte */
}

int wububase_utf8_encode(uint32_t cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp; return 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F)); return 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F)); return 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F)); return 4;
    }
}

int wububase_utf8_len(const char *s) {
    if (!s) return 0;
    int n = 0; uint32_t cp; const char *p = s;
    while (*p) {
        int k = wububase_utf8_decode(p, &cp);
        if (k <= 0) { p++; n++; continue; }   /* count invalid bytes too */
        p += k; n++;
    }
    return n;
}

/* ---------------- dynamic string buffer ---------------- */
void buf_init(Buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }

void buf_free(Buf *b) {
    free(b->p);
    b->p = NULL; b->len = 0; b->cap = 0;
}

size_t buf_len(const Buf *b) { return b ? b->len : 0; }

static int buf_reserve(Buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return 0;
    size_t nc = b->cap ? b->cap : 256;
    while (nc < b->len + extra + 1) nc *= 2;
    char *np = realloc(b->p, nc);
    if (!np) return -1;
    b->p = np; b->cap = nc;
    return 0;
}

int buf_add(Buf *b, const char *t) {
    if (!b || !t) return -1;
    size_t al = strlen(t);
    if (buf_reserve(b, al) != 0) return -1;
    memcpy(b->p + b->len, t, al);
    b->len += al;
    b->p[b->len] = '\0';
    return 0;
}

int buf_vprintf(Buf *b, const char *fmt, va_list ap) {
    if (!b || !fmt) return -1;
    va_list ap2; va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) return -1;
    if (buf_reserve(b, (size_t)n) != 0) return -1;
    vsnprintf(b->p + b->len, (size_t)n + 1, fmt, ap);
    b->len += (size_t)n;
    return 0;
}

int buf_printf(Buf *b, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = buf_vprintf(b, fmt, ap);
    va_end(ap);
    return r;
}

const char *buf_str(Buf *b) {
    if (!b) return "";
    if (!b->p) {               /* ensure a valid empty string */
        if (buf_reserve(b, 0) != 0) return "";
        b->p[0] = '\0';
    }
    b->p[b->len] = '\0';
    return b->p;
}

/* ---------------- XML / HTML escaping ---------------- */
int wububase_xml_escape(Buf *b, const char *t) {
    if (!b || !t) return -1;
    for (const char *p = t; *p; p++) {
        switch (*p) {
            case '&': if (buf_add(b, "&amp;"))  return -1; break;
            case '<': if (buf_add(b, "&lt;"))   return -1; break;
            case '>': if (buf_add(b, "&gt;"))   return -1; break;
            case '"': if (buf_add(b, "&quot;")) return -1; break;
            default:  { char c[2] = { *p, 0 }; if (buf_add(b, c)) return -1; }
        }
    }
    return 0;
}
