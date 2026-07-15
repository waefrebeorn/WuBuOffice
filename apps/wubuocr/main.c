/* wubuocr main -- image (Netpbm) -> structured document JSON.
 *
 * The image-ingestion front-end for wubuOS document digestion:
 *   wubuocr_cli page.pgm                 -> ocr_page JSON (layout + recognized text)
 *   wubuocr_cli page.pgm --doc           -> wubudoc "document" model JSON
 *   wubuocr_cli page.pgm --geom          -> geometry only (no recognition)
 *
 * Recognition uses the built-in zoning + 1-NN classifier (font8x8 templates);
 * pass --geom for honest bounding boxes with empty text. The classifier never
 * fabricates: ambiguous blobs fall below the confidence gate and stay empty.
 */
#include "wubuocr.h"
#include "recognize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        fprintf(stderr, "usage: %s <image.pbm|pgm|ppm> [--doc] [--geom]\n", argv[0]);
        return 2;
    }
    int as_doc = 0, geom = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--doc") == 0) as_doc = 1;
        else if (strcmp(argv[i], "--geom") == 0) geom = 1;
    }

    size_t n = 0;
    uint8_t *data = slurp(argv[1], &n);
    if (!data) { fprintf(stderr, "error: cannot read %s\n", argv[1]); return 1; }

    OcrTemplates *tmpl = NULL;
    OcrRecognizer rec = NULL;
    void *user = NULL;
    if (!geom) {
        tmpl = ocr_templates_create(5);   /* 5x5 zoning grid */
        if (tmpl) { rec = ocr_recognizer_fn(); user = tmpl; }
    }

    OcrPage *pg = ocr_page_from_netpbm(data, n, rec, user);
    free(data);
    ocr_templates_free(tmpl);
    if (!pg) { fprintf(stderr, "error: not a valid Netpbm image or analysis failed\n"); return 1; }

    char *json = as_doc ? ocr_page_to_docmodel_json(pg) : ocr_page_to_json(pg);
    ocr_page_free(pg);
    if (!json) { fprintf(stderr, "error: serialization failed\n"); return 1; }

    fputs(json, stdout);
    fputc('\n', stdout);
    free(json);
    return 0;
}
