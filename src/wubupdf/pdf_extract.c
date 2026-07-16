/* pdf_extract.c -- clean-room PDF text extraction (see pdf_extract.h). */
#define _GNU_SOURCE          /* memmem */
#define _POSIX_C_SOURCE 200809L
#include "pdf_extract.h"
#include "inflate.h"   /* wubuzip_inflate (raw DEFLATE / zlib) */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- dynamic output buffer ---------- */
typedef struct { char *p; size_t len, cap; } DBuf;
static int db_add(DBuf *d, const char *s, size_t n) {
    if (d->len + n + 1 > d->cap) {
        size_t nc = d->cap ? d->cap * 2 : 1024;
        while (nc < d->len + n + 1) nc *= 2;
        char *np = realloc(d->p, nc);
        if (!np) return -1;
        d->p = np; d->cap = nc;
    }
    memcpy(d->p + d->len, s, n); d->len += n; d->p[d->len] = '\0';
    return 0;
}
static int db_putc(DBuf *d, char c) { return db_add(d, &c, 1); }

/* WinAnsi (CP1252) byte -> Unicode codepoint; identity for 0x00..0x7F and
 * 0xA0..0xFF; '?' for undefined controls. */
static uint32_t winansi(uint8_t b) {
    static const uint16_t hi[32] = {
        0x20AC,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,0x02C6,
        0x2030,0x0160,0x2039,0x0152,0x0000,0x017D,0x0000,0x0000,
        0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x02DC,
        0x2122,0x0161,0x203A,0x0153,0x0000,0x017E,0x0000,0x0178
    };
    if (b < 0x80) return b;
    if (b >= 0xA0) return b;            /* Latin-1 identity */
    uint16_t cp = hi[b - 0x80];
    return cp ? cp : '?';
}

/* emit one decoded byte as UTF-8 into d */
static void emit_utf8(DBuf *d, uint8_t b) {
    uint32_t cp = winansi(b);
    if (cp < 0x80) db_putc(d, (char)cp);
    else if (cp < 0x800) {
        db_putc(d, (char)(0xC0 | (cp >> 6)));
        db_putc(d, (char)(0x80 | (cp & 0x3F)));
    } else {
        db_putc(d, (char)(0xE0 | (cp >> 12)));
        db_putc(d, (char)(0x80 | ((cp >> 6) & 0x3F)));
        db_putc(d, (char)(0x80 | (cp & 0x3F)));
    }
}

/* ---------- content-stream text scanner ----------
 * Walks a (possibly inflated) content stream and appends decoded text. Text
 * is shown by (...) Tj, [ (...) (...) ] TJ, (...) ', (...) ". Hex strings
 * <...> are also supported inside those. A newline is emitted on each
 * text-positioning operator so layout survives. */
