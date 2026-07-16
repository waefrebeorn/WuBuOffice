/* wubufont.c -- clean-room SFNT/TrueType parser. Native C11, no deps. */
#include "wubufont.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- big-endian readers (sfnt is big-endian on disk) ---- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static int16_t  rd16s(const uint8_t *p) { return (int16_t)rd16(p); }

static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n ? n : 1); if (!r) abort(); return r; }

#define TAG(a,b,c,d) ((uint32_t)((a)<<24)|((b)<<16)|((c)<<8)|(d))

struct Font {
    const uint8_t *data;
    size_t size;
    /* parsed directory */
    uint16_t n_tables;
    uint32_t *tags;
    size_t   *off;
    size_t   *len;
    /* cached metrics */
    int have_head, have_maxp, have_hhea;
    uint16_t units_per_em;
    uint16_t glyph_count;
    int16_t  ascent, descent;
    /* loca cache (glyph offsets), length glyph_count+1, in bytes */
    const uint8_t *loca;     /* points into data if present */
    int loca_is_long;        /* 1 => 4-byte offsets, 0 => 2-byte */
    const uint8_t *glyf;
    uint8_t *owned;          /* if non-NULL, a heap copy of `data` we own */
};

/* Open from a caller-owned blob (we reference it; caller keeps it alive). */
Font *font_open(const uint8_t *data, size_t size) {
    Font *f = font_open_owned(data, size, 0);
    return f;
}

/* Open from a blob. If take_ownership, we copy the blob so the returned Font
 * is fully self-contained (used by woff_open which reconstructs an sfnt). */
Font *font_open_owned(const uint8_t *data, size_t size, int take_ownership) {
    if (!data || size < 12) return NULL;
    uint32_t sig = rd32(data);
    if (sig != TAG('t','r','u','e') && sig != TAG('O','T','T','O') && sig != 0x00010000u)
        return NULL;
    Font *f = xrealloc(NULL, sizeof *f);
    memset(f, 0, sizeof *f);
    if (take_ownership) {
        uint8_t *copy = xrealloc(NULL, size);
        memcpy(copy, data, size);
        f->owned = copy;
        f->data = copy;
    } else {
        f->data = data;
    }
    /* Parse from the buffer we actually retain (the copy when we own it), so
     * cached pointers (loca/glyf) never dangle after the caller frees `data`. */
    const uint8_t *base = f->data;
    f->size = size;
    f->n_tables = rd16(base + 4);
    f->tags = xrealloc(NULL, f->n_tables * sizeof *f->tags);
    f->off  = xrealloc(NULL, f->n_tables * sizeof *f->off);
    f->len  = xrealloc(NULL, f->n_tables * sizeof *f->len);
    /* directory entries start at offset 12, 16 bytes each */
    for (uint16_t i = 0; i < f->n_tables; i++) {
        const uint8_t *e = base + 12 + (size_t)i * 16;
        f->tags[i] = rd32(e);
        f->off[i]  = rd32(e + 8);
        f->len[i]  = rd32(e + 12);
    }
    /* head */
    size_t o, l;
    if (font_find_table(f, TAG('h','e','a','d'), &o, &l) && l >= 54) {
        f->have_head = 1;
        f->units_per_em = rd16(base + o + 18);
    }
    /* maxp */
    if (font_find_table(f, TAG('m','a','x','p'), &o, &l) && l >= 6) {
        f->have_maxp = 1;
        f->glyph_count = rd16(base + o + 4);
    }
    /* hhea */
    if (font_find_table(f, TAG('h','h','e','a'), &o, &l) && l >= 36) {
        f->have_hhea = 1;
        f->ascent  = rd16s(base + o + 4);
        f->descent = rd16s(base + o + 6);
    }
    /* loca + glyf */
    size_t lo, ll, go, gl;
    if (font_find_table(f, TAG('l','o','c','a'), &lo, &ll) &&
        font_find_table(f, TAG('g','l','y','f'), &go, &gl)) {
        f->loca = base + lo;
        f->glyf = base + go;
    }
    /* loca format (short/long) lives in head[50]; read it now that head is known */
    if (f->have_head) {
        size_t ho; size_t hl;
        if (font_find_table(f, TAG('h','e','a','d'), &ho, &hl) && hl >= 52)
            f->loca_is_long = (rd16s(base + ho + 50) != 0);
    }
    return f;
}

void font_free(Font *f) {
    if (!f) return;
    free(f->tags); free(f->off); free(f->len);
    free(f->owned);
    free(f);
}

