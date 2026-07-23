/* crnn_transcribe.c -- page -> line segmentation -> CRNN -> docmodel JSON.
 * See crnn_transcribe.h. C11, no deps beyond wubuocr core. */
#include "crnn_transcribe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A pixel is "ink" when it deviates strongly from the page background. This
 * is adaptive: synthetic test pages use a dark background (low value) with
 * light text, while real scans/photos use a light background with dark text.
 * Either way, ink = |g - bg| > MARGIN. */
#define INK_MARGIN 40
static int is_ink(uint8_t g, int bg) { return g > bg ? (g - bg) > INK_MARGIN : (bg - g) > INK_MARGIN; }

/* Growable byte-append: realloc (doubling) as needed, then copy `n` bytes
 * from `s`. Never uses strcat on a possibly-reallocated pointer. */
static int ba_append(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
    if (n == 0) return 0;
    if (*len + n + 1 > *cap) {
        size_t ncap = *cap ? *cap : 64;
        while (*len + n + 1 > ncap) ncap *= 2;
        char *nb = realloc(*buf, ncap);
        if (!nb) return -1;
        *buf = nb; *cap = ncap;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
    return 0;
}

static int ba_append_str(char **buf, size_t *len, size_t *cap, const char *s) {
    return ba_append(buf, len, cap, s, strlen(s));
}

/* Append a JSON-escaped copy of `s`. */
static int ba_append_json_escaped(char **buf, size_t *len, size_t *cap, const char *s) {
    for (const char *p = s; *p; p++) {
        char lit[8];
        const char *esc = NULL;
        size_t elen = 1;
        switch (*p) {
            case '"':  esc = "\\\""; elen = 2; break;
            case '\\': esc = "\\\\"; elen = 2; break;
            case '\n': esc = "\\n";  elen = 2; break;
            case '\r': esc = "\\r";  elen = 2; break;
            case '\t': esc = "\\t";  elen = 2; break;
            case '\b': esc = "\\b";  elen = 2; break;
            case '\f': esc = "\\f";  elen = 2; break;
            default: lit[0] = *p; esc = lit; elen = 1; break;
        }
        if (ba_append(buf, len, cap, esc, elen) != 0) return -1;
    }
    return 0;
}

/* Build a strip-tall (height == `strip`) line image from the page band
 * [ry0, ry0+band_h). Vertical scaling is nearest-neighbour so the conv trunk
 * (which slices exactly `strip`-tall windows) receives correctly-proportioned
 * ink; the CRNN is width-invariant, so only the height must match `strip`. */
static OcrImage *normalize_line(const OcrImage *page, int ry0, int band_h, int strip) {
    int W = (int)ocr_image_width(page);
    OcrImage *line = ocr_image_create((size_t)W, (size_t)strip);
    if (!line) return NULL;
    if (band_h < 1) band_h = 1;
    for (int y = 0; y < strip; y++) {
        int sy = ry0 + (int)((long)y * band_h / strip);
        for (int x = 0; x < W; x++) {
            ocr_image_set(line, (size_t)x, (size_t)y, ocr_image_get(page, (size_t)x, (size_t)sy));
        }
    }
    return line;
}

int crnn_transcribe_page_json(CRNN *m, const OcrImage *page,
                              int strip, const char *charset,
                              char **out_json) {
    if (out_json) *out_json = NULL;
    if (!m || !page || strip < 1 || !charset) return -1;

    int W = (int)ocr_image_width(page);
    int H = (int)ocr_image_height(page);
    if (W < 1 || H < 1) return -1;

    /* ---- adaptive background: median intensity of the page ----
     * Synthetic test pages paint a dark background; real scans/photos paint a
     * light one. Take the median pixel value as the background estimate. */
    int bg = 128;
    {
        size_t hist[256]; memset(hist, 0, sizeof hist);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                hist[ocr_image_get(page, (size_t)x, (size_t)y)]++;
        size_t half = (size_t)W * H / 2, acc = 0;
        for (int v = 0; v < 256; v++) { acc += hist[v]; if (acc >= half) { bg = v; break; } }
    }

    /* ---- horizontal ink-projection: mark rows that contain any ink ---- */
    char *row_ink = malloc((size_t)H);
    if (!row_ink) return -1;
    for (int y = 0; y < H; y++) {
        int ink = 0;
        for (int x = 0; x < W; x++) if (is_ink(ocr_image_get(page, (size_t)x, (size_t)y), bg)) { ink = 1; break; }
        row_ink[y] = (char)ink;
    }

    /* ---- growable JSON buffer ---- */
    char *buf = malloc(64); size_t len = 0, cap = 64;
    if (!buf) { free(row_ink); return -1; }
    if (ba_append_str(&buf, &len, &cap, "{\"blocks\":[") != 0) {
        free(buf); free(row_ink); return -1;
    }

    int y = 0, nlines = 0;
    int first = 1;
    while (y < H) {
        while (y < H && !row_ink[y]) y++;
        if (y >= H) break;
        int y0 = y;
        while (y < H && row_ink[y]) y++;
        int y1 = y;                 /* [y0, y1) is the ink band */
        int band_h = y1 - y0;

        OcrImage *line = normalize_line(page, y0, band_h, strip);
        char pred[512];
        if (line) {
            crnn_recognize(m, line, charset, pred, sizeof pred);
            ocr_image_free(line);
        } else {
            pred[0] = '\0';
        }

        /* append {"text":"..."} */
        if (!first) { if (ba_append_str(&buf, &len, &cap, ",") != 0) break; }
        first = 0;
        if (ba_append_str(&buf, &len, &cap, "{\"kind\":\"paragraph\",\"text\":\"") != 0) break;
        if (ba_append_json_escaped(&buf, &len, &cap, pred) != 0) break;
        if (ba_append_str(&buf, &len, &cap, "\"}") != 0) break;
        nlines++;
    }

    int rc = 0;
    if (ba_append_str(&buf, &len, &cap, "]}") != 0) rc = -1;

    free(row_ink);

    if (rc != 0) { free(buf); return -1; }
    if (out_json) *out_json = buf;
    (void)nlines;
    return 0;
}
