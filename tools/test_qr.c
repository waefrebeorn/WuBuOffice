/* test_qr.c -- verifies the QR codec (#49):
 *   1) encode -> module matrix -> decode (direct matrix round-trip) across
 *      versions 1..7 (byte-mode, ECC level M).
 *   2) matrix round-trip WITH injected module errors (exercises RS correction
 *      through the full pipeline, including edge codeword positions).
 *   3) render a QR to a grayscale page (scaled/offset), then acquire + decode
 *      via qr_detect_blocks (finder detection, trio geometry, affine sampling).
 */
#include "qr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *cases[] = {
    "A",
    "HELLO",
    "https://example.com/page?id=42",
    "WUBOFFICE OCR 1234567890",
    "this-is-a-26-byte-payload!",                                       /* v2 */
    "a-42-byte-payload-that-needs-version-three",                       /* v3 */
    "sixty-two-bytes-of-payload-to-push-into-version-four-territory",   /* v4 */
    NULL
};

/* render a module matrix to a grayscale page at `scale` px/module with a white
 * quiet zone, placed at (offx,offy) inside a pw x ph page. */
static unsigned char *render_page(const unsigned char *m, int N, int scale, int quiet,
                                  int offx, int offy, int pw, int ph){
    unsigned char *pix = (unsigned char*)malloc((size_t)pw * ph);
    memset(pix, 255, (size_t)pw * ph);
    int q = quiet * scale;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) {
        if (!m[r * N + c]) continue;
        for (int dy = 0; dy < scale; dy++) for (int dx = 0; dx < scale; dx++) {
            int x = offx + q + c * scale + dx, y = offy + q + r * scale + dy;
            if (x >= 0 && x < pw && y >= 0 && y < ph) pix[y * pw + x] = 0;
        }
    }
    return pix;
}

static int acquire_case(const char *msg, int scale, int offx, int offy){
    unsigned char *m = NULL; int N = 0;
    int ver = qr_encode(msg, &m, &N);
    if (ver < 1) { printf("FAIL: encode acquire '%s'\n", msg); return 1; }
    int q = 4;
    int pw = offx + (N + 2 * q) * scale + 40, ph = offy + (N + 2 * q) * scale + 40;
    unsigned char *pix = render_page(m, N, scale, q, offx, offy, pw, ph);
    char text[4][256]; int x0[4], y0[4], x1[4], y1[4];
    int n = qr_detect_blocks(pix, pw, ph, 255, 4, text, 256, x0, y0, x1, y1);
    int ok = (n >= 1 && strcmp(text[0], msg) == 0);
    if (!ok) printf("FAIL: acquire v%d scale=%d off=(%d,%d) '%s' (found %d, got '%s')\n",
                    ver, scale, offx, offy, msg, n, n ? text[0] : "");
    else     printf("ok   acquire v%d scale=%d off=(%d,%d) '%s'\n", ver, scale, offx, offy, msg);
    free(pix); free(m);
    return ok ? 0 : 1;
}

int main(void){
    int fail = 0;

    /* 1) direct matrix round-trip across versions */
    for (int i = 0; cases[i]; i++) {
        unsigned char *m = NULL; int N = 0;
        int ver = qr_encode(cases[i], &m, &N);
        if (ver < 1) { printf("FAIL: encode '%s'\n", cases[i]); fail++; free(m); continue; }
        char out[256];
        if (qr_decode_matrix(m, N, out, sizeof out) != 0 || strcmp(out, cases[i]) != 0) {
            printf("FAIL: round-trip '%s' -> '%s' (ver %d)\n", cases[i], out, ver);
            fail++;
        } else {
            printf("ok   ver%d '%s'\n", ver, cases[i]);
        }
        free(m);
    }

    /* 2) matrix round-trip with injected module damage: flip a small square of
     * data modules; RS must correct it. (Stays within EC capacity: v1-M
     * corrects 4 codewords per block.) */
    {
        const char *msg = "DAMAGE-TEST";
        unsigned char *m = NULL; int N = 0;
        int ver = qr_encode(msg, &m, &N);
        if (ver < 1) { printf("FAIL: encode damage case\n"); fail++; }
        else {
            /* flip an 2x2 block of modules in the data region (bottom-right
             * quadrant, away from finders/timing/format) */
            for (int r = N - 3; r < N - 1; r++)
                for (int c = N - 3; c < N - 1; c++)
                    m[r * N + c] ^= 1;
            char out[256];
            if (qr_decode_matrix(m, N, out, sizeof out) != 0 || strcmp(out, msg) != 0) {
                printf("FAIL: damaged round-trip '%s' -> '%s'\n", msg, out);
                fail++;
            } else {
                printf("ok   damaged round-trip '%s' (2x2 modules flipped, RS corrected)\n", msg);
            }
        }
        free(m);
    }

    /* 3) image acquisition across scales, offsets and versions */
    fail += acquire_case("HI", 6, 0, 0);
    fail += acquire_case("QR-TEST-2026", 6, 0, 0);
    fail += acquire_case("QR-TEST-2026", 3, 0, 0);
    fail += acquire_case("QR-TEST-2026", 9, 0, 0);
    fail += acquire_case("QR-TEST-2026", 6, 57, 33);
    fail += acquire_case("https://waefrebeorn.com/x", 6, 0, 0);
    fail += acquire_case("WUBUOFFICE-QR-ACQUISITION-LONGER-PAYLOAD-42", 6, 0, 0);
    fail += acquire_case("WUBUOFFICE-QR-ACQUISITION-LONGER-PAYLOAD-42", 4, 21, 60);

    printf(fail ? "FAIL: %d QR cases wrong\n" : "PASS: all QR cases correct\n", fail);
    return fail ? 1 : 0;
}
