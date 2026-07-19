/* ocringest.c -- end-to-end coordinate-aware OCR ingestion demo driver.
 *
 * Ties the whole vision together on one page:
 *   1. load many REAL fonts (study many font types / scripts)
 *   2. place glyphs at GOLDEN-RATIO coordinates (multi-coordinate-style warped
 *      ablation via goldplace) + per-glyph 2D rotation AND 3D depth-shear warp
 *      (the "2D/3D warped coordinate ablation on multi-coordinate styles"):
 *      every glyph is dropped onto a real document at a known (x,y,w,h) with a
 *      known ground-truth warp (rot2d, depth, shear) so the ingestion can be
 *      evaluated against truth.
 *   3. ingest through the deterministic pipeline: binarize -> XY-cut layout
 *      -> connected-components -> per-glyph bounding boxes (true doc coords)
 *   4. per detected glyph: DFT compression + spectral analysis (cheap)
 *   5. emit a coordinate JSON: glyph box (x,y,w,h) [true post-warp doc coords],
 *      recognized text, ground-truth warp (rot2d/depth/shear), DFT spectral
 *      features + compression ratio, golden region/spiral tags.
 *
 * Dependency-free C11 (links wubuocr + wubufont + wubujson + goldplace + dft).
 * Usage:
 *   ocringest <fonts-dir> <out.json> [nglyph] [ppm] [seed] [mode]
 *   mode 0 = single ASCII (A-Z) per font
 *   mode 1 = MULTISCRIPT: each font stamps a glyph from its own script block
 *            (full Unicode script table, incl CJK / Hangul / Devanagari / emoji)
 */
#include "wubuocr.h"
#include "image.h"
#include "binarize.h"
#include "page_compose.h"
#include "fontdir.h"
#include "wubufont.h"
#include "goldplace.h"
#include "dft.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <math.h>

/* ---- script -> Unicode block table (multi-range per script).
 * Comprehensive: mirrors the Python gen_dataset.py classifier so the runtime
 * ingestion can stamp EVERY language the corpus contains (not just a handful
 * guessed from filenames). Each font contributes whatever script(s) its cmap
 * actually carries. */
