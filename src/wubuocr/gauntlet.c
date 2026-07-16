/* gauntlet.c -- OCR robustness battery (see gauntlet.h).
 *
 * Pure, deterministic corruptions over OcrImage (grayscale), plus an
 * accuracy sweep and a font-ablation study for a multi-font OcrFontBank.
 * No third-party libs: rotation/perspective use bilinear resampling,
 * the DFT low-pass is a naive O((W*H)^2) 2D transform, and the
 * block quantizer is an 8x8 forward/inverse quant loop. All small
 * enough to run in milliseconds on render-sized glyph pages.
 */
#include "gauntlet.h"
#include "wubufont.h"   /* Font, font_open, font_rasterize */
#include "wubuocr.h"    /* OcrPage, ocr_page_from_netpbm, ocr_page_to_json, ocr_page_block_count, ocr_page_free */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ---- small helpers ---- */
static double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Bilinear sample of a grayscale image at (fx,fy); out-of-range = 255. */
static uint8_t sample_bilin(const uint8_t *px, int W, int H, double fx, double fy) {
    if (fx < 0 || fy < 0 || fx > W - 1 || fy > H - 1) return 255;
    int x0 = (int)fx, y0 = (int)fy;
    double dx = fx - x0, dy = fy - y0;
    int x1 = x0 + 1 < W ? x0 + 1 : x0;
    int y1 = y0 + 1 < H ? y0 + 1 : y0;
    double v00 = px[(size_t)y0 * W + x0];
    double v01 = px[(size_t)y0 * W + x1];
    double v10 = px[(size_t)y1 * W + x0];
    double v11 = px[(size_t)y1 * W + x1];
    double top = v00 * (1 - dx) + v01 * dx;
    double bot = v10 * (1 - dx) + v11 * dx;
    return (uint8_t)clampd(top * (1 - dy) + bot * dy, 0, 255);
}

/* Render `text` from `fo` at `ppm` into a single PGM-style page with
 * wide gutters (reuses the same layout the fontbank test uses). The
 * returned OcrImage is owned by the callers and freed via ocr_image_free.
 * On failure returns NULL. */
static OcrImage *render_page(const Font *fo, const char *text, int ppm) {
    size_t n = strlen(text);
    int *ws = malloc((n + 1) * sizeof *ws);
    int *hs = malloc((n + 1) * sizeof *hs);
    uint8_t **gs = malloc((n + 1) * sizeof *gs);
    if (!ws || !hs || !gs) { free(ws); free(hs); free(gs); return NULL; }
    int maxh = 0, totalw = 0;
    for (size_t i = 0; i < n; i++) {
        int w = 0, h = 0; uint8_t *b = NULL;
        if (!font_rasterize(fo, (uint32_t)text[i], ppm, &b, &w, &h)) { w = 1; h = 1; b = calloc(1, 1); }
        gs[i] = b; ws[i] = w; hs[i] = h;
        if (h > maxh) maxh = h;
        totalw += w ? w : 1;
    }
    int gutter = ppm / 4 > 2 ? ppm / 4 : 2;
    int W = totalw + (int)n * gutter;
    int H = maxh + 4;
    OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
    if (!im) { for (size_t i = 0; i < n; i++) free(gs[i]); free(ws); free(hs); free(gs); return NULL; }
    int cx = gutter;
    for (size_t i = 0; i < n; i++) {
        int gy = (H - hs[i]) / 2;
        for (int y = 0; y < hs[i]; y++)
            for (int x = 0; x < ws[i]; x++) {
                int px = cx + x, py = gy + y;
                if (px < 0 || py < 0 || px >= W || py >= H) continue;
                if (gs[i][(size_t)y * (ws[i] ? ws[i] : 1) + x])
                    ocr_image_set(im, (size_t)px, (size_t)py, 0);  /* ink */
            }
        free(gs[i]);
        cx += (ws[i] ? ws[i] : 1) + gutter;
    }
    free(ws); free(hs); free(gs);
    return im;
}

/* ---- corruptions ---- */

/* 2D rotation about the center by `deg` degrees. */
static OcrImage *op_rotate(const OcrImage *im, double deg) {
    int W = (int)ocr_image_width(im), H = (int)ocr_image_height(im);
    OcrImage *out = ocr_image_create((size_t)W, (size_t)H);
    if (!out) return NULL;
    double a = deg * 3.14159265358979323846 / 180.0;
    double ca = cos(a), sa = sin(a);
    double cx = (W - 1) / 2.0, cy = (H - 1) / 2.0;
    const uint8_t *src = ocr_image_pixels(im);
    uint8_t *dst = (uint8_t *)ocr_image_pixels(out);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double dx = x - cx, dy = y - cy;
            double sx = cx + dx * ca + dy * sa;
            double sy = cy - dx * sa + dy * ca;
            dst[(size_t)y * W + x] = sample_bilin(src, W, H, sx, sy);
        }
    return out;
}

