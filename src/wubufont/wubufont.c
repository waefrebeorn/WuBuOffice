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
};

Font *font_open(const uint8_t *data, size_t size) {
    if (!data || size < 12) return NULL;
    uint32_t sig = rd32(data);
    if (sig != TAG('t','r','u','e') && sig != TAG('O','T','T','O') && sig != 0x00010000u)
        return NULL;
    Font *f = xrealloc(NULL, sizeof *f);
    memset(f, 0, sizeof *f);
    f->data = data; f->size = size;
    f->n_tables = rd16(data + 4);
    f->tags = xrealloc(NULL, f->n_tables * sizeof *f->tags);
    f->off  = xrealloc(NULL, f->n_tables * sizeof *f->off);
    f->len  = xrealloc(NULL, f->n_tables * sizeof *f->len);
    /* directory entries start at offset 12, 16 bytes each */
    for (uint16_t i = 0; i < f->n_tables; i++) {
        const uint8_t *e = data + 12 + (size_t)i * 16;
        f->tags[i] = rd32(e);
        f->off[i]  = rd32(e + 8);
        f->len[i]  = rd32(e + 12);
    }
    /* head */
    size_t o, l;
    if (font_find_table(f, TAG('h','e','a','d'), &o, &l) && l >= 54) {
        f->have_head = 1;
        f->units_per_em = rd16(data + o + 18);
    }
    /* maxp */
    if (font_find_table(f, TAG('m','a','x','p'), &o, &l) && l >= 6) {
        f->have_maxp = 1;
        f->glyph_count = rd16(data + o + 4);
    }
    /* hhea */
    if (font_find_table(f, TAG('h','h','e','a'), &o, &l) && l >= 36) {
        f->have_hhea = 1;
        f->ascent  = rd16s(data + o + 4);
        f->descent = rd16s(data + o + 6);
    }
    /* loca + glyf */
    size_t lo, ll, go, gl;
    if (font_find_table(f, TAG('l','o','c','a'), &lo, &ll) &&
        font_find_table(f, TAG('g','l','y','f'), &go, &gl)) {
        f->loca = data + lo;
        f->glyf = data + go;
    }
    /* loca format (short/long) lives in head[50]; read it now that head is known */
    if (f->have_head) {
        size_t ho; size_t hl;
        if (font_find_table(f, TAG('h','e','a','d'), &ho, &hl) && hl >= 52)
            f->loca_is_long = (rd16s(data + ho + 50) != 0);
    }
    return f;
}

void font_free(Font *f) {
    if (!f) return;
    free(f->tags); free(f->off); free(f->len);
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

/* Decode a simple glyph (ncontours >= 0) into an SVG path 'd'. Reused by the
 * composite path, which concatenates translated component outlines. */
static char *font_glyph_decode_simple(const Font *f, const uint8_t *g, uint16_t n) {
    uint16_t instrlen = rd16(g + 10 + (size_t)n * 2);
    const uint8_t *flags = g + 12 + (size_t)n * 2;
    const uint8_t *xcoords = flags + instrlen;
    /* pass 1: read flags (1 or 2 bytes each, repeat bit 0x08) */
    uint16_t *fl = xrealloc(NULL, sizeof *fl * 1024);
    int16_t *X = xrealloc(NULL, sizeof *X * 1024);
    int16_t *Y = xrealloc(NULL, sizeof *Y * 1024);
    /* count total points = last end point + 1 */
    uint16_t total = (n == 0) ? 0 : (uint16_t)(rd16(g + 10 + (size_t)(n - 1) * 2) + 1);
    if (total > 1024) total = 1024;
    size_t fi = 0;
    while (fi < total) {
        uint8_t flag = flags[fi];
        fl[fi] = flag;
        fi++;
        if (flag & 0x08) { /* repeat */
            uint8_t rep = flags[fi]; fi++;
            for (uint8_t r = 0; r < rep && fi < total; r++) fl[fi++] = flag;
        }
    }
    /* pass 2: x coordinates (delta, signedness from flag bit 0x02) */
    size_t xp = 0;
    int16_t cx = 0;
    for (uint16_t i = 0; i < total; i++) {
        uint8_t flag = fl[i] & 0xFF;
        if (flag & 0x02) { cx += (int16_t)xcoords[xp]; xp++; }
        else if (flag & 0x10) { cx += (int8_t)xcoords[xp]; xp++; }
        else { cx -= (int8_t)xcoords[xp]; xp++; }
        X[i] = cx;
    }
    /* pass 3: y coordinates */
    const uint8_t *ycoords = xcoords + xp;
    size_t yp = 0;
    int16_t cy = 0;
    for (uint16_t i = 0; i < total; i++) {
        uint8_t flag = fl[i] & 0xFF;
        if (flag & 0x04) { cy += (int16_t)ycoords[yp]; yp++; }
        else if (flag & 0x20) { cy += (int8_t)ycoords[yp]; yp++; }
        else { cy -= (int8_t)ycoords[yp]; yp++; }
        Y[i] = cy;
    }
    /* build path; SVG flips Y. Capacity-safe: grow d as we append. */
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
