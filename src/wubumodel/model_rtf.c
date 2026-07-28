/* model_rtf.c -- Rich Text Format (.rtf) import for the unified model
 * (closes the RTF *import* half of gap 79; export already ships in wubuexp).
 *
 * A pragmatic RTF reader: tokenizes the {\rtf1 ...} control stream into
 * paragraphs + runs. Handles the constructs our exporter (and common RTF)
 * produce:
 *   \par            -> end current paragraph (start a new one)
 *   \pard           -> (paragraph reset; ignored structurally)
 *   \line / \tab    -> newline / tab
 *   \{ \} \\         -> escaped literals
 *   \'hh            -> hex byte (interpreted as Latin-1)
 *   \fonttbl \colortbl \stylesheet \info \* -> skipped destinations
 *
 * Self-contained: no third-party deps. Sufficient for round-tripping our own
 * RTF and reading standard word-processor RTF text. */

#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    wubumodel_doc *doc;
    wubumodel_node *sec;     /* current section (created lazily) */
    wubumodel_node *par;     /* current paragraph (created lazily) */
    int skip;                /* depth of skipped destinations */
    int skip_depth;          /* group depth at which skip began */
    int group_depth;
    char *buf;               /* pending run text */
    size_t bcap, blen;
} rtf_ctx_t;

static wubumodel_node *rtf_sec(rtf_ctx_t *c) {
    if (!c->sec) {
        c->sec = wubumodel_node_create(c->doc, WUBUMODEL_SECTION);
    }
    return c->sec;
}
static wubumodel_node *rtf_par(rtf_ctx_t *c) {
    if (!c->par) {
        wubumodel_node *p = wubumodel_node_create(c->doc, WUBUMODEL_PARAGRAPH);
        wubumodel_node_append(c->doc, rtf_sec(c), p);
        c->par = p;
    }
    return c->par;
}
static void rtf_flush(rtf_ctx_t *c) {
    if (c->blen == 0) return;
    wubumodel_node *p = rtf_par(c);
    wubumodel_node *run = wubumodel_node_create(c->doc, WUBUMODEL_RUN);
    c->buf[c->blen] = 0;
    wubumodel_run_set_text(run, c->buf);
    wubumodel_node_append(c->doc, p, run);
    c->blen = 0;
}
static void rtf_putc(rtf_ctx_t *c, int ch) {
    if (c->skip) return;
    if (c->blen + 1 + 1 > c->bcap) {
        c->bcap = (c->blen + 64) * 2;
        char *nb = realloc(c->buf, c->bcap);
        if (!nb) return;
        c->buf = nb;
    }
    c->buf[c->blen++] = (char)ch;
}
static void rtf_par_break(rtf_ctx_t *c) {
    rtf_flush(c);
    c->par = NULL; /* next text starts a fresh paragraph */
}

/* Map a few common \'hh codepage bytes (CP1252-ish) to UTF-8. */
static void rtf_hex(rtf_ctx_t *c, int hi, int lo) {
    int code = hi * 16 + lo;
    /* CP1252 overrides for the 0x80–0x9F range */
    static const int cp1252[32] = {
        0x20AC,0x0081,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,
        0x02C6,0x2030,0x0160,0x2039,0x0152,0x008D,0x017D,0x008F,
        0x0090,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
        0x02DC,0x2122,0x0161,0x203A,0x0153,0x009D,0x017E,0x0178 };
    int u = (code >= 0x80 && code <= 0x9F) ? cp1252[code - 0x80] : code;
    if (u < 0x80) rtf_putc(c, u);
    else if (u < 0x800) {
        rtf_putc(c, 0xC0 | (u >> 6));
        rtf_putc(c, 0x80 | (u & 0x3F));
    } else {
        rtf_putc(c, 0xE0 | (u >> 12));
        rtf_putc(c, 0x80 | ((u >> 6) & 0x3F));
        rtf_putc(c, 0x80 | (u & 0x3F));
    }
}

