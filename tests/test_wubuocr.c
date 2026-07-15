/* test_wubuocr.c -- image->document pipeline: decode, Otsu, XY-cut reading
 * order, connected components, and JSON model shape.
 *
 * The load-bearing assertion is the two-column reading-order test: a synthetic
 * page with a full-width headline over two text columns must yield blocks in
 * the order headline -> left column -> right column. That is precisely the
 * multi-column reading-order failure that plagues raster OCR (pain point #1).
 */
#include "wubuocr.h"
#include "image.h"
#include "binarize.h"
#include "layout.h"
#include "components.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", (msg)); fails++; } } while (0)

/* Paint a solid filled rectangle of ink (value 0) into a grayscale image. */
static void fill_rect(OcrImage *im, size_t x0, size_t y0, size_t x1, size_t y1) {
    for (size_t y = y0; y < y1; y++)
        for (size_t x = x0; x < x1; x++)
            ocr_image_set(im, x, y, 0);
}

/* Does a block roughly cover [x0,x1)x[y0,y1)? (centroid inside the target) */
static int block_in(const OcrBlock *b, size_t x0, size_t y0, size_t x1, size_t y1) {
    size_t cx = (b->x0 + b->x1) / 2, cy = (b->y0 + b->y1) / 2;
    return cx >= x0 && cx < x1 && cy >= y0 && cy < y1;
}

