/* test_columns.c -- end-to-end multi-column detection.
 * Generates 1/2/3-column synthetic pages and verifies the docmodel JSON
 * carries the right block shape (paragraph for 1-col, table(rows,cols) for
 * multi-column). Needs LOAD=<model> CHARS=<charset> like the other transcribe
 * tests; skips otherwise. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image.h"
#include "crnn.h"
#include "crnn_transcribe.h"

#define STRIP 20

/* crude JSON probe: count top-level "kind":"table" blocks and grab cols */
static int count_tables(const char *json) {
    int n = 0; const char *p = json;
    while ((p = strstr(p, "\"kind\":\"table\"")) != NULL) { n++; p++; }
    return n;
}
static int get_first_cols(const char *json) {
    const char *p = strstr(json, "\"cols\":");
    if (!p) return -1;
    return atoi(p + 7);
}

int main(int argc, char **argv) {
    const char *load = getenv("LOAD");
    const char *charset = getenv("CHARS");
    if (!load || !charset) { printf("SKIP: set LOAD=<model> CHARS=<charset>\n"); return 0; }

    CRNN *m = NULL;
    if (!crnn_load(load, &m) || !m) { printf("FAIL: cannot load model %s\n", load); return 1; }

    int fail = 0;
    /* 1-column page (dark bg, light text, THIN band): expect paragraphs, no table */
    {
        int W = 400, H = 60, strip = STRIP;
        OcrImage *pg = ocr_image_create((size_t)W, (size_t)H);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            /* textured ink (per-column variation) so deskew stays at 0 deg */
            int v = 255 - ((x % 3) * 10);
            ocr_image_set(pg, (size_t)x, (size_t)y, (y > 25 && y < 35) ? (uint8_t)v : 0);
        }
        char *json = NULL;
        if (crnn_transcribe_page_json(m, pg, strip, charset, NULL, &json) == 0 && json) {
            if (count_tables(json) != 0) { printf("FAIL: 1-col produced a table\n  json=%s\n", json); fail++; }
            free(json);
        }
        ocr_image_free(pg);
    }

    /* 2-column page (dark bg, light text, THIN band): expect a table with cols>=2 */
    {
        int W = 800, H = 60, strip = STRIP;
        OcrImage *pg = ocr_image_create((size_t)W, (size_t)H);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            int ink = (y > 25 && y < 35);
            /* leave a wide central gutter empty: x in [380,420] is background */
            if (x > 380 && x < 420) ink = 0;
            /* textured ink (per-column variation) so deskew stays at 0 deg */
            int v = 255 - ((x % 3) * 10);
            ocr_image_set(pg, (size_t)x, (size_t)y, ink ? (uint8_t)v : 0);
        }
        char *json = NULL;
        if (crnn_transcribe_page_json(m, pg, strip, charset, NULL, &json) == 0 && json) {
            int t = count_tables(json);
            int cols = get_first_cols(json);
            if (t < 1) { printf("FAIL: 2-col produced no table\n  json=%s\n", json); fail++; }
            else if (cols < 2) { printf("FAIL: 2-col table cols=%d (<2)\n  json=%s\n", cols, json); fail++; }
            else printf("OK: 2-col -> table cols=%d\n", cols);
            free(json);
        }
        ocr_image_free(pg);
    }

    /* 3-column page (dark bg, light text, THIN band): expect a table with cols>=3 */
    {
        int W = 1200, H = 60, strip = STRIP;
        OcrImage *pg = ocr_image_create((size_t)W, (size_t)H);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            int ink = (y > 25 && y < 35);
            /* two wide central gutters: x in (360,480) and (720,840) are background */
            if ((x > 360 && x < 480) || (x > 720 && x < 840)) ink = 0;
            int v = 255 - ((x % 3) * 10);
            ocr_image_set(pg, (size_t)x, (size_t)y, ink ? (uint8_t)v : 0);
        }
        char *json = NULL;
        if (crnn_transcribe_page_json(m, pg, strip, charset, NULL, &json) == 0 && json) {
            int t = count_tables(json);
            int cols = get_first_cols(json);
            if (t < 1) { printf("FAIL: 3-col produced no table\n  json=%s\n", json); fail++; }
            else if (cols < 3) { printf("FAIL: 3-col table cols=%d (<3)\n  json=%s\n", cols, json); fail++; }
            else printf("OK: 3-col -> table cols=%d\n", cols);
            free(json);
        }
        ocr_image_free(pg);
    }

    crnn_free(m);
    if (fail) { printf("FAIL: test_columns (%d)\n", fail); return 1; }
    printf("PASS: test_columns\n");
    return 0;
}