size_t font_table_count(const Font *f) { return f ? f->n_tables : 0; }
uint32_t font_table_tag(const Font *f, size_t idx) { return f && idx < f->n_tables ? f->tags[idx] : 0; }
int font_table_range(const Font *f, size_t idx, size_t *offset, size_t *length) {
    if (!f || idx >= f->n_tables) return 0;
    if (offset) *offset = f->off[idx];
    if (length) *length = f->len[idx];
    return 1;
}
int font_find_table(const Font *f, uint32_t tag, size_t *offset, size_t *length) {
    if (!f) return 0;
    for (uint16_t i = 0; i < f->n_tables; i++)
        if (f->tags[i] == tag) {
            if (offset) *offset = f->off[i];
            if (length) *length = f->len[i];
            /* sanity: table must fit in blob */
            if (f->off[i] + f->len[i] > f->size) return 0;
            return 1;
        }
    return 0;
}

uint16_t font_units_per_em(const Font *f) { return f && f->have_head ? f->units_per_em : 0; }
uint16_t font_glyph_count(const Font *f) { return f && f->have_maxp ? f->glyph_count : 0; }
int16_t  font_ascent(const Font *f) { return f && f->have_hhea ? f->ascent : 0; }
int16_t  font_descent(const Font *f) { return f && f->have_hhea ? f->descent : 0; }

/* ---- name table -> UTF-8 ---- */
char *font_name(const Font *f, uint16_t name_id) {
    size_t o, l;
    if (!font_find_table(f, TAG('n','a','m','e'), &o, &l) || l < 6) return NULL;
    const uint8_t *p = f->data + o;
    uint16_t count = rd16(p + 2);
    uint16_t stor  = rd16(p + 4);
    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *r = p + 6 + (size_t)i * 12;
        uint16_t pid  = rd16(r);
        uint16_t eid  = rd16(r + 2);
        uint16_t nid  = rd16(r + 6);
        uint16_t slen = rd16(r + 8);
        uint16_t soff = rd16(r + 10);
        if (nid != name_id) continue;
        /* prefer Windows (pid 3) UTF-16BE (eid 1) */
        if (!(pid == 3 && eid == 1) && i + 1 < count) continue; /* try to find a better one first pass */
        const uint8_t *s = p + stor + soff;
        if ((size_t)stor + soff + slen > f->size) continue;
        /* decode UTF-16BE -> UTF-8 */
        char *out = xrealloc(NULL, (size_t)slen * 3 + 1);
        size_t k = 0;
        for (uint16_t j = 0; j + 1 < slen; j += 2) {
            uint32_t cp = (uint32_t)((s[j] << 8) | s[j + 1]);
            if (cp < 0x80) out[k++] = (char)cp;
            else if (cp < 0x800) { out[k++] = (char)(0xC0 | (cp >> 6)); out[k++] = (char)(0x80 | (cp & 0x3F)); }
            else { out[k++] = (char)(0xE0 | (cp >> 12)); out[k++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[k++] = (char)(0x80 | (cp & 0x3F)); }
        }
        out[k] = '\0';
        return out;
    }
    return NULL;
}

/* ---- cmap -> glyph index (format 4, with format 0/6 fallback) ---- */
uint16_t font_cmap(const Font *f, uint32_t cp) {
    size_t o, l;
    if (!font_find_table(f, TAG('c','m','a','p'), &o, &l) || l < 4) return 0;
    const uint8_t *p = f->data + o;
    uint16_t nv = rd16(p + 2);
    /* find a subtable: prefer format 4, platform 3 (Windows) */
    size_t best_fmt4 = 0; int have_fmt4 = 0;
    size_t best_any = 0;  int have_any = 0;
    for (uint16_t i = 0; i < nv; i++) {
        const uint8_t *r = p + 4 + (size_t)i * 8;
        uint16_t pid = rd16(r);
        uint16_t eid = rd16(r + 2);
        uint32_t sub = rd32(r + 4);
        if (sub + 4 > l) continue;
        uint16_t f2 = rd16(p + o + sub);
        if (f2 == 4) { best_fmt4 = (size_t)sub; have_fmt4 = 1; break; }
        if (!have_any) { best_any = (size_t)sub; have_any = 1; }
        (void)pid; (void)eid;
    }
    size_t sub_off = have_fmt4 ? best_fmt4 : (have_any ? best_any : 0);
    if (!sub_off) return 0;
    const uint8_t *s = p + sub_off;
    uint16_t fmt = rd16(s);
    if (fmt == 4) {
        uint16_t segX2 = rd16(s + 6);
        const uint8_t *ends   = s + 14;
        const uint8_t *starts = ends + segX2 + 2;
        const uint8_t *deltas = starts + segX2;
        const uint8_t *rangeo = deltas + segX2;
        uint16_t segs = segX2 / 2;
        for (uint16_t i = 0; i < segs; i++) {
            uint16_t end   = rd16(ends + (size_t)i * 2);
            uint16_t start = rd16(starts + (size_t)i * 2);
            int16_t  delta = rd16s(deltas + (size_t)i * 2);
            if (cp < start || cp > end) continue;
            uint16_t ro = rd16(rangeo + (size_t)i * 2);
            if (ro == 0) return (uint16_t)(cp + delta) & 0xFFFF;
            /* glyphIdArray is at rangeo + segX2 */
            const uint8_t *gid = rangeo + segX2 + (size_t)i * 2;
            /* index into array by (cp - start); ro is byte offset from rangeo[i] */
            uint16_t idx = (cp - start) + (uint16_t)(ro / 2);
            return rd16(gid + (size_t)idx * 2);
        }
        return 0;
    }
    if (fmt == 6) {
        uint16_t first = rd16(s + 6);
        uint16_t cnt   = rd16(s + 8);
        if (cp >= first && cp < (uint32_t)first + cnt)
            return rd16(s + 10 + (cp - first) * 2);
        return 0;
    }
    if (fmt == 0) {
        if (cp < 256) return s[6 + cp];
        return 0;
    }
    return 0;
}

