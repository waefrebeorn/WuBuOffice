/* page_compose.c -- synthetic document page composer (see page_compose.h).
 * Clean C11, self-contained. Reuses wubufont + the OcrImage API. */
#include "page_compose.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lexicon.h"   /* utf8_decode (codepoint decode for correct glyph rasterization) */

/* Deterministic PRNG (xorshift32) so a seed yields a reproducible page. */
static unsigned rng_state;
static void rng_seed(unsigned s) { rng_state = s ? s : 0x9E3779B9u; }
static unsigned rng_u32(void) {
    unsigned x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    rng_state = x;
    return x;
}
static double rng_d(void) { return (double)rng_u32() / 4294967296.0; }   /* [0,1) */
static double rng_range(double lo, double hi) { return lo + (hi - lo) * rng_d(); }

/* Bilinear sample of a grayscale tile at (fx,fy); out-of-range = 255 (bg). */
static uint8_t sample_tile(const uint8_t *px, int W, int H, double fx, double fy) {
    if (fx < 0 || fy < 0 || fx > W - 1 || fy > H - 1) return 255;
    int x0 = (int)fx, y0 = (int)fy;
    double dx = fx - x0, dy = fy - y0;
    int x1 = x0 + 1 < W ? x0 + 1 : x0;
    int y1 = y0 + 1 < H ? y0 + 1 : y0;
    double v00 = px[(size_t)y0 * W + x0], v01 = px[(size_t)y0 * W + x1];
    double v10 = px[(size_t)y1 * W + x0], v11 = px[(size_t)y1 * W + x1];
    double top = v00 * (1 - dx) + v01 * dx;
    double bot = v10 * (1 - dx) + v11 * dx;
    return (uint8_t)(top * (1 - dy) + bot * dy);
}

OcrImage *ocr_compose_line(const Font *font, const char *text, int ppm) {
    if (!font || !text) return NULL;
    size_t n = strlen(text);
    if (n == 0) return NULL;
    /* first pass: per-glyph bitmaps */
    int *ws = malloc((n + 1) * sizeof *ws);
    int *hs = malloc((n + 1) * sizeof *hs);
    uint8_t **gs = malloc((n + 1) * sizeof *gs);
    if (!ws || !hs || !gs) { free(ws); free(hs); free(gs); return NULL; }
    int maxh = 0, totalw = 0;
    size_t ng = 0;
    size_t i = 0;
    while (i < n) {
        uint32_t cp;
        int k = utf8_decode(text + i, &cp);   /* decode codepoint, NOT a raw byte */
        if (k <= 0) { cp = (unsigned char)text[i]; k = 1; }
        i += (size_t)k;
        int w = 0, h = 0; uint8_t *b = NULL;
        if (!font_rasterize(font, cp, ppm, &b, &w, &h)) { w = 1; h = 1; b = calloc(1, 1); }
        gs[ng] = b; ws[ng] = w; hs[ng] = h;
        if (h > maxh) maxh = h;
        totalw += w ? w : 1;
        ng++;
    }
    int gutter = ppm / 4 > 2 ? ppm / 4 : 2;
    int W = totalw + (int)ng * gutter;
    int H = maxh + 4;
    OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
    if (!im) {
        for (size_t i = 0; i < ng; i++) free(gs[i]);
        free(ws); free(hs); free(gs);
        return NULL;
    }
    int cx = gutter;
    for (size_t i = 0; i < ng; i++) {
        int gy = (H - hs[i]) / 2;
        for (int y = 0; y < hs[i]; y++)
            for (int x = 0; x < ws[i]; x++) {
                int px = cx + x, py = gy + y;
                if (px < 0 || py < 0 || px >= W || py >= H) continue;
                if (gs[i][(size_t)y * (ws[i] ? ws[i] : 1) + x])
                    ocr_image_set(im, (size_t)px, (size_t)py, 0);
            }
        free(gs[i]);
        cx += (ws[i] ? ws[i] : 1) + gutter;
    }
    free(ws); free(hs); free(gs);
    return im;
}

/* Decode a 1/2/3-byte UTF-8 glyph string to a Unicode codepoint. */
static int utf8_cp(const char *ch) {
    int cp = (unsigned char)ch[0];
    if ((cp & 0xE0) == 0xC0 && ch[1])
        cp = ((cp & 0x1F) << 6) | (ch[1] & 0x3F);
    else if ((cp & 0xF0) == 0xE0 && ch[1] && ch[2])
        cp = ((cp & 0x0F) << 12) | ((ch[1] & 0x3F) << 6) | (ch[2] & 0x3F);
    return cp;
}