int wubumodel_load_rtf(const char *path, wubumodel_doc **out) {
    if (!path || !out) return -1;
    *out = NULL;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); return -1; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return -1; }
    char *src = malloc((size_t)sz + 1);
    if (!src) { fclose(fp); return -1; }
    if (fread(src, 1, (size_t)sz, fp) != (size_t)sz) { fclose(fp); free(src); return -1; }
    fclose(fp);
    src[sz] = 0;

    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) { free(src); return -1; }
    rtf_ctx_t c; memset(&c, 0, sizeof c); c.doc = d;

    const char *s = src;
    while (*s) {
        if (*s == '{') {
            c.group_depth++;
            s++;
            /* peek for a skipped destination */
            const char *p = s;
            while (*p == '\\' || *p == ' ' || *p == '*') {
                if (*p != '\\') { p++; continue; }
                /* control word */
                p++;
                const char *w = p;
                while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) p++;
                size_t wl = (size_t)(p - w);
                if (wl == 7 && strncmp(w, "fonttbl", 7) == 0) { c.skip = 1; c.skip_depth = c.group_depth; break; }
                if (wl == 8 && strncmp(w, "colortbl", 8) == 0) { c.skip = 1; c.skip_depth = c.group_depth; break; }
                if (wl == 9 && strncmp(w, "stylesheet", 9) == 0) { c.skip = 1; c.skip_depth = c.group_depth; break; }
                if (wl == 4 && strncmp(w, "info", 4) == 0) { c.skip = 1; c.skip_depth = c.group_depth; break; }
                if (wl == 1 && *w == '*') { c.skip = 1; c.skip_depth = c.group_depth; break; }
                /* not a skipped dest; stop peeking */
                break;
            }
            continue;
        }
        if (*s == '}') {
            if (c.skip && c.group_depth == c.skip_depth) c.skip = 0;
            if (c.group_depth > 0) c.group_depth--;
            s++;
            continue;
        }
        if (*s == '\\') {
            s++;
            if (*s == '{') { rtf_putc(&c, '{'); s++; continue; }
            if (*s == '}') { rtf_putc(&c, '}'); s++; continue; }
            if (*s == '\\') { rtf_putc(&c, '\\'); s++; continue; }
            if (*s == ';') { s++; continue; }
            if (*s == '\'') {
                s++;
                int hi = (*s >= '0' && *s <= '9') ? *s - '0'
                       : (*s >= 'a' && *s <= 'f') ? *s - 'a' + 10
                       : (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : 0;
                s++;
                int lo = (*s >= '0' && *s <= '9') ? *s - '0'
                       : (*s >= 'a' && *s <= 'f') ? *s - 'a' + 10
                       : (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : 0;
                s++;
                rtf_hex(&c, hi, lo);
                continue;
            }
            /* control word: letters, optional '-' + digits, then a space delimiter */
            const char *w = s;
            while (*s && ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) s++;
            size_t wl = (size_t)(s - w);
            /* optional numeric param */
            if (*s == '-' || (*s >= '0' && *s <= '9')) {
                while (*s == '-' || (*s >= '0' && *s <= '9')) s++;
            }
            if (*s == ' ') s++; /* delimiter space is consumed */
            if (wl == 3 && strncmp(w, "par", 3) == 0) { rtf_par_break(&c); }
            else if (wl == 4 && strncmp(w, "line", 4) == 0) { rtf_putc(&c, '\n'); }
            else if (wl == 3 && strncmp(w, "tab", 3) == 0) { rtf_putc(&c, '\t'); }
            else if (wl == 3 && strncmp(w, "pard", 3) == 0) { /* reset; ignore */ }
            /* all other control words ignored */
            continue;
        }
        /* plain text */
        rtf_putc(&c, (unsigned char)*s);
        s++;
    }
    rtf_flush(&c);
    free(c.buf);
    free(src);

    if (!c.sec) { wubumodel_doc_destroy(d); return -1; }
    *out = d;
    return 0;
}