static void scan_content(const uint8_t *p, size_t n, DBuf *out) {
    size_t i = 0;
    int last_was_text = 0;   /* separate consecutive show ops with a space */
    while (i < n) {
        uint8_t c = p[i];
        if (c == '(') {
            /* balanced parenthesized literal string with \ escapes */
            i++;
            int depth = 1;
            while (i < n && depth > 0) {
                uint8_t d = p[i++];
                if (d == '\\') {
                    if (i >= n) break;
                    uint8_t e = p[i++];
                    if (e >= '0' && e <= '7') {           /* \ddd octal */
                        int v = e - '0';
                        int k = 1;
                        while (k < 3 && i < n && p[i] >= '0' && p[i] <= '7') {
                            v = v * 8 + (p[i++] - '0'); k++;
                        }
                        emit_utf8(out, (uint8_t)(v & 0xFF));
                    } else if (e == 'n') db_putc(out, '\n');
                    else if (e == 'r') db_putc(out, '\r');
                    else if (e == 't') db_putc(out, '\t');
                    else if (e == 'b') db_putc(out, '\b');
                    else if (e == 'f') db_putc(out, '\f');
                    else if (e == '(' || e == ')' || e == '\\') emit_utf8(out, e);
                    else db_putc(out, (char)e);   /* unknown escape -> literal */
                } else if (d == '(') depth++;
                else if (d == ')') { if (--depth == 0) break; else emit_utf8(out, d); }
                else emit_utf8(out, d);
            }
            last_was_text = 1;
        } else if (c == '<') {
            /* hex string <xx xx ...> */
            i++;
            int hi = -1;
            while (i < n && p[i] != '>') {
                uint8_t h = p[i++];
                if (h >= '0' && h <= '9') h -= '0';
                else if (h >= 'a' && h <= 'f') h = h - 'a' + 10;
                else if (h >= 'A' && h <= 'F') h = h - 'A' + 10;
                else continue;   /* whitespace/garbage between nibbles */
                if (hi < 0) hi = h;
                else { emit_utf8(out, (uint8_t)((hi << 4) | h)); hi = -1; }
            }
            if (i < n && p[i] == '>') i++;
            last_was_text = 1;
        } else if (c == 'T' && i + 1 < n) {
            char nxt = p[i + 1];
            /* text-positioning operators -> paragraph break for layout */
            if (nxt == '*' || nxt == 'd' || nxt == 'D') {
                if (last_was_text) { db_putc(out, '\n'); last_was_text = 0; }
            } else if (nxt == 'j' || nxt == 'J' || nxt == '\'' || nxt == '"') {
                /* a show op completed: separate from the next with a space */
                if (last_was_text) { db_putc(out, ' '); }
                last_was_text = 0;
            } else if (nxt == 'E' && i + 2 < n && p[i + 2] == 'T') {
                if (last_was_text) { db_putc(out, '\n'); last_was_text = 0; }
            }
            i++;
        } else {
            i++;
        }
    }
}

/* Inflate one stream body if it is FlateDecode; scan the result (or raw body
 * if inflation is unavailable/not-flate) for text. */
static void process_stream(const uint8_t *body, size_t blen, int flate, DBuf *out) {
    if (flate) {
        uint8_t *inf = NULL; size_t ilen = 0;
        if (wubuzip_inflate(body, blen, &inf, &ilen, 1) == 0 && ilen > 0) {
            scan_content(inf, ilen, out);
            free(inf);
            return;
        }
        /* fall through to raw scan if inflate fails */
    }
    scan_content(body, blen, out);
}

char *pdf_extract_text(const uint8_t *data, size_t len) {
    if (!data || len < 5 || memcmp(data, "%PDF-", 5) != 0) return NULL;

    DBuf out = {0};
    const char *hay = (const char *)data;
    size_t i = 0;
    while (i + 6 < len) {
        if (memcmp(hay + i, "stream", 6) == 0) {
            /* Is this stream FlateDecode? Look at the dict immediately before
             * "stream" (between the nearest "<<" and "stream"). */
            int flate = 0;
            size_t ds = i;
            while (ds > 0 && !(hay[ds] == '<' && hay[ds+1] == '<')) ds--;
            if (hay[ds] == '<' && hay[ds+1] == '<') {
                size_t dlen = i - (ds + 2);
                if (dlen && memmem(hay + ds + 2, dlen, "FlateDecode", 11))
                    flate = 1;
            }

            size_t j = i + 6;
            if (j < len && (hay[j] == '\r' || hay[j] == '\n')) {
                if (hay[j] == '\r' && j + 1 < len && hay[j + 1] == '\n') j += 2;
                else j += 1;
            }
            /* Bounded search for "endstream" (hay is NOT NUL-terminated, so
             * strstr() would read past `len` and corrupt the heap). */
            size_t end = 0; int found = 0;
            for (size_t k = j; k + 9 <= len; k++) {
                if (memcmp(hay + k, "endstream", 9) == 0) { end = k; found = 1; break; }
            }
            if (!found) break;
            size_t body_end = end;
            if (body_end > j && hay[body_end - 1] == '\n') {
                body_end--;
                if (body_end > j && hay[body_end - 1] == '\r') body_end--;
            }
            if (body_end > j) process_stream(data + j, body_end - j, flate, &out);
            i = end + 9;   /* past 'endstream' */
        } else {
            i++;
        }
    }

    if (out.len == 0) { free(out.p); return NULL; }
    return out.p;
}