int main(void) {
    /* ---------- 1. Netpbm decode: ASCII PGM ---------- */
    {
        const char *pgm = "P2\n4 2\n255\n0 255 128 64\n200 100 50 25\n";
        OcrImage *im = ocr_image_from_netpbm((const uint8_t*)pgm, strlen(pgm));
        CK(im != NULL, "decode ASCII PGM");
        if (im) {
            CK(ocr_image_width(im) == 4 && ocr_image_height(im) == 2, "PGM dims");
            CK(ocr_image_get(im, 0, 0) == 0, "PGM px(0,0)=0");
            CK(ocr_image_get(im, 1, 0) == 255, "PGM px(1,0)=255");
            CK(ocr_image_get(im, 0, 1) == 200, "PGM px(0,1)=200");
            ocr_image_free(im);
        }
    }

    /* ---------- 2. Netpbm decode: binary PBM (bit-packed, 1=black) ---------- */
    {
        /* 8x1: 10000001 -> ink at x=0 and x=7 */
        const uint8_t pbm[] = { 'P','4','\n','8',' ','1','\n', 0x81 };
        OcrImage *im = ocr_image_from_netpbm(pbm, sizeof pbm);
        CK(im != NULL, "decode binary PBM");
        if (im) {
            CK(ocr_image_get(im, 0, 0) == 0, "PBM bit7 -> ink(black)");
            CK(ocr_image_get(im, 1, 0) == 255, "PBM bit6 -> white");
            CK(ocr_image_get(im, 7, 0) == 0, "PBM bit0 -> ink(black)");
            ocr_image_free(im);
        }
    }

    /* ---------- 3. Otsu threshold on a clean bimodal image ---------- */
    {
        /* half pixels at 20 (dark), half at 230 (light): threshold between */
        OcrImage *im = ocr_image_create(10, 10);
        for (size_t y = 0; y < 10; y++)
            for (size_t x = 0; x < 10; x++)
                ocr_image_set(im, x, y, (x < 5) ? 20 : 230);
        uint8_t t = ocr_otsu_threshold(im);
        CK(t >= 20 && t < 230, "Otsu threshold lies between the two modes");
        OcrBinary *b = ocr_binarize(im, t);
        CK(b != NULL, "binarize");
        if (b) {
            CK(ocr_binary_ink(b, 0, 0) == 1, "dark side is ink");
            CK(ocr_binary_ink(b, 9, 0) == 0, "light side is background");
            ocr_binary_free(b);
        }
        ocr_image_free(im);
    }

    /* ---------- 4. XY-cut reading order: headline over two columns ---------- */
    {
        /* 200x200 white page.
         * headline: full-width band y[10,30)
         * left column: x[10,80) y[60,180)
         * right column: x[120,190) y[60,180)
         * wide vertical gutter x[80,120) separates columns;
         * wide horizontal gutter y[30,60) separates headline from columns. */
        OcrImage *im = ocr_image_create(200, 200);
        fill_rect(im, 10, 10, 190, 30);     /* headline (full width) */
        fill_rect(im, 10, 60, 80, 180);     /* left column */
        fill_rect(im, 120, 60, 190, 180);   /* right column */

        OcrPage *pg = ocr_page_analyze(im, NULL, NULL, NULL);
        CK(pg != NULL, "analyze two-column page");
        if (pg) {
            size_t nb = ocr_page_block_count(pg);
            CK(nb == 3, "detected exactly 3 blocks (headline + 2 columns)");
            if (nb == 3) {
                const OcrBlock *b0 = ocr_page_block(pg, 0);
                const OcrBlock *b1 = ocr_page_block(pg, 1);
                const OcrBlock *b2 = ocr_page_block(pg, 2);
                /* reading order MUST be: headline, then left col, then right col */
                CK(block_in(b0, 0, 0, 200, 50), "block[0] is the headline (top band)");
                CK(block_in(b1, 0, 50, 100, 200), "block[1] is the LEFT column");
                CK(block_in(b2, 100, 50, 200, 200), "block[2] is the RIGHT column");
                /* the discriminating check: left column precedes right column */
                CK(b1->x0 < b2->x0, "reading order: left column before right column");
            }
            ocr_page_free(pg);
        }
        ocr_image_free(im);
    }

    /* ---------- 5. Connected components: 3 separated ink squares ---------- */
    {
        OcrImage *im = ocr_image_create(100, 30);
        fill_rect(im, 5, 5, 15, 25);
        fill_rect(im, 40, 5, 50, 25);
        fill_rect(im, 75, 5, 85, 25);
        OcrBinary *b = ocr_binarize(im, ocr_otsu_threshold(im));
        CK(b != NULL, "binarize for components");
        if (b) {
            OcrComponents *cc = ocr_components(b, 0, 0, 100, 30, 5);
            CK(cc != NULL, "components run");
            if (cc) {
                CK(ocr_components_count(cc) == 3, "found 3 components");
                if (ocr_components_count(cc) == 3) {
                    /* reading order left-to-right on one line band */
                    const OcrBlock *c0 = ocr_components_box(cc, 0);
                    const OcrBlock *c1 = ocr_components_box(cc, 1);
                    const OcrBlock *c2 = ocr_components_box(cc, 2);
                    CK(c0->x0 < c1->x0 && c1->x0 < c2->x0,
                       "components ordered left-to-right");
                }
                ocr_components_free(cc);
            }
            ocr_binary_free(b);
        }
        ocr_image_free(im);
    }

    /* ---------- 6. JSON model shape + docmodel bridge ---------- */
    {
        OcrImage *im = ocr_image_create(80, 40);
        fill_rect(im, 10, 10, 70, 30);
        OcrPage *pg = ocr_page_analyze(im, NULL, NULL, NULL);
        CK(pg != NULL, "analyze single-block page");
        if (pg) {
            char *js = ocr_page_to_json(pg);
            CK(js && strstr(js, "\"type\":\"ocr_page\"") != NULL, "ocr_page json type");
            CK(js && strstr(js, "\"blocks\"") != NULL, "ocr_page has blocks");
            CK(js && strstr(js, "\"glyphs\"") != NULL, "ocr_page has glyphs array");
            free(js);
            /* geometry-only: text must be empty, NEVER fabricated */
            char *dj = ocr_page_to_docmodel_json(pg);
            CK(dj && strstr(dj, "\"type\":\"document\"") != NULL, "docmodel json type");
            CK(dj && strstr(dj, "\"kind\":\"paragraph\"") != NULL, "docmodel paragraph");
            CK(dj && strstr(dj, "\"text\":\"\"") != NULL, "geometry-only: empty text (no fabrication)");
            free(dj);
            ocr_page_free(pg);
        }
        ocr_image_free(im);
    }

    if (fails) { printf("WUBUOCR TESTS FAILED (%d)\n", fails); return 1; }
    printf("WUBUOCR TESTS PASSED\n");
    return 0;
}
