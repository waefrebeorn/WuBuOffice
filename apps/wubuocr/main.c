/* wubuocr main -- image (Netpbm) -> structured document JSON.
 *
 * The image-ingestion front-end for wubuOS document digestion:
 *   wubuocr_cli page.pgm                 -> ocr_page JSON (layout + glyph boxes)
 *   wubuocr_cli page.pgm --doc           -> wubudoc "document" model JSON
 *
 * Geometry-only by default (no recognizer linked): honest empty text with real
 * bounding boxes. A recognizer plug-in fills the text without changing this CLI.
 */
#include "wubuocr.h"

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
        fprintf(stderr, "usage: %s <image.pbm|pgm|ppm> [--doc]\n", argv[0]);
        return 2;
    }
    int as_doc = (argc >= 3 && strcmp(argv[2], "--doc") == 0);

    size_t n = 0;
    uint8_t *data = slurp(argv[1], &n);
    if (!data) { fprintf(stderr, "error: cannot read %s\n", argv[1]); return 1; }

    OcrPage *pg = ocr_page_from_netpbm(data, n, NULL, NULL);
    free(data);
    if (!pg) { fprintf(stderr, "error: not a valid Netpbm image or analysis failed\n"); return 1; }

    char *json = as_doc ? ocr_page_to_docmodel_json(pg) : ocr_page_to_json(pg);
    ocr_page_free(pg);
    if (!json) { fprintf(stderr, "error: serialization failed\n"); return 1; }

    fputs(json, stdout);
    fputc('\n', stdout);
    free(json);
    return 0;
}