#define RAD (3.14159265358979323846 / 180.0)

/* Estimated placement box for a glyph rasterized at `ppm` (used for spacing
 * and overlap rejection; the actual glyph is warped + clipped inside). */
static void box_for_ppm(int ppm, int *gw, int *gh) {
    *gw = (int)((double)ppm * 0.95) + 8;
    *gh = (int)((double)ppm * 1.35) + 8;
}

/* Rasterize `cp` from `fo`, warp it with a random 2D rotation + 3D
 * perspective/shear blend, and stamp the ink into `im` at box (bx,by,gw,gh).
 * One rasterization per placement (no duplicate work). */
static void warp_and_stamp(OcrImage *im, const Font *fo, int cp, int ppm,
                           int bx, int by, int gw, int gh,
                           double maxrot, double maxpersp, double maxshear) {
    double rot   = rng_range(-maxrot, maxrot) * RAD;
    double persp = rng_range(-maxpersp, maxpersp);
    double shear = rng_range(-maxshear, maxshear) * RAD;
    double ca = cos(rot), sa = sin(rot);

    int w = 0, h = 0; uint8_t *b = NULL;
    if (!font_rasterize(fo, (uint32_t)cp, ppm, &b, &w, &h) || !b) {
        if (!b) { w = 1; h = 1; b = calloc(1, 1); }
        else return;
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    int W = (int)ocr_image_width(im), H = (int)ocr_image_height(im);
    for (int dy = 0; dy < gh; dy++) {
        for (int dx = 0; dx < gw; dx++) {
            double fx = (double)(dx - gw / 2);
            double fy = (double)(dy - gh / 2);
            /* 3D-ish perspective: trapezoid pinch over the vertical extent. */
            double ty = gh > 1 ? (double)dy / (gh - 1) : 0.5;   /* 0 top .. 1 bot */
            double pscale = 1.0 - persp * (0.5 - ty);
            if (pscale < 0.05) pscale = 0.05;
            /* shear: vertical tilt as a function of x. */
            double sy = fy + tan(shear) * fx;
            /* 2D rotation of the (perspective-scaled, sheared) point. */
            double rx = fx * pscale * ca - sy * sa;
            double ry = fx * pscale * sa + sy * ca;
            /* back to tile coords (tile centered at w/2,h/2) */
            double sx = rx + w / 2.0, syy = ry + h / 2.0;
            int px = bx + dx, py = by + dy;
            if (px < 0 || py < 0 || (size_t)px >= (size_t)W || (size_t)py >= (size_t)H) continue;
            uint8_t v = sample_tile(b, w, h, sx, syy);
            if (v < 128) ocr_image_set(im, (size_t)px, (size_t)py, 0);
        }
    }
    free(b);
}

OcrImage *ocr_compose_page_ex(const Font *const *fonts, size_t nfonts,
                              const char *const *chars, size_t nchars,
                              size_t W, size_t H, int ppm, unsigned seed,
                              double maxrot, double maxpersp, double maxshear,
                              OcrComposeLayout layout, size_t rows, size_t cols,
                              size_t *out_placed) {
    if (!fonts || nfonts == 0 || !chars || nchars == 0 || W == 0 || H == 0) return NULL;
    OcrImage *im = ocr_image_create(W, H);
    if (!im) return NULL;
    rng_seed(seed);
    size_t placed = 0;

    if (layout == OCR_LAYOUT_SCATTER) {
        /* Non-overlapping crowd via rejection sampling so the downstream
         * layout segments each warped glyph into its own block (a dense
         * overlapping crowd merges into one blob and can't be read). */
        size_t max_glyphs = (nchars > 60) ? 60 : nchars;
        int *bxs = malloc(max_glyphs * sizeof *bxs);
        int *bys = malloc(max_glyphs * sizeof *bys);
        int *bws = malloc(max_glyphs * sizeof *bws);
        int *bhs = malloc(max_glyphs * sizeof *bhs);
        if (!bxs || !bys || !bws || !bhs) {
            free(bxs); free(bys); free(bws); free(bhs);
            ocr_image_free(im);
            return NULL;
        }
        size_t attempts = max_glyphs * 12;
        for (size_t a = 0; a < attempts && placed < max_glyphs; a++) {
            const Font *fo = fonts[rng_u32() % nfonts];
            const char *ch = chars[rng_u32() % nchars];
            int cp = utf8_cp(ch);
            int gw = 0, gh = 0; box_for_ppm(ppm, &gw, &gh);
            int bx = (int)rng_range(0, (double)W - gw);
            int by = (int)rng_range(0, (double)H - gh);
            if (bx < 0) bx = 0;
            if (by < 0) by = 0;
            int overlap = 0;
            for (size_t k = 0; k < placed; k++)
                if (bx < bxs[k] + bws[k] + 2 && bx + gw + 2 > bxs[k] &&
                    by < bys[k] + bhs[k] + 2 && by + gh + 2 > bys[k]) { overlap = 1; break; }
            if (overlap) continue;
            warp_and_stamp(im, fo, cp, ppm, bx, by, gw, gh, maxrot, maxpersp, maxshear);
            bxs[placed] = bx; bys[placed] = by; bws[placed] = gw; bhs[placed] = gh;
            placed++;
        }
        free(bxs); free(bys); free(bws); free(bhs);
    } else if (layout == OCR_LAYOUT_LINES) {
        /* Left-to-right baseline lines; each glyph individually warped. */
        size_t nlines = rows ? rows : 1;
        double gutter = (double)ppm / 6.0;
        double lineH  = (double)ppm * 1.5;
        double top    = (double)ppm * 0.3;
        for (size_t r = 0; r < nlines; r++) {
            double y = top + (double)r * lineH;
            if (y + lineH > (double)H) break;
            double x = (double)ppm * 0.3;
            size_t perline = cols ? cols : nchars;
            for (size_t c = 0; c < perline && x < (double)W; c++) {
                const Font *fo = fonts[rng_u32() % nfonts];
                const char *ch = chars[rng_u32() % nchars];
                int cp = utf8_cp(ch);
                int gw = 0, gh = 0; box_for_ppm(ppm, &gw, &gh);
                int bx = (int)x, by = (int)(y + (lineH - gh) / 2.0);
                if (bx < 0) bx = 0;
                if (by < 0) by = 0;
                warp_and_stamp(im, fo, cp, ppm, bx, by, gw, gh, maxrot, maxpersp, maxshear);
                placed++;
                x += gw + gutter;
            }
        }
    } else { /* OCR_LAYOUT_GRID: rows x cols cell grid (line-grid model). */
        size_t nrows = rows ? rows : 1;
        size_t ncols = cols ? cols : 1;
        double cellW = (double)W / (double)ncols;
        double cellH = (double)H / (double)nrows;
        for (size_t r = 0; r < nrows; r++) {
            for (size_t c = 0; c < ncols; c++) {
                double cx0 = c * cellW, cy0 = r * cellH;
                const Font *fo = fonts[rng_u32() % nfonts];
                const char *ch = chars[rng_u32() % nchars];
                int cp = utf8_cp(ch);
                int gw = 0, gh = 0; box_for_ppm(ppm, &gw, &gh);
                if (gw > (int)cellW) gw = (int)cellW;
                if (gh > (int)cellH) gh = (int)cellH;
                if (gw < 1) gw = 1;
                if (gh < 1) gh = 1;
                int bx = (int)(cx0 + (cellW - gw) / 2.0);
                int by = (int)(cy0 + (cellH - gh) / 2.0);
                if (bx < 0) bx = 0;
                if (by < 0) by = 0;
                warp_and_stamp(im, fo, cp, ppm, bx, by, gw, gh, maxrot, maxpersp, maxshear);
                placed++;
            }
        }
    }

    if (out_placed) *out_placed = placed;
    return im;
}

OcrImage *ocr_compose_page(const Font *const *fonts, size_t nfonts,
                           const char *const *chars, size_t nchars,
                           size_t W, size_t H, int ppm, unsigned seed,
                           double maxrot, double maxpersp, double maxshear,
                           size_t *out_placed) {
    return ocr_compose_page_ex(fonts, nfonts, chars, nchars, W, H, ppm, seed,
                               maxrot, maxpersp, maxshear, OCR_LAYOUT_SCATTER, 0, 0,
                               out_placed);
}
