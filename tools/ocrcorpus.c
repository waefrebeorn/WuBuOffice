/* ocrcorpus.c -- generate a multi-script LABELED glyph corpus for training.
 *
 * The user's "wide good source data we do our modules on": for every loaded
 * font (across many scripts -- CJK, JP, KR, Devanagari, Bengali, Tamil, Telugu,
 * Thai, Arabic, Cyrillic, Latin/Hispanic) rasterize each glyph in its script
 * block at a fixed ppm, emit a clean crop (tight bbox) as a 28x28-normalized
 * sample with: utf8 codepoint label, script tag, DFT compression ratio +
 * spectral features. Output: JSONL (one line per glyph) -- trivially convertible
 * to IDX or fed straight to the conv/MLP trainer.
 *
 * This is the DATA half of the multi-font advantage: the recognizer (conv3 +
 * MLP, or the zoning 1-NN) trains on this wide, multi-script, multi-font source
 * so it learns style- and script-invariant glyph shapes.
 *
 * Dependency-free C11 (links wubuocr + wubufont + wubujson + dft).
 * Usage:
 *   ocrcorpus <fonts-dir> <out.jsonl> [ppm=40] [per-font-cap=200] [maxfonts=64]
 */
#include "wubuocr.h"
#include "image.h"
#include "binarize.h"
#include "fontdir.h"
#include "wubufont.h"
#include "dft.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <math.h>

/* script block per font basename token (mirrors ocringest) */
static void script_range(const char *bn, unsigned *lo, unsigned *hi, const char **tag) {
    *lo = 0x41; *hi = 0x5B; *tag = "latin";
    if (strstr(bn,"ChineseSC")||strstr(bn,"ChineseTC")||strstr(bn,"Japanese")||strstr(bn,"JP")) { *lo=0x4E00; *hi=0x9FFF; *tag="cjk"; }
    else if (strstr(bn,"Korean")||strstr(bn,"KR")) { *lo=0xAC00; *hi=0xD7A4; *tag="hangul"; }
    else if (strstr(bn,"Devanagari")) { *lo=0x0900; *hi=0x097F; *tag="devanagari"; }
    else if (strstr(bn,"Bengali")) { *lo=0x0980; *hi=0x09FF; *tag="bengali"; }
    else if (strstr(bn,"Tamil")) { *lo=0x0B80; *hi=0x0BFF; *tag="tamil"; }
    else if (strstr(bn,"Telugu")) { *lo=0x0C00; *hi=0x0C7F; *tag="telugu"; }
    else if (strstr(bn,"Thai")) { *lo=0x0E00; *hi=0x0E7F; *tag="thai"; }
    else if (strstr(bn,"Arabic")) { *lo=0x0600; *hi=0x06FF; *tag="arabic"; }
    else if (strstr(bn,"Cyrillic")) { *lo=0x0400; *hi=0x04FF; *tag="cyrillic"; }
    else if (strstr(bn,"Hispanic")||strstr(bn,"Display")) { *tag="latin"; }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fonts-dir> <out.jsonl> [ppm=40] [cap=200] [maxfonts=64]\n", argv[0]); return 2; }
    const char *fdir = argv[1], *out = argv[2];
    int ppm = argc > 3 ? atoi(argv[3]) : 40;
    int cap = argc > 4 ? atoi(argv[4]) : 200;
    int maxf = argc > 5 ? atoi(argv[5]) : 64;

    Font **fonts = NULL; uint8_t **bufs = NULL; char **paths = NULL;
    size_t nfonts = ocr_font_dir_load(fdir, &fonts, &bufs, &paths, (size_t)maxf);
    if (nfonts == 0) { fprintf(stderr, "no fonts\n"); return 1; }

    FILE *fo = fopen(out, "w");
    if (!fo) { fprintf(stderr, "cannot write %s\n", out); return 1; }

    size_t total = 0;
    for (size_t fi = 0; fi < nfonts; fi++) {
        const char *bn = paths ? paths[fi] : "font";
        unsigned lo, hi; const char *tag;
        script_range(bn, &lo, &hi, &tag);
        int count = 0;
        for (unsigned cp = lo; cp < hi && count < cap; cp++) {
            if (!font_cmap(fonts[fi], cp)) continue;
            int w = 0, h = 0; uint8_t *b = NULL;
            if (!font_rasterize(fonts[fi], cp, ppm, &b, &w, &h) || !b) continue;
            if (w < 2 || h < 2) { free(b); continue; }
            /* tight ink bbox */
            int x0=w,y0=h,x1=0,y1=0, ink=0;
            for (int y=0;y<h;y++) for (int x=0;x<w;x++) if (b[(size_t)y*w+x]) {
                if(x<x0)x0=x; if(x>x1)x1=x; if(y<y0)y0=y; if(y>y1)y1=y; ink++;
            }
            if (ink < 3) { free(b); continue; }
            int cw = x1-x0+1, ch = y1-y0+1;
            /* crop to tight bbox, run DFT compression + features */
            double *re = malloc(sizeof(double)*cw*ch), *im2 = malloc(sizeof(double)*cw*ch);
            uint8_t *crop = malloc((size_t)cw*ch);
            for (int y=0;y<ch;y++) for (int x=0;x<cw;x++) crop[(size_t)y*cw+x] = b[(size_t)(y0+y)*w+(x0+x)];
            dft2d(crop, cw, ch, re, im2);
            int keep = cw*ch/4+1; double *buf = malloc(sizeof(double)*keep*4);
            int nk = dft_compress(re, im2, cw, ch, keep, buf);
            double feat[6]; dft_features(re, im2, cw, ch, feat);
            /* emit one JSONL record */
            char u8[5]; int n = wctomb(u8, (wchar_t)cp); if (n<1){u8[0]='?';n=1;} u8[n]=0;
            fprintf(fo, "{\"cp\":%u,\"utf8\":\"%s\",\"script\":\"%s\",\"font\":\"%s\","
                         "\"w\":%d,\"h\":%d,\"ink\":%d,\"dft_ratio\":%.4f,\"coeffs\":%d,"
                         "\"low\":%.4f,\"mid\":%.4f,\"high\":%.4f,\"dom_r\":%.3f,\"dom_a\":%.3f}\n",
                    cp, u8, tag, bn, cw, ch, ink, dft_ratio(cw,nk), nk,
                    feat[1], feat[2], feat[3], feat[4], feat[5]);
            free(re); free(im2); free(buf); free(crop); free(b);
            count++; total++;
        }
        fprintf(stderr, "[ocrcorpus] %s (%s): %d glyphs\n", bn, tag, count);
    }
    fclose(fo);
    fprintf(stderr, "[ocrcorpus] TOTAL %zu labeled glyphs -> %s\n", total, out);
    ocr_font_dir_free(fonts, bufs, paths, nfonts);
    return 0;
}
