/* test_zoning.c -- unit tests for the ZoningExtractor module.
 * Build: cc -std=c11 -O2 -Isrc/wubuocr tests/test_zoning.c src/wubuocr/zoning.c -o build/test_zoning
 * Verifies: create/destroy, dim formula, determinism, and that two distinct
 * synthetic glyphs yield distinct feature vectors. Returns 0 on success. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "zoning.h"

static int fail(const char *msg) { fprintf(stderr, "FAIL: %s\n", msg); return 1; }

int main(void) {
    int rc = 0;
    ZoningExtractor *z = zoning_create(12);
    if (!z) return fail("zoning_create NULL");
    if (zoning_dim(z) != 12 * 12 + 8) rc |= fail("dim formula wrong");
    zoning_destroy(z);

    /* Render two distinct 28x28 glyphs (a vertical bar vs a horizontal bar).
     * Inverted convention: ink is DARK (<=127). */
    unsigned char A[28 * 28], B[28 * 28];
    memset(A, 255, sizeof A); memset(B, 255, sizeof B);   /* all background */
    for (int y = 4; y < 24; y++) for (int x = 13; x < 16; x++) A[y * 28 + x] = 0;  /* vertical bar */
    for (int y = 13; y < 16; y++) for (int x = 4; x < 24; x++) B[y * 28 + x] = 0;  /* horizontal bar */

    ZoningExtractor *zx = zoning_create(12);
    int dim = zoning_dim(zx);
    float fa[512], fb[512];
    zoning_extract(zx, A, 28, 28, fa);
    zoning_extract(zx, B, 28, 28, fb);
    double diff = 0;
    for (int d = 0; d < dim; d++) diff += fabsf(fa[d] - fb[d]);
    if (diff < 1e-3f) rc |= fail("distinct glyphs produced identical features");
    /* determinism */
    float fb2[512];
    zoning_extract(zx, B, 28, 28, fb2);
    for (int d = 0; d < dim; d++) if (fa[d] != fa[d] || fb[d] != fb2[d]) { rc |= fail("non-deterministic"); break; }
    /* empty image -> zero features (no crash) */
    unsigned char E[28 * 28]; memset(E, 255, sizeof E);
    float fe[512];
    zoning_extract(zx, E, 28, 28, fe);
    zoning_destroy(zx);

    if (rc == 0) printf("test_zoning: ALL PASS (glyph diff=%.3f)\n", diff);
    return rc;
}
