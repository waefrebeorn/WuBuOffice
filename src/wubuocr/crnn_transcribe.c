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

/* Count ink pixels in row `y` that are 8-connected to another ink pixel
 * (any of the 8 neighbours is ink). Real glyph strokes -- even thin vertical
 * stems -- are connected, so their pixels count. Salt-and-pepper noise is
 * single isolated pixels with no ink neighbour, so it counts ~0. This cleanly
 * separates text rows from noisy blank rows regardless of ink density. */
static int row_paired_ink(const OcrImage *page, int y, int bg, int W) {
    int H = (int)ocr_image_height(page);
    int cnt = 0;
    for (int x = 0; x < W; x++) {
        if (!is_ink(ocr_image_get(page, (size_t)x, (size_t)y), bg)) continue;
        int connected = 0;
        for (int dy = -1; dy <= 1 && !connected; dy++) {
            int ny = y + dy;
            if (ny < 0 || ny >= H) continue;
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx;
                if (nx < 0 || nx >= W) continue;
                if (is_ink(ocr_image_get(page, (size_t)nx, (size_t)ny), bg)) { connected = 1; break; }
            }
        }
        if (connected) cnt++;
    }
    return cnt;
}

/* Build a strip-tall (height == `strip`) line image centered on the text
 * line at vertical position `cy`. The window is a FIXED strip-tall region
 * around the line center -- NO vertical scaling. The CRNN was trained on
 * glyphs centered in a fixed strip-tall cell; stretching the variable-height
 * ink band to `strip` pixels distorts glyph proportions and tanks accuracy.
 * `cy` is the middle of the detected ink run; we take [cy-strip/2, cy+strip/2). */
static OcrImage *extract_line(const OcrImage *page, int cy, int strip) {
    int W = (int)ocr_image_width(page);
    int H = (int)ocr_image_height(page);
    OcrImage *line = ocr_image_create((size_t)W, (size_t)strip);
    if (!line) return NULL;
    int top = cy - strip / 2;
    for (int y = 0; y < strip; y++) {
        int sy = top + y;
        if (sy < 0) sy = 0;
        if (sy >= H) sy = H - 1;
        for (int x = 0; x < W; x++)
            ocr_image_set(line, (size_t)x, (size_t)y, ocr_image_get(page, (size_t)x, (size_t)sy));
    }
    return line;
}

/* Rotate `src` about its center by `deg` degrees (nearest-neighbour). The
 * canvas is kept the same size; rotated-out corners are filled with `fill`
 * (use the page background so they don't read as ink). Returns a new image. */
static OcrImage *rotate_img(const OcrImage *src, double deg, uint8_t fill) {
    int W = (int)ocr_image_width(src), H = (int)ocr_image_height(src);
    int cx = W / 2, cy = H / 2;
    double a = deg * 3.141592653589793 / 180.0, ca = cos(a), sa = sin(a);
    OcrImage *dst = ocr_image_create((size_t)W, (size_t)H);
    if (!dst) return NULL;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
        ocr_image_set(dst, (size_t)x, (size_t)y, fill);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int sx = (int)(cx + (x - cx) * ca + (y - cy) * sa);
        int sy = (int)(cy - (x - cx) * sa + (y - cy) * ca);
        if (sx >= 0 && sx < W && sy >= 0 && sy < H)
            ocr_image_set(dst, (size_t)x, (size_t)y, ocr_image_get(src, (size_t)sx, (size_t)sy));
    }
    return dst;
}

/* Projection-profile deskew: scan small rotation angles and pick the one that
 * maximizes the variance of the per-row ink count. Text lines are most
 * separated (and background bands emptiest) at the correct skew, so this
 * straightens a slightly-rotated page before line segmentation. */
static OcrImage *deskew_page(const OcrImage *src, int bg) {
    if (getenv("DESKEW") && getenv("DESKEW")[0] == '0') return NULL;
    double best = -1; int bestk = 0;
    uint8_t fill = (uint8_t)(bg > 127 ? 235 : 15);
    for (int k = -16; k <= 16; k++) {
        double deg = k * 0.5;
        OcrImage *r = rotate_img(src, deg, fill);
        int H = (int)ocr_image_height(r), W = (int)ocr_image_width(r);
        double mean = 0;
        int *proj = calloc((size_t)H, sizeof(int));
        for (int y = 0; y < H; y++) {
            int cnt = row_paired_ink(r, y, bg, W);
            proj[y] = cnt > 0 ? cnt : 0;   /* paired>0 => real ink row */
            mean += proj[y];
        }
        mean /= H;
        double var = 0;
        for (int y = 0; y < H; y++) { double d = proj[y] - mean; var += d * d; }
        free(proj); ocr_image_free(r);
        if (var > best) { best = var; bestk = k; }
    }
    if (bestk == 0) return NULL;
    return rotate_img(src, bestk * 0.5, fill);
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

    /* ---- polarity normalization ----
     * The model is trained on dark background + light text. Real scans/photos
     * are the opposite (light background + dark text). Detect that from the
     * median and invert the whole page so the rest of the pipeline (deskew,
     * segmentation, CRNN) sees the polarity it was trained on. */
    OcrImage *norm = NULL;
    if (bg > 127) {
        norm = ocr_image_create((size_t)W, (size_t)H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                ocr_image_set(norm, (size_t)x, (size_t)y,
                              (uint8_t)(255 - ocr_image_get(page, (size_t)x, (size_t)y)));
        bg = 15;   /* after inversion, background is dark */
        page = norm;
    }

    /* ---- deskew: straighten a slightly-rotated page before segmentation ----\n     * A perfect (unrotated) page deskews to itself (no-op), so this is safe to\n     * always run on photo/scan input. */
    OcrImage *desk = deskew_page(page, bg);
    const OcrImage *pg = desk ? desk : page;
    W = (int)ocr_image_width(pg);
    H = (int)ocr_image_height(pg);

    /* ---- horizontal ink-projection: mark rows that contain any ink ---- */
    char *row_ink = malloc((size_t)H);
    if (!row_ink) { if (desk) ocr_image_free(desk); return -1; }
    for (int y = 0; y < H; y++) {
        /* paired-ink: a real text row has contiguous strokes (paired pixels);
         * a blank row with salt-and-pepper noise has only isolated single
         * pixels (paired = 0), so noise is rejected while real lines stay solid.
         * Use >=1 so sparse stroke-tips at line edges don't split a line. */
        int cnt = row_paired_ink(pg, y, bg, W);
        row_ink[y] = (char)(cnt >= 1);
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
        int cy = (y0 + y1) / 2;     /* center of the line */

        OcrImage *line = extract_line(pg, cy, strip);
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
    if (desk) ocr_image_free(desk);
    if (norm) ocr_image_free(norm);

    if (rc != 0) { free(buf); return -1; }
    if (out_json) *out_json = buf;
    (void)nlines;
    return 0;
}
