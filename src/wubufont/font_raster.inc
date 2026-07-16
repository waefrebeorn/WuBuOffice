#include "wubufont_internal.h"

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
             * is emitted when we reach it). Coordinates are screen-space
             * (y grows downward) — note we do NOT negate Y here. */
            int px = cx, py = cy;
            int qx = X[i], qy = Y[i];
            size_t ni = (i + 1) % n;
            int ex, ey;
            if (on[ni] & ON_BIT) { ex = X[ni]; ey = Y[ni]; }
            else { ex = (X[i] + X[ni]) / 2; ey = (Y[i] + Y[ni]) / 2; }
            /* Adaptive subdivision: sample finely enough that no flattened
             * segment spans more than ~1 device pixel. A fixed count
             * under-samples tight strokes (e.g. the top of an 'o' loop) and
             * leaves holes that break connectivity after binarization. */
            double dxs = (double)ex - (double)px, dys = (double)ey - (double)py;
            double len = sqrt(dxs*dxs + dys*dys) * s;
            int steps = (int)(len + 1.0);
            if (steps < 2) steps = 2;
            if (steps > 24) steps = 24;   /* bound output size */
            for (int t = 1; t < steps; t++) {
                double u = (double)t / (double)steps, v = 1.0 - u;
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