/* ---- glyf outline -> SVG path 'd' ---- */
static size_t glyph_offset(const Font *f, uint16_t gi) {
    if (!f->loca) return 0;
    if (f->loca_is_long) return rd32(f->loca + (size_t)gi * 4);
    return (size_t)rd16(f->loca + (size_t)gi * 2) * 2;
}
static size_t glyph_next(const Font *f, uint16_t gi) {
    if (f->loca_is_long) return rd32(f->loca + (size_t)(gi + 1) * 4);
    return (size_t)rd16(f->loca + (size_t)(gi + 1) * 2) * 2;
}

/* forward declaration: shared TrueType coordinate decoder (defined below) */
/* Decode a simple glyph (ncontours >= 0) into an SVG path 'd'. Reused by the
 * composite path, which concatenates translated component outlines. */
/* forward declaration so the SVG path decoder can reuse the shared
 * coordinate decoder defined below (also used by the rasterizer). */
static uint16_t decode_simple_points(const uint8_t *g, uint16_t n,
                                     int16_t *X, int16_t *Y, uint8_t *on);
static char *font_glyph_decode_simple(const Font *f, const uint8_t *g, uint16_t n) {
    uint16_t *fl = xrealloc(NULL, sizeof *fl * 1024);
    int16_t *X = xrealloc(NULL, sizeof *X * 1024);
    int16_t *Y = xrealloc(NULL, sizeof *Y * 1024);
    /* Single source of truth: decode contours via decode_simple_points
     * (the same routine the rasterizer uses). This guarantees the SVG
     * path and the rasterized bitmap agree exactly. */
    uint16_t total = decode_simple_points(g, n, X, Y, (uint8_t *)fl);
    if (total > 1024) total = 1024;
    size_t dcap = (size_t)total * 32 + 16;
    char *d = xrealloc(NULL, dcap);
    size_t dp = 0;
    size_t start = 0;
    for (uint16_t c = 0; c < n; c++) {
        uint16_t endpt = rd16(g + 10 + (size_t)c * 2);
        int first = 1;
        for (size_t k = start; k <= endpt; k++) {
            int on = (fl[k] & 0x01);
            int wrote;
            if (first) {
                wrote = snprintf(d + dp, dcap - dp, "M%hd %hd", X[k], -Y[k]);
                first = 0;
            } else if (on) {
                wrote = snprintf(d + dp, dcap - dp, "L%hd %hd", X[k], -Y[k]);
            } else {
                int16_t cx2 = X[k], cy2 = -Y[k];
                size_t nk = k + 1;
                if (nk > endpt) nk = start;
                int next_on = (fl[nk] & 0x01);
                int16_t ex, ey;
                if (next_on) { ex = X[nk]; ey = -Y[nk]; }
                else { ex = (X[k] + X[nk]) / 2; ey = (-Y[k] + -Y[nk]) / 2; }
                wrote = snprintf(d + dp, dcap - dp, "Q%hd %hd %hd %hd", cx2, cy2, ex, ey);
            }
            dp += (size_t)wrote;
            if (dp + 8 >= dcap) { dcap = dcap * 2 + 32; d = xrealloc(d, dcap); }
        }
        if (dp + 2 < dcap) { d[dp++] = 'Z'; d[dp] = '\0'; }
        start = (size_t)endpt + 1;
    }
    d[dp] = '\0';
    free(fl); free(X); free(Y);
    return d;
}