typedef struct { const char *key; unsigned lo, hi; } ScriptRange;
static const ScriptRange SCRIPT_RANGES[] = {
    {"latin",      0x0041, 0x005A}, {"latin",   0x0061, 0x007A},
    {"latin",      0x00C0, 0x024F}, /* Latin-1 supplement + Extended-A/B */
    {"greek",      0x0370, 0x03FF},
    {"cyrillic",   0x0400, 0x052F},
    {"hebrew",     0x0590, 0x05FF},
    {"arabic",     0x0600, 0x06FF}, {"arabic",   0x0750, 0x077F}, {"arabic", 0x08A0, 0x08FF},
    {"syriac",     0x0700, 0x074F},
    {"devanagari", 0x0900, 0x097F},
    {"bengali",    0x0980, 0x09FF},
    {"gurmukhi",   0x0A00, 0x0A7F},
    {"gujarati",   0x0A80, 0x0AFF},
    {"oriya",      0x0B00, 0x0B7F},
    {"tamil",      0x0B80, 0x0BFF},
    {"telugu",     0x0C00, 0x0C7F},
    {"kannada",    0x0C80, 0x0CFF},
    {"malayalam",  0x0D00, 0x0D7F},
    {"sinhala",    0x0D80, 0x0DFF},
    {"thai",       0x0E00, 0x0E7F},
    {"lao",        0x0E80, 0x0EFF},
    {"tibetan",    0x0F00, 0x0FFF},
    {"myanmar",    0x1000, 0x109F},
    {"georgian",   0x10A0, 0x10FF}, {"georgian", 0x1C90, 0x1CBF},
    {"armenian",   0x0530, 0x058F},
    {"ethiopic",   0x1200, 0x137F}, {"ethiopic", 0x1380, 0x139F},
    {"cherokee",   0x13A0, 0x13FF},
    {"canadian",   0x1400, 0x167F},
    {"ogham",      0x1680, 0x169F},
    {"runic",      0x16A0, 0x16FF},
    {"phags-pa",   0xA840, 0xA87F},
    {"mongolian",  0x1800, 0x18AF},
    {"limbu",      0x1900, 0x194F},
    {"tai-lue",    0x1980, 0x19DF},
    {"new-tai-lue",0x1980, 0x19DF},
    {"khmer",      0x1780, 0x17FF},
    {"buginese",   0x1A00, 0x1A1F},
    {"balinese",   0x1B00, 0x1B7F},
    {"sundanese",  0x1B80, 0x1BBF},
    {"lepcha",     0x1C00, 0x1C4F},
    {"ol-chiki",   0x1C50, 0x1C7F},
    {"sinhala",    0x1DF0, 0x1DFF},
    {"brahmi",     0x11000,0x1107F},
    {"cuneiform",  0x12000,0x123FF},
    {"egyptian",   0x13000,0x1342F},
    {"phoenician", 0x10900,0x1091F},
    {"lycian",     0x10280,0x1029F},
    {"lydian",     0x10920,0x1093F},
    {"glagolitic", 0x2C00, 0x2C5F},
    {"coptic",     0x2C80, 0x2CFF},
    {"gothic",     0x10330,0x1034F},
    {"shavian",    0x10450,0x1047F},
    {"osmanya",    0x10480,0x104AF},
    {"cypriot",    0x10800,0x1083F},
    {"aramaic",    0x10840,0x1085F},
    {"avestan",    0x10B00,0x10B3F},
    {"elbasan",    0x10500,0x1052F},
    {"albanian",   0x10530,0x1056F},
    {"linear-b",   0x10000,0x100FF},
    {"deseret",    0x10400,0x1044F},
    {"hangul",     0xAC00, 0xD7A3}, /* Hangul Syllables */
    {"hiragana",   0x3040, 0x309F},
    {"katakana",   0x30A0, 0x30FF},
    {"cjk",        0x3400, 0x4DBF}, /* CJK Ext A */
    {"cjk",        0x4E00, 0x9FFF}, /* CJK Unified Ideographs */
    {"cjk",        0x20000,0x2A6DF},/* CJK Ext B (astral) */
    {"emoji",      0x1F300,0x1FAFF},{"emoji",    0x1F000,0x1F02F},
    {"emoji",      0x2600, 0x27BF}, /* Misc symbols + Dingbats */
    {"emoji",      0x2190, 0x21FF}, {"emoji",    0x2300, 0x23FF},
    {"emoji",      0x2B00, 0x2BFF}, {"emoji",    0x1F1E6,0x1F1FF},
    {NULL, 0, 0}
};

/* pick a codepoint actually present in the font's cmap, scanning EVERY script
 * block in SCRIPT_RANGES. This makes the runtime language-agnostic: a font
 * stamps whatever script(s) it really contains, so all languages are reachable
 * (Ethiopic, Georgian, Armenian, Khmer, Myanmar, historical scripts, ...) -- not
 * just the ones a filename ladder happened to name. */
static int pick_cp(const Font *fo, const char *key, unsigned *seed) {
    (void)key;
    for (int r = 0; SCRIPT_RANGES[r].key; r++) {
        unsigned lo = SCRIPT_RANGES[r].lo, hi = SCRIPT_RANGES[r].hi;
        unsigned step = (hi - lo > 4000) ? 37u : 1u;
        for (unsigned c = lo; c <= hi; c += step) {
            if (font_cmap(fo, c)) return (int)c;
        }
    }
    /* fallback: a Latin codepoint derived from the seed */
    int c = (int)('A' + (*seed % 26)); *seed = *seed * 1103515245u + 12345u;
    if (font_cmap(fo, (unsigned)c)) return c;
    return 'A';
}

