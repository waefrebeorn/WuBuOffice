/* wubuocr main -- image (Netpbm) -> structured document JSON.
 *
 * The image-ingestion front-end for wubuOS document digestion:
 *   wubuocr_cli page.pgm                 -> ocr_page JSON (layout + recognized text)
 *   wubuocr_cli page.pgm --doc           -> wubudoc "document" model JSON
 *   wubuocr_cli page.pgm --geom          -> geometry only (no recognition)
 *   wubuocr_cli page.pgm --bank          -> multi-font template bank (study
 *                                                    many font types)
 *
 * Recognition uses the built-in zoning + 1-NN classifier (font8x8 templates)
 * by default, or a MULTI-FONT bank built from every real system font it can
 * open (--bank): each glyph is classified by cumulative confidence across all
 * fonts, which is far more robust than any single font. The classifier never
 * fabricates: ambiguous blobs fall below the confidence gate and stay empty.
 */
#include "wubuocr.h"
#include "recognize.h"
#include "fontbank.h"
#include "wubufont.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Candidate system fonts the --bank mode tries to build a bank from. */
static const char *bank_font_paths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/opentype/unifont/unifont.otf",
    "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
    "/mnt/c/Windows/Fonts/arial.ttf",
    NULL
};

static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f); rewind(f);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = 0;
    *n = rd;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <image.pbm|pgm|ppm> [--doc] [--geom] [--bank]\n", argv[0]);
        return 2;
    }
    int as_doc = 0, geom = 0, use_bank = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--doc") == 0) as_doc = 1;
        else if (strcmp(argv[i], "--geom") == 0) geom = 1;
        else if (strcmp(argv[i], "--bank") == 0) use_bank = 1;
    }

    size_t n = 0;
    uint8_t *data = slurp(argv[1], &n);
    if (!data) { fprintf(stderr, "error: cannot read %s\n", argv[1]); return 1; }

    OcrTemplates *tmpl = NULL;
    OcrFontBank  *bank = NULL;
    OcrRecognizer rec = NULL;
    void *user = NULL;

    if (!geom) {
        if (use_bank) {
            /* Collect every system font we can open and build the bank. */
            const void *fonts[OCR_FONTBANK_MAX];
            uint8_t *bufs[OCR_FONTBANK_MAX];
            Font *objs[OCR_FONTBANK_MAX];
            size_t nf = 0;
            for (int i = 0; bank_font_paths[i] && nf < OCR_FONTBANK_MAX; i++) {
                size_t bn = 0;
                uint8_t *b = slurp(bank_font_paths[i], &bn);
                if (!b) continue;
                Font *fo = font_open(b, bn);
                if (!fo) { free(b); continue; }
                bufs[nf] = b; objs[nf] = fo; fonts[nf] = fo; nf++;
            }
            if (nf) {
                bank = ocr_fontbank_build_english(fonts, nf, 5, 48);
                if (bank) { rec = ocr_fontbank_recognizer(); user = bank; }
                else for (size_t i = 0; i < nf; i++) { font_free(objs[i]); free(bufs[i]); }
            } else {
                fprintf(stderr, "warning: --bank: no system font found; "
                                "falling back to built-in 8x8 templates\n");
            }
        }
        if (!rec) {                 /* default single-font recognizer */
            tmpl = ocr_templates_create(5);   /* 5x5 zoning grid */
            if (tmpl) { rec = ocr_recognizer_fn(); user = tmpl; }
        }
    }

    OcrPage *pg = ocr_page_from_netpbm(data, n, rec, user);
    free(data);
    ocr_templates_free(tmpl);
    ocr_fontbank_free(bank);
    if (!pg) { fprintf(stderr, "error: not a valid Netpbm image or analysis failed\n"); return 1; }

    char *json = as_doc ? ocr_page_to_docmodel_json(pg) : ocr_page_to_json(pg);
    ocr_page_free(pg);
    if (!json) { fprintf(stderr, "error: serialization failed\n"); return 1; }

    fputs(json, stdout);
    fputc('\n', stdout);
    free(json);
    return 0;
}