/* 3D-ish perspective: scale each row horizontally by a trapezoid
 * factor that depends on the row's vertical position (top narrower). */
static OcrImage *op_perspective(const OcrImage *im, double k) {
    int W = (int)ocr_image_width(im), H = (int)ocr_image_height(im);
    OcrImage *out = ocr_image_create((size_t)W, (size_t)H);
    if (!out) return NULL;
    const uint8_t *src = ocr_image_pixels(im);
    uint8_t *dst = (uint8_t *)ocr_image_pixels(out);
    for (int y = 0; y < H; y++) {
        double t = (H <= 1) ? 0.5 : (double)y / (H - 1);  /* 0 top .. 1 bottom */
        double scale = 1.0 - k * (0.5 - t);          /* pinch at top/bottom */
        if (scale <= 0.001) scale = 0.001;
        double mid = (W - 1) / 2.0;
        for (int x = 0; x < W; x++) {
            double sx = mid + (x - mid) / scale;
            dst[(size_t)y * W + x] = sample_bilin(src, W, H, sx, (double)y);
        }
    }
    return out;
}

/* Naive 2D DFT low-pass: transform, keep the lowest-frequency
 * `keep` fraction of coefficients (radially), inverse. */
static OcrImage *op_dft_lowpass(const OcrImage *im, double keep) {
    int W = (int)ocr_image_width(im), H = (int)ocr_image_height(im);
    if (W <= 1 || H <= 1) return ocr_image_create((size_t)W, (size_t)H);
    const uint8_t *src = ocr_image_pixels(im);
    /* complex buffers (re, im) as double, size W*H */
    double *re = malloc((size_t)W * H * sizeof *re);
    double *imc = malloc((size_t)W * H * sizeof *imc);
    if (!re || !imc) { free(re); free(imc); return NULL; }
    /* forward 2D DFT (naive) */
    for (int v = 0; v < H; v++)
        for (int u = 0; u < W; u++) {
            double sre = 0, sim = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    double ang = -2.0 * 3.14159265358979323846 *
                                 ((double)u * x / W + (double)v * y / H);
                    double s = (double)src[(size_t)y * W + x] - 128.0;
                    sre += s * cos(ang);
                    sim += s * sin(ang);
                }
            re[(size_t)v * W + u] = sre;
            imc[(size_t)v * W + u] = sim;
        }
    /* zero high frequencies: keep iff |freq| within keep radius */
    double maxr = sqrt((double)(W*W + H*H)) / 2.0;
    double rcut = maxr * keep;
    for (int v = 0; v < H; v++)
        for (int u = 0; u < W; u++) {
            double du = (u <= W/2) ? u : u - W;
            double dv = (v <= H/2) ? v : v - H;
            double r = sqrt(du*du + dv*dv);
            if (r > rcut) { re[(size_t)v * W + u] = 0; imc[(size_t)v * W + u] = 0; }
        }
    /* inverse 2D DFT */
    OcrImage *out = ocr_image_create((size_t)W, (size_t)H);
    if (!out) { free(re); free(imc); return NULL; }
    uint8_t *dst = (uint8_t *)ocr_image_pixels(out);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double sre = 0;
            for (int v = 0; v < H; v++)
                for (int u = 0; u < W; u++) {
                    double ang = 2.0 * 3.14159265358979323846 *
                                 ((double)u * x / W + (double)v * y / H);
                    sre += re[(size_t)v * W + u] * cos(ang) - imc[(size_t)v * W + u] * sin(ang);
                }
            double val = sre / ((double)W * H) + 128.0;
            dst[(size_t)y * W + x] = (uint8_t)clampd(val, 0, 255);
        }
    free(re); free(imc);
    return out;
}

/* JPEG-like 8x8 block quantization: forward 1D-DCT-lite + uniform
 * quantize + inverse, per 8x8 block. `q` is the step (>1 = loss). */