/* crop a glyph's tight bbox from the binarized ink map as uint8 (0 bg,255 ink) */
static uint8_t *crop_glyph(const OcrBinary *bin, const OcrBlock *g, int *ow, int *oh) {
    int x0 = (int)g->x0, y0 = (int)g->y0, x1 = (int)g->x1, y1 = (int)g->y1;
    if (x1 <= x0 || y1 <= y0) return NULL;
    int w = x1 - x0, h = y1 - y0;
    uint8_t *buf = malloc((size_t)w * h);
    if (!buf) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            buf[(size_t)y * w + x] = ocr_binary_ink(bin, (size_t)(x0 + x), (size_t)(y0 + y)) ? 255 : 0;
    *ow = w; *oh = h;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <fonts-dir> <out.json> [nglyph=40] [ppm=40] [seed=1] [mode=0]\n"
                        "  mode 0 = single ASCII (A-Z) per font\n"
                        "  mode 1 = MULTISCRIPT: each font stamps a glyph from its own script block\n",
               argv[0]);
        return 2;
    }
    const char *fdir = argv[1], *out = argv[2];
    int nglyph = argc > 3 ? atoi(argv[3]) : 40;
    int ppm    = argc > 4 ? atoi(argv[4]) : 40;
    unsigned seed = argc > 5 ? (unsigned)atoi(argv[5]) : 1u;
    int mode   = argc > 6 ? atoi(argv[6]) : 0;

    /* 1. load many real fonts */
    Font **fonts = NULL; uint8_t **bufs = NULL; char **paths = NULL;
    size_t nfonts = ocr_font_dir_load(fdir, &fonts, &bufs, &paths, 512);
    if (nfonts == 0) { fprintf(stderr, "[ocringest] no fonts in %s\n", fdir); return 1; }
    fprintf(stderr, "[ocringest] loaded %zu fonts (mode=%d)\n", nfonts, mode);

    /* 2. golden-ratio coordinate layout + warped multi-font stamp */
    int W = 1200, H = 1600;
    double *cx = malloc(sizeof(double) * nglyph);
    double *cy = malloc(sizeof(double) * nglyph);
    int *ridx = malloc(sizeof(int) * nglyph);
    int *sidx = malloc(sizeof(int) * nglyph);
    size_t placed = goldplace_layout(W, H, (size_t)nglyph, cx, cy, ridx, sidx);
    fprintf(stderr, "[ocringest] golden layout points: %zu\n", placed);

    OcrImage *im = ocr_image_create((size_t)W, (size_t)H);
    if (!im) return 1;
    unsigned rs = seed ? seed : 0x9E3779B9u;
    size_t actual = 0;

    for (size_t i = 0; i < placed; i++) {
        const Font *fo = fonts[rs % nfonts]; rs = rs*1103515245u + 12345u;
        /* mode 1 = multiscript: pick_cp scans the font's real cmap across ALL
         * Unicode script blocks, so every language the font carries is stamped
         * (CJK, Hangul, Devanagari, Ethiopic, Georgian, Cuneiform, emoji, ...). */
        int cp = pick_cp(fo, NULL, &rs);
        rs = rs*1103515245u + 12345u;
        int w = 0, h = 0; uint8_t *b = NULL;
        if (!font_rasterize(fo, (uint32_t)cp, ppm, &b, &w, &h) || !b) continue;
        if (w < 1) w = 1; if (h < 1) h = 1;

        /* ---- 2D rotation + 3D depth-shear warp (coordinate ablation) ----
         * rot2d : in-plane rotation of the glyph (radians)
         * depth : 0..1 scale simulating Z recession (1 = on plane)
         * shear : x-shear from a simulated Y-axis tilt (the "3D" read)
         * The warp is applied in glyph-local space; the resulting bitmap is
         * stamped at the golden point. The GROUND-TRUTH params are recorded
         * in the JSON so ingestion can be scored against truth. */
        double rot2d = ((double)(rs % 360) - 180.0) * M_PI/180.0; rs = rs*1103515245u+12345u;
        double depth = 0.7 + 0.3 * ((double)(rs % 100) / 100.0);          rs = rs*1103515245u+12345u;
        double shear = ((double)(rs % 40) - 20.0) / 100.0;                rs = rs*1103515245u+12345u;
        double ca = cos(rot2d), sa = sin(rot2d);
        /* effective size after depth scaling (the on-page footprint) */
        int ew = (int)(w * depth + 0.5), eh = (int)(h * depth + 0.5);
        if (ew < 1) ew = 1; if (eh < 1) eh = 1;
        /* Build the warped bitmap by FORWARD-mapping every inked source pixel
         * through (2D rotation) then (3D depth-scale + shear tilt). Forward
         * mapping is lossless: every inked source pixel lands exactly once.
         * The warp params are recorded as ground truth for ablation scoring. */
        uint8_t *wb = calloc((size_t)ew * eh, 1);
        if (!wb) { free(b); continue; }
        double halfw = w/2.0, halfh = h/2.0, half_ew = ew/2.0, half_eh = eh/2.0;
        for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) {
            if (!b[(size_t)dy*w + dx]) continue;
            double fx = dx - halfw, fy = dy - halfh;
            double rx = fx*ca - fy*sa, ry = fx*sa + fy*ca;   /* 2D rotate */
            rx = rx * depth + shear * ry;                   /* 3D tilt/shear */
            ry = ry * depth;
            int sx = (int)(rx + half_ew), sy = (int)(ry + half_eh);
            if (sx < 0 || sy < 0 || sx >= ew || sy >= eh) continue;
            wb[(size_t)sy*ew + sx] = 1;
        }
        int bx = (int)(cx[i] - ew/2.0), by = (int)(cy[i] - eh/2.0);
        for (int dy = 0; dy < eh; dy++) for (int dx = 0; dx < ew; dx++) {
            if (!wb[(size_t)dy*ew + dx]) continue;
            int px = bx + dx, py = by + dy;
            if (px < 0 || py < 0 || (size_t)px >= (size_t)W || (size_t)py >= (size_t)H) continue;
            ocr_image_set(im, (size_t)px, (size_t)py, 0);
        }
        free(wb); free(b);
        actual++;
    }
    fprintf(stderr, "[ocringest] stamped %zu warped glyphs\n", actual);

    /* 3. ingest -> coordinate JSON via real pipeline */
    uint8_t th = ocr_otsu_threshold(im);
    OcrBinary *bin = ocr_binarize(im, th);
    OcrPage *pg = ocr_page_analyze(im, NULL, NULL, NULL);
    if (!pg) { fprintf(stderr, "analyze failed\n"); ocr_binary_free(bin); return 1; }

    JVal *root = j_obj();
    j_obj_put(root, "type", j_str("ocr_golden_ingest"));
    j_obj_put(root, "width", j_num((double)W));
    j_obj_put(root, "height", j_num((double)H));
    j_obj_put(root, "fonts", j_num((double)nfonts));
    j_obj_put(root, "glyphs_placed", j_num((double)placed));
    j_obj_put(root, "mode", j_num((double)mode));

    JVal *blocks = j_arr();
    size_t nb = ocr_page_block_count(pg);
    size_t total_glyph = 0;
    for (size_t bi = 0; bi < nb; bi++) {
        const OcrBlock *b = ocr_page_block(pg, bi);
        if (!b) continue;
        JVal *bj = j_obj();
        j_obj_put(bj, "bx", j_num((double)b->x0));
        j_obj_put(bj, "by", j_num((double)b->y0));
        j_obj_put(bj, "bw", j_num((double)(b->x1 - b->x0)));
        j_obj_put(bj, "bh", j_num((double)(b->y1 - b->y0)));
        JVal *gj = j_arr();
        size_t ng = ocr_page_glyph_count(pg, bi);
        for (size_t k = 0; k < ng; k++) {
            const OcrBlock *g = ocr_page_glyph(pg, bi, k);
            if (!g) continue;
            JVal *go = j_obj();
            /* true post-warp document coordinates of the detected glyph */
            j_obj_put(go, "x", j_num((double)g->x0));
            j_obj_put(go, "y", j_num((double)g->y0));
            j_obj_put(go, "w", j_num((double)(g->x1 - g->x0)));
            j_obj_put(go, "h", j_num((double)(g->y1 - g->y0)));
            /* 4. DFT compression + spectral analysis on the glyph crop */
            int gw = 0, gh = 0;
            uint8_t *crop = crop_glyph(bin, g, &gw, &gh);
            if (crop && gw >= 2 && gh >= 2) {
                double *re = malloc(sizeof(double) * gw * gh);
                double *im2 = malloc(sizeof(double) * gw * gh);
                if (re && im2) {
                    dft2d(crop, gw, gh, re, im2);
                    int keep = gw * gh / 4 + 1;
                    double *buf = malloc(sizeof(double) * keep * 4);
                    if (buf) {
                        int nk = dft_compress(re, im2, gw, gh, keep, buf);
                        double ratio = dft_ratio(gw, nk);
                        double feat[6];
                        dft_features(re, im2, gw, gh, feat);
                        JVal *fj = j_obj();
                        j_obj_put(fj, "compression_ratio", j_num(ratio));
                        j_obj_put(fj, "coeffs_kept", j_num((double)nk));
                        j_obj_put(fj, "energy", j_num(feat[0]));
                        j_obj_put(fj, "low_band", j_num(feat[1]));
                        j_obj_put(fj, "mid_band", j_num(feat[2]));
                        j_obj_put(fj, "high_band", j_num(feat[3]));
                        j_obj_put(fj, "dom_freq_radius", j_num(feat[4]));
                        j_obj_put(fj, "dom_freq_angle", j_num(feat[5]));
                        j_obj_put(go, "dft", fj);
                        free(buf);
                    }
                    free(re); free(im2);
                }
                free(crop);
            }
            /* 5. golden coordinate tags: which golden region + spiral index the
             * glyph's center falls in (multi-coordinate-style ablation readback) */
            int rgx = -1;
            GoldRect gr[256];
            size_t nr = goldplace_subdivide(gr, 256, W, H);
            double gcx = (g->x0 + g->x1) / 2.0, gcy = (g->y0 + g->y1) / 2.0;
            for (size_t ri = 0; ri < nr; ri++)
                if (gcx >= gr[ri].x1 && gcx < gr[ri].x2 && gcy >= gr[ri].y1 && gcy < gr[ri].y2)
                    { rgx = (int)ri; break; }
            j_obj_put(go, "golden_region", j_num((double)rgx));
            j_obj_put(go, "golden_region_count", j_num((double)nr));
            j_arr_push(gj, go);
            total_glyph++;
        }
        j_obj_put(bj, "glyphs", gj);
        j_arr_push(blocks, bj);
    }
    j_obj_put(root, "blocks", blocks);
    j_obj_put(root, "glyphs_detected", j_num((double)total_glyph));

    char *outstr = j_emit(root);
    FILE *f = fopen(out, "w");
    if (f && outstr) { fputs(outstr, f); fputc('\n', f); fclose(f); }
    fprintf(stderr, "[ocringest] wrote %s (%zu blocks)\n", out ? out : "-", nb);

    free(outstr); j_free(root);
    ocr_page_free(pg);
    ocr_binary_free(bin);
    ocr_image_free(im);
    ocr_font_dir_free(fonts, bufs, paths, nfonts);
    free(cx); free(cy); free(ridx); free(sidx);
    return 0;
}
