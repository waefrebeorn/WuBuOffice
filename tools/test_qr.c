/* test_qr.c -- verifies the QR codec (#49):
 *   1) encode -> module matrix -> decode (direct matrix round-trip) for several
 *      strings across versions 1..7.
 *   2) render a QR to a grayscale page (scaled), then acquire + decode via
 *      qr_detect_blocks (exercises finder detection + affine sampling).
 */
#include "qr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct { const char *s; } cases[] = {
    {"HELLO"},
    {"https://example.com/page?id=42"},
    {"WUBOFFICE OCR 1234567890"},
    {"ABCDEFGHIJKLMNOPQRSTUVWXYZ12"}, /* 28 bytes, fits v7 datacw(31)-2 */
    {"A"},
    {NULL,}
};

/* render a module matrix to a PGM grayscale page at `scale` px/module,
 * surrounded by a white quiet zone. Returns malloc'd pixel buffer. */
static unsigned char *render_pgm(const unsigned char *m, int N, int scale, int quiet, int *W, int *H){
    int q = quiet * scale;
    int w = N * scale + 2 * q, h = N * scale + 2 * q;
    unsigned char *pix = (unsigned char*)malloc((size_t)w * h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) pix[y * w + x] = 255;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
        if (!m[r * N + c]) continue;
        for (int dy = 0; dy < scale; dy++) for (int dx = 0; dx < scale; dx++) {
            int x = q + c * scale + dx, y = q + r * scale + dy;
            pix[y * w + x] = 0;
        }
    }
    *W = w; *H = h;
    return pix;
}

int main(void){
    int fail = 0;
    /* 1) direct matrix round-trip */
    for (int i = 0; cases[i].s; i++) {
        unsigned char *m = NULL; int N = 0;
        int ver = qr_encode(cases[i].s, &m, &N);
        if (ver < 1) { printf("FAIL: encode '%s'\n", cases[i].s); fail++; free(m); continue; }
        char out[256];
        if (qr_decode_matrix(m, N, out, sizeof out) != 0 || strcmp(out, cases[i].s) != 0) {
            printf("FAIL: round-trip '%s' -> '%s' (ver %d)\n", cases[i].s, out, ver);
            fail++;
        } else {
            printf("ok   ver%d '%s'\n", ver, cases[i].s);
        }
        free(m);
    }

    /* 2) image acquisition: render -> acquire -> decode */
    const char *msg = "QR-TEST-2026";
    unsigned char *m = NULL; int N = 0;
    int ver = qr_encode(msg, &m, &N);
    if (ver < 1) { printf("FAIL: encode image case\n"); fail++; }
    else {
        int W, H; int scale = 6, quiet = 4;
        unsigned char *pix = render_pgm(m, N, scale, quiet, &W, &H);
        char text[4][256]; int x0[4], y0[4], x1[4], y1[4];
        int n = qr_detect_blocks(pix, W, H, 255, 4, text, 256, x0, y0, x1, y1);
        int ok = (n >= 1 && strcmp(text[0], msg) == 0);
        if (!ok) {
            printf("FAIL: image acquire '%s' (found %d, got '%s')\n", msg, n, n ? text[0] : "");
            fail++;
        } else {
            printf("ok   image acquire ver%d '%s' bbox=%dx%d\n", ver, text[0], x1[0]-x0[0], y1[0]-y0[0]);
        }
        free(pix); free(m);
    }

    printf(fail ? "FAIL: %d QR cases wrong\n" : "PASS: all QR cases correct\n", fail);
    return fail ? 1 : 0;
}