static double dct_basis(int k, int n) {
    return cos(3.14159265358979323846 * (2*n + 1) * k / 16.0) *
           (k == 0 ? 0.7071067811865476 : 1.0);
}
static OcrImage *op_block_quant(const OcrImage *im, double q) {
    int W = (int)ocr_image_width(im), H = (int)ocr_image_height(im);
    OcrImage *out = ocr_image_create((size_t)W, (size_t)H);
    if (!out) return NULL;
    const uint8_t *src = ocr_image_pixels(im);
    uint8_t *dst = (uint8_t *)ocr_image_pixels(out);
    double *buf = malloc(64 * sizeof *buf);
    if (!buf) { /* fall back to a copy */ memcpy(dst, src, (size_t)W*H); return out; }
    int step = q < 1 ? 1 : (int)q;
    for (int by = 0; by + 8 <= H; by += 8)
        for (int bx = 0; bx + 8 <= W; bx += 8) {
            /* gather 8x8 */
            for (int i = 0; i < 64; i++) {
                int yy = by + i / 8, xx = bx + i % 8;
                buf[i] = (double)src[(size_t)yy * W + xx] - 128.0;
            }
            /* 2D DCT via separable 1D (naive, tiny) */
            double coef[64];
            for (int v = 0; v < 8; v++)
                for (int u = 0; u < 8; u++) {
                    double s = 0;
                    for (int y = 0; y < 8; y++)
                        for (int x = 0; x < 8; x++)
                            s += buf[y*8+x] * dct_basis(u, x) * dct_basis(v, y);
                    coef[v*8+u] = s * 0.25;
                }
            /* quantize (round to nearest step) */
            for (int i = 0; i < 64; i++)
                coef[i] = (double)( (int)(coef[i] / step) ) * step;
            /* inverse 2D DCT */
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    double s = 0;
                    for (int v = 0; v < 8; v++)
                        for (int u = 0; u < 8; u++)
                            s += coef[v*8+u] * dct_basis(u, x) * dct_basis(v, y);
                    double val = s * 0.25 + 128.0;
                    int yy = by + y, xx = bx + x;
                    dst[(size_t)yy * W + xx] = (uint8_t)clampd(val, 0, 255);
                }
        }
    free(buf);
    return out;
}

OcrImage *ocr_gauntlet_apply(const OcrImage *im, GOp op, double amount) {
    if (!im) return NULL;
    switch (op) {
        case GA_ROTATE:      return op_rotate(im, amount);
        case GA_PERSPECTIVE:  return op_perspective(im, amount);
        case GA_DFT_LOWPASS: return op_dft_lowpass(im, amount < 0.02 ? 0.02 : amount);
        case GA_BLOCK_QUANT:  return op_block_quant(im, amount);
        default: return NULL;
    }
}

/* ---- accuracy sweep ---- */
size_t ocr_gauntlet_sweep(const OcrFontBank *bank, const Font *probe,
                             const char *text, int ppm, GOp op,
                             const double *amounts, size_t n, double *acc) {
    if (!bank || !probe || !text || !amounts || n == 0) return 0;
    size_t done = 0;
    for (size_t i = 0; i < n; i++) {
        double a = amounts[i];
        OcrImage *clean = render_page(probe, text, ppm);
        OcrImage *corr = clean ? ocr_gauntlet_apply(clean, op, a) : NULL;
        double score = 0.0;
        if (corr) {
            /* OCR the corrupted page through the bank, count correct glyphs.
             * Use the public block-text accessor (no JSON scraping):
             * each reading-order block contributes its recognized
             * string; compare positionally to the source word. */
            uint8_t *pgm = NULL; size_t pgmlen = 0;
            if (ocr_image_to_pgm(corr, &pgm, &pgmlen) == 0) {
                OcrPage *pg = ocr_page_from_netpbm(pgm, pgmlen,
                                                   ocr_fontbank_recognizer(),
                                                   (void *)bank);
                if (pg) {
                    size_t total = strlen(text);
                    size_t nb = ocr_page_block_count(pg);
                    size_t correct = 0, pos = 0;
                    for (size_t bi = 0; bi < nb && pos < total; bi++) {
                        const char *t = ocr_page_block_text(pg, bi);
                        if (t && t[0] == text[pos]) correct++;
                        if (getenv("GA_DBG")) printf("    blk[%zu]=[%s] (pos %zu -> %c)\n", bi, t?t:"", pos, text[pos]);
                        /* advance past this block's glyph run: count its chars */
                        size_t adv = t ? strlen(t) : 1;
                        if (adv == 0) adv = 1;
                        pos += adv;
                        free((void *)t);   /* ocr_page_block_text returns a heap string */
                    }
                    score = total ? (double)correct / (double)total : 0.0;
                    ocr_page_free(pg);
                }
                free(pgm);
            }
        }
        if (acc) acc[i] = score;
        ocr_image_free(clean);
        ocr_image_free(corr);
        done++;
    }
    return done;
}

/* ---- font ablation ---- */
double ocr_gauntlet_ablate(const OcrFontBank *bank,
                            const void *const *fonts, size_t nfonts,
                            const Font *probe, const char *text,
                            int ppm, size_t drop_index) {
    (void)bank;
    if (drop_index >= nfonts || nfonts < 2) return 0.0;
    /* rebuild the bank without fonts[drop_index] */
    const void *kept[OCR_FONTBANK_MAX];
    size_t k = 0;
    for (size_t i = 0; i < nfonts && k < OCR_FONTBANK_MAX; i++)
        if (i != drop_index) kept[k++] = fonts[i];
    OcrFontBank *reduced = ocr_fontbank_build(kept, k, 5, ppm);
    if (!reduced) return 0.0;
    /* accuracy on the clean probe text (severity 0) */
    double amts[1] = { 0.0 };
    double acc[1] = { 0.0 };
    ocr_gauntlet_sweep(reduced, probe, text, ppm, GA_ROTATE, amts, 1, acc);
    ocr_fontbank_free(reduced);
    return acc[0];
}