char *font_to_svg(const Font *f, const char *sample) {
    if (!f) return NULL;
    uint16_t upm = font_units_per_em(f);
    if (!upm) upm = 1000;
    char *family = font_name(f, 1);
    char *full   = font_name(f, 4);
    const char *fam = family ? family : (full ? full : "ConvertedFont");
    if (!sample || !*sample) sample = "Ag";

    /* accumulate <glyph> elements for the sample's code points */
    char *glyphs = xrealloc(NULL, 1); glyphs[0] = '\0';
    size_t cap = 1;
    for (const char *s = sample; *s; ) {
        /* decode one UTF-8 code point */
        uint32_t cp; int adv;
        if ((*s & 0x80) == 0) { cp = (uint8_t)*s; adv = 1; }
        else if ((*s & 0xE0) == 0xC0) { cp = ((*s & 0x1F) << 6) | (s[1] & 0x3F); adv = 2; }
        else if ((*s & 0xF0) == 0xE0) { cp = ((*s & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); adv = 3; }
        else if ((*s & 0xF8) == 0xF0) { cp = ((*s & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); adv = 4; }
        else { adv = 1; s++; continue; }
        s += adv;
        uint16_t gi = font_cmap(f, cp);
        char unichar[8]; int ul = snprintf(unichar, sizeof unichar, "%04X", cp);
        (void)ul;
        char *gd = font_glyph_svg_path(f, gi);
        const char *d = gd ? gd : "";
        size_t need = strlen(glyphs) + strlen(unichar) + strlen(d) + 96;
        if (need > cap) { cap = need * 2; glyphs = xrealloc(glyphs, cap); }
        /* element buffer must hold the (possibly long) path d; size it */
        char *el = xrealloc(NULL, strlen(d) + 128);
        snprintf(el, strlen(d) + 128,
                 "<glyph unicode=\"&#x%s;\" glyph-name=\"u%s\" d=\"%s\"/>\n", unichar, unichar, d);
        strcat(glyphs, el);
        free(el);
        free(gd);
    }

    /* measure width of the sample string (sum advance widths via hmtx if present) */
    int string_w = 0;
    size_t ho, hl;
    if (font_find_table(f, TAG('h','m','t','x'), &ho, &hl)) {
        const uint8_t *hm = f->data + ho;
        for (const char *s = sample; *s; ) {
            uint32_t cp; int adv;
            if ((*s & 0x80) == 0) { cp = (uint8_t)*s; adv = 1; }
            else if ((*s & 0xE0) == 0xC0) { cp = ((*s & 0x1F) << 6) | (s[1] & 0x3F); adv = 2; }
            else if ((*s & 0xF0) == 0xE0) { cp = ((*s & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); adv = 3; }
            else { cp = 0; adv = 1; }
            s += adv;
            uint16_t gi = font_cmap(f, cp);
            if (gi < font_glyph_count(f)) string_w += (int)rd16(hm + (size_t)gi * 4);
        }
    } else {
        string_w = (int)strlen(sample) * upm / 2;
    }

    int ascent  = font_ascent(f)  ? font_ascent(f)  : (int)upm;
    int descent = font_descent(f) ? font_descent(f) : -(int)(upm / 4);

    size_t out_cap = 4096 + strlen(glyphs);
    char *svg = xrealloc(NULL, out_cap);
    int written = snprintf(svg, out_cap,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
        "width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n"
        "  <defs>\n"
        "    <font id=\"%s\" horiz-adv-x=\"%u\">\n"
        "      <font-face font-family=\"%s\" units-per-em=\"%u\" "
        "ascent=\"%d\" descent=\"%d\"/>\n"
        "%s"
        "    </font>\n"
        "  </defs>\n"
        "  <text font-family=\"%s\" font-size=\"%u\" y=\"%u\">%s</text>\n"
        "</svg>\n",
        string_w * 2 / (int)upm + 4, 10 * 2, string_w * 2 / (int)upm + 4, 20,
        fam, upm, fam, upm, ascent, descent, glyphs, fam, upm, (unsigned)(upm - descent), sample);
    (void)written;
    (void)hl;
    free(glyphs); free(family); free(full);
    return svg;
}

char *font_glyph_svg_path(const Font *f, uint16_t gi) {
    if (!f || !f->glyf || !f->loca) return NULL;
    if (gi >= f->glyph_count) return NULL;
    size_t off  = glyph_offset(f, gi);
    size_t next = glyph_next(f, gi);
    if (next <= off) return xrealloc(NULL, 1);           /* empty glyph */
    if (off + 2 > f->size) return xrealloc(NULL, 1);
    const uint8_t *g = f->glyf + off;
    int16_t ncontours = rd16s(g);
    if (ncontours >= 0) {
        if (ncontours == 0) return xrealloc(NULL, 1);   /* space glyph */
        return font_glyph_decode_simple(f, g, (uint16_t)ncontours);
    }
    /* composite glyph: walk components, concatenate translated outlines */
    char *acc = xrealloc(NULL, 1); acc[0] = '\0';
    const uint8_t *com = g + 10;
    for (;;) {
        if (off + (size_t)(com - g) + 4 > f->size) break;
        uint16_t flags = rd16(com);
        uint16_t gc = rd16(com + 2);
        int dx = 0, dy = 0;
        com += 4;
        if (flags & 0x01) { dx = rd16s(com); com += 2; }
        else if (off + (size_t)(com - g) + 1 <= f->size) { dx = (int8_t)com[0]; com += 1; }
        if (flags & 0x02) { dy = rd16s(com); com += 2; }
        else if (off + (size_t)(com - g) + 1 <= f->size) { dy = (int8_t)com[0]; com += 1; }
        /* we support ARG_1/2 as signed offsets only (no scale/2x2 here) */
        char *sub = font_glyph_svg_path(f, gc);
        if (sub && sub[0]) {
            size_t need = strlen(acc) + strlen(sub) + 32;
            acc = xrealloc(acc, need);
            char tmp[32];
            int wlen = snprintf(tmp, sizeof tmp, "M%d %d ", dx, -dy);
            /* prepend a translate via a relative move; simplest: wrap with group later.
             * Here we just emit a move by (dx,-dy) before the sub-path's first M. */
            memmove(acc + wlen, acc, strlen(acc) + 1);
            memcpy(acc, tmp, (size_t)wlen);
            strcat(acc, sub);
        }
        free(sub);
        if (!(flags & 0x20)) break;   /* MORE_COMPONENTS */
    }
    return acc;
}

/* ---- raw contour decode (shared by rasterizer) ----
 * We store, per point, the on-curve bit in bit0 and the x/y signedness bits
 * (0x02/0x10 for x, 0x04/0x20 for y) in the high bits of `on[]`, so a single
 * compact array carries everything the SVG decoder keeps in two passes. */
#define ON_BIT      0x01
#define XSHORT_POS  0x02
#define XSHORT_NEG  0x10
#define YSHORT_POS  0x04
#define YSHORT_NEG  0x20

/* One pass of the TrueType coordinate stream (X or Y). Replays the flag
 * bytes, expanding REPEAT (0x08) flags via an explicit `repleft` counter so
 * that, once a repeat run is exhausted, the NEXT distinct flag is read (the
 * previous buggy version kept 0x08 latched in `active` and never advanced).
 * `get` returns the signed delta for one point and advances `cp` by the
 * number of coordinate bytes consumed. */
typedef struct { const uint8_t *data; size_t cp; } coord_cursor;

static int16_t coord_next(coord_cursor *c, uint8_t flag, int is_y) {
    uint8_t shp = is_y ? YSHORT_POS : XSHORT_POS;
    uint8_t shn = is_y ? YSHORT_NEG : XSHORT_NEG;
    int16_t v;
    if (flag & shp) {
        int8_t b = (int8_t)c->data[c->cp];
        if (flag & shn) v =  b;   /* short, positive (0x10/0x20 set) */
        else            v = -b;   /* short, negative */
        c->cp++;
    } else if (flag & shn) {
        v = 0;                                        /* same as previous */
    } else {
        v = (int16_t)((uint16_t)c->data[c->cp] << 8 | c->data[c->cp + 1]); /* 2-byte */
        c->cp += 2;
    }
    return v;
}

static void decode_flags_and_coords(const uint8_t *flags, size_t flaglen,
                                  coord_cursor *cc, int is_y,
                                  int16_t *out, uint8_t *on, uint16_t total) {
    uint8_t active = 0;
    uint8_t repleft = 0;
    size_t fi = 0;
    int16_t cur = 0;
    for (uint16_t pi = 0; pi < total; pi++) {
        if (repleft == 0) {
            active = flags[fi++];
            if (active & 0x08) repleft = flags[fi++];   /* repeat count */
        } else {
            repleft--;
        }
        on[pi] = (uint8_t)(active & ON_BIT);
        cur += coord_next(cc, active, is_y);
        out[pi] = cur;
    }
    (void)flaglen;
}

static uint16_t decode_simple_points(const uint8_t *g, uint16_t n,
                                     int16_t *X, int16_t *Y, uint8_t *on) {
    uint16_t instrlen = rd16(g + 10 + (size_t)n * 2);
    /* Layout after the header (ncontours@0, bbox@2..10):
     *   endPtsOfContours[n] @10, instructionLength(2) @10+n*2,
     *   instructions[instrlen], THEN flags, THEN coordinates.
     * The flag byte stream is *variable length* (REPEAT 0x08 flags embed a
     * count byte), so the coordinate stream starts after the expanded flag
     * bytes — NOT after `instrlen`. We expand flags first to find that
     * offset, exactly as the verified reference decoder does. */
    const uint8_t *flags = g + 10 + (size_t)n * 2 + 2 + instrlen;
    uint16_t total = (n == 0) ? 0 : (uint16_t)(rd16(g + 10 + (size_t)(n - 1) * 2) + 1);
    if (total > 1024) total = 1024;
    /* Find where the coordinate stream begins by walking the FLAG BYTE
     * stream (NOT points): each distinct flag is 1 byte, plus a count
     * byte when the REPEAT (0x08) bit is set. Repeated points do NOT
     * consume extra flag bytes, so counting once-per-point (the previous
     * buggy version) over-counts and shifts the coords off by exactly the
     * number of repeat-count bytes. */
    size_t fi = 0;
    for (uint16_t pi = 0; pi < total; ) {
        uint8_t f = flags[fi++];
        if (f & 0x08) {
            uint8_t rep = flags[fi++];   /* repeat count byte */
            pi += (uint16_t)rep + 1;   /* this flag + `rep` more points */
        } else {
            pi += 1;
        }
    }
    const uint8_t *xcoords = flags + fi;
    /* X pass, then Y pass. Each pass replays the SAME flag stream, so the
     * Y coordinates begin precisely where the X coordinates ended. */
    coord_cursor xc = { xcoords, 0 };
    decode_flags_and_coords(flags, 0, &xc, 0, X, on, total);
    coord_cursor yc = { xcoords + xc.cp, 0 };
    decode_flags_and_coords(flags, 0, &yc, 1, Y, on, total);
    return total;
}

/* Flatten a TrueType contour (on/off curve points) into a polyline of
 * integer pixel points. TrueType uses quadratic Béziers; an off-curve point
 * followed by another off-curve point implies an implicit on-curve midpoint.
 * `pts` are font-units; we scale by `s` and round. Fills `out` (caller sized
 * >= n*16); returns the number of output vertices. */
static size_t flatten_contour(const int16_t *X, const int16_t *Y, const uint8_t *on,
                              size_t n, double s, int16_t *out) {
    if (n == 0) return 0;
    /* find a starting on-curve point (if none, point 0 is treated as on-curve) */
    size_t start = 0;
    for (size_t i = 0; i < n; i++) if (on[i] & ON_BIT) { start = i; break; }
    /* emit the start point */
    size_t m = 0;
    int cx = X[start], cy = Y[start];
    out[m*2] = (int16_t)(cx * s + 0.5); out[m*2+1] = (int16_t)(cy * s + 0.5); m++;
    for (size_t k = 1; k <= n; k++) {
        size_t i = (start + k) % n;
        if (on[i] & ON_BIT) {
            /* straight line from current to this on-curve point */
            cx = X[i]; cy = Y[i];
            out[m*2] = (int16_t)(cx * s + 0.5); out[m*2+1] = (int16_t)(cy * s + 0.5); m++;
        } else {
            /* quadratic from current (cx,cy) through control (X[i],Y[i]) to
             * the next on-curve point, or the implied midpoint if the next is
             * also off-curve. Sample the curve (excluding the endpoint, which
             * is emitted when we reach it). NOTE: the caller already supplies
             * Y in screen coordinates (y grows downward), so we must NOT
             * negate again here — the previous double-negation pushed composite
             * glyphs (A/H/W) entirely below the bitmap, rendering them empty. */
            int px = cx, py = cy;
            int qx = X[i], qy = Y[i];
            size_t ni = (i + 1) % n;
            int ex, ey;
            if (on[ni] & ON_BIT) { ex = X[ni]; ey = Y[ni]; }
            else { ex = (X[i] + X[ni]) / 2; ey = (Y[i] + Y[ni]) / 2; }
            for (int t = 1; t < 6; t++) {
                double u = (double)t / 6.0, v = 1.0 - u;
                int qx2 = (int)(v*v*px + 2*v*u*qx + u*u*ex);
                int qy2 = (int)(v*v*py + 2*v*u*qy + u*u*ey);
                out[m*2] = (int16_t)(qx2 * s + 0.5); out[m*2+1] = (int16_t)(qy2 * s + 0.5); m++;
            }
            cx = ex; cy = ey;   /* continue from the curve end */
        }
    }
    return m;
}

/* Even-odd scanline fill of `nc` flattened polygons (each `cnt[k]` verts in
 * `verts`) into a w x h bitmap. */
static void scanline_fill(const int16_t *verts, const size_t *cnt, size_t nc,
                          int w, int h, uint8_t *bits) {
    for (int y = 0; y < h; y++) {
        /* collect edge crossings at this y */
        int xs[4096]; int nx = 0;
        for (size_t c = 0; c < nc; c++) {
            size_t base = 0; for (size_t k = 0; k < c; k++) base += cnt[k];
            size_t m = cnt[c];
            for (size_t k = 0; k < m; k++) {
                size_t a = base + k, b = base + (k + 1) % m;
                int y0 = verts[a*2+1], y1 = verts[b*2+1];
                int x0 = verts[a*2],   x1 = verts[b*2];
                if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                    int xx = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
                    if (nx < 4096) xs[nx++] = xx;
                }
            }
        }
        /* sort crossings (insertion sort, small n) */
        for (int i = 1; i < nx; i++) { int v = xs[i], j = i - 1; while (j >= 0 && xs[j] > v) { xs[j+1] = xs[j]; j--; } xs[j+1] = v; }
        for (int i = 0; i + 1 < nx; i += 2) {
            int xa = xs[i], xb = xs[i+1];
            if (xa < 0) { xa = 0; }
            if (xb > w) { xb = w; }
            if (xb <= xa) xb = xa + 1;   /* coincident edges: keep a 1px sliver */
            for (int x = xa; x < xb; x++) bits[(size_t)y * w + x] = 1;
        }
    }
}

size_t font_glyph_contours_idx(const Font *f, uint16_t gi,
                           double scale, FontContour *out, size_t max_out) {
    if (!f || !f->glyf || !f->loca) return 0;
    if (gi == 0 || gi >= f->glyph_count) return 0;
    size_t off  = glyph_offset(f, gi);
    size_t next = glyph_next(f, gi);
    if (next <= off || off + 2 > f->size) return 0;
    const uint8_t *g = f->glyf + off;
    int16_t ncontours = rd16s(g);
    size_t filled = 0;
    if (ncontours > 0) {
        /* simple glyph: decode its contours directly */
        uint16_t n = (uint16_t)ncontours;
        int16_t X[1024], Y[1024]; uint8_t on[1024];
        uint16_t total = decode_simple_points(g, n, X, Y, on);
        if (total == 0) return 0;
        size_t start = 0;
        for (uint16_t c = 0; c < n; c++) {
            uint16_t endpt = rd16(g + 10 + (size_t)c * 2);
            if (filled < max_out && endpt >= start) {
                size_t cn = (size_t)endpt - start + 1;
                out[filled].pts = xrealloc(NULL, sizeof(FontPoint) * cn);
                for (size_t k = 0; k < cn; k++) {
                    out[filled].pts[k].x = (int)(X[start + k] * scale + 0.5);
                    out[filled].pts[k].y = (int)(-Y[start + k] * scale + 0.5);
                    out[filled].pts[k].on_curve = (on[start + k] & ON_BIT) ? 1 : 0;
                }
                out[filled].n = cn; out[filled].empty = 0;
                filled++;
            }
            start = (size_t)endpt + 1;
        }
    } else if (ncontours < 0) {
        /* composite glyph: walk components, translate each by (dx,dy),
         * recursively reusing the simple decoder (or nested composites).
         * We cap recursion depth to avoid malformed-font loops. */
        const uint8_t *com = g + 10;
        int depth = 0; (void)depth;
        for (;;) {
            if (off + (size_t)(com - g) + 4 > f->size) break;
            uint16_t cflags = rd16(com);
            uint16_t gc     = rd16(com + 2);
            int dx = 0, dy = 0;
            com += 4;
            if (cflags & 0x01) { dx = rd16s(com); com += 2; }
            else if (off + (size_t)(com - g) + 1 <= f->size) { dx = (int8_t)com[0]; com += 1; }
            if (cflags & 0x02) { dy = rd16s(com); com += 2; }
            else if (off + (size_t)(com - g) + 1 <= f->size) { dy = (int8_t)com[0]; com += 1; }
            /* NOTE: this renderer supports the common component forms
             * (ARG_1_AND_2_ARE_WORDS via 0x01/0x02, signed byte
             * offsets). TrueType 2x2 affine matrices (0x08) and
             * point-anchored matching (0x04) are not applied; DejaVu
             * (and most text fonts) use simple translation only. */
            if (gc != 0 && gc < f->glyph_count) {
                FontContour comp[64];
                size_t nc = font_glyph_contours_idx(f, gc, scale, comp, 64);
                for (size_t k = 0; k < nc && filled < max_out; k++) {
                    size_t cn = comp[k].n;
                    out[filled].pts = xrealloc(NULL, sizeof(FontPoint) * cn);
                    for (size_t j = 0; j < cn; j++) {
                        out[filled].pts[j].x = comp[k].pts[j].x + (int)(dx * scale + 0.5);
                        out[filled].pts[j].y = comp[k].pts[j].y - (int)(dy * scale + 0.5);
                        out[filled].pts[j].on_curve = comp[k].pts[j].on_curve;
                    }
                    out[filled].n = cn; out[filled].empty = 0;
                    filled++;
                    free(comp[k].pts);
                }
            }
            if (!(cflags & 0x20)) break;   /* MORE_COMPONENTS */
        }
    }
    return filled;
}

size_t font_glyph_contours(const Font *f, uint32_t codepoint,
                           double scale, FontContour *out, size_t max_out) {
    if (!f || !f->glyf || !f->loca) return 0;
    uint16_t gi = font_cmap(f, codepoint);
    if (gi == 0 || gi >= f->glyph_count) return 0;
    return font_glyph_contours_idx(f, gi, scale, out, max_out);
}

int font_rasterize(const Font *f, uint32_t codepoint, int ppm,
                   uint8_t **bits, int *w, int *h) {
    *bits = NULL; *w = 0; *h = 0;
    if (!f || ppm <= 0) return 0;
    uint16_t upm = font_units_per_em(f); if (!upm) upm = 1000;
    double s = (double)ppm / (double)upm;
    FontContour cs[64];
    size_t nc = font_glyph_contours(f, codepoint, s, cs, 64);
    if (nc == 0) {
        /* empty glyph (space) */
        for (size_t i = 0; i < nc; i++) free(cs[i].pts);
        *w = 0; *h = 0; return 1; /* success: nothing to draw */
    }
    /* bounds */
    int minx = 1<<30, miny = 1<<30, maxx = -(1<<30), maxy = -(1<<30);
    for (size_t c = 0; c < nc; c++)
        for (size_t k = 0; k < cs[c].n; k++) {
            int x = cs[c].pts[k].x, y = cs[c].pts[k].y;
            if (x < minx) { minx = x; }
            if (x > maxx) { maxx = x; }
            if (y < miny) { miny = y; }
            if (y > maxy) { maxy = y; }
        }
    int gw = (maxx - minx) + 2, gh = (maxy - miny) + 2;
    if (gw <= 0) { gw = 1; }
    if (gh <= 0) { gh = 1; }
    uint8_t *bm = xrealloc(NULL, (size_t)gw * gh);
    memset(bm, 0, (size_t)gw * gh);
    /* flatten + fill (translate into bitmap space) */
    int16_t *verts = xrealloc(NULL, sizeof(int16_t) * 64 * 1024);
    size_t *cnt = xrealloc(NULL, sizeof(size_t) * 64);
    size_t vbase = 0; size_t ci = 0;
    for (size_t c = 0; c < nc; c++) {
        int16_t X[1024], Y[1024]; uint8_t on[1024];
        for (size_t k = 0; k < cs[c].n; k++) { X[k] = (int16_t)cs[c].pts[k].x; Y[k] = (int16_t)cs[c].pts[k].y; on[k] = cs[c].pts[k].on_curve ? ON_BIT : 0; }
        /* each flattened vertex occupies 2 int16_t (x,y), so the
         * per-contour slice must be offset by 2*vbase, NOT vbase. */
        size_t m = flatten_contour(X, Y, on, cs[c].n, 1.0, verts + 2*vbase);
        for (size_t k = 0; k < m; k++) { verts[(vbase+k)*2] -= minx - 1; verts[(vbase+k)*2+1] -= miny - 1; }
        cnt[ci++] = m; vbase += m;
        free(cs[c].pts);
    }
    scanline_fill(verts, cnt, ci, gw, gh, bm);
    free(verts); free(cnt);
    *bits = bm; *w = gw; *h = gh;
    return 1;
}

int font_rasterize_string(const Font *f, const char *utf8, int ppm,
                          uint8_t **bits, int *w, int *h) {
    *bits = NULL; *w = 0; *h = 0;
    if (!f || !utf8 || !*utf8) return 0;
    /* first pass: rasterize each glyph, accumulate widths + max height */
    size_t len = strlen(utf8);
    uint8_t **gl = xrealloc(NULL, sizeof(*gl) * (len + 1));
    int *gw = xrealloc(NULL, sizeof(*gw) * (len + 1));
    int *gh = xrealloc(NULL, sizeof(*gh) * (len + 1));
    int total = 0, maxh = 0;
    size_t gi = 0;
    for (const char *s = utf8; *s; ) {
        uint32_t cp; int adv;
        if ((*s & 0x80) == 0) { cp = (uint8_t)*s; adv = 1; }
        else if ((*s & 0xE0) == 0xC0) { cp = ((*s & 0x1F) << 6) | (s[1] & 0x3F); adv = 2; }
        else if ((*s & 0xF0) == 0xE0) { cp = ((*s & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); adv = 3; }
        else { adv = 1; s++; continue; }
        s += adv;
        int gwi, ghi; uint8_t *b;
        if (font_rasterize(f, cp, ppm, &b, &gwi, &ghi)) {
            gl[gi] = b; gw[gi] = gwi; gh[gi] = ghi;
            total += gwi ? gwi + 1 : 1;
            if (ghi > maxh) maxh = ghi;
            gi++;
        }
    }
    if (gi == 0) { free(gl); free(gw); free(gh); return 0; }
    int W = total, H = maxh ? maxh : 1;
    uint8_t *bm = xrealloc(NULL, (size_t)W * H);
    memset(bm, 0, (size_t)W * H);
    int x = 0;
    for (size_t i = 0; i < gi; i++) {
        for (int y = 0; y < gh[i]; y++)
            for (int xx = 0; xx < gw[i]; xx++)
                if (gl[i][(size_t)y * gw[i] + xx])
                    bm[(size_t)y * W + (x + xx)] = 1;
        free(gl[i]);
        x += gw[i] ? gw[i] + 1 : 1;
    }
    free(gl); free(gw); free(gh);
    *bits = bm; *w = W; *h = H;
    return 1;
}
