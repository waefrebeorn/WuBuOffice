#include "wubufont_internal.h"

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
