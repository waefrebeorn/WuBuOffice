/* test_mlp.c -- unit tests for the dependency-free MLP module.
 * Build: cc -std=c11 -O2 -Isrc/wubuocr tests/test_mlp.c src/wubuocr/mlp.c -o build/test_mlp -lm
 * Verifies: create/destroy, opaque accessors, forward determinism, and that a
 * 2-class problem is separable (gradient flows). Returns 0 on success. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mlp.h"

static int fail(const char *msg) { fprintf(stderr, "FAIL: %s\n", msg); return 1; }

int main(void) {
    int rc = 0;
    MLP *m = mlp_create(10, 16, 8, 2, 12345);
    if (!m) return fail("mlp_create returned NULL");
    if (mlp_din(m) != 10 || mlp_h1(m) != 16 || mlp_h2(m) != 8 || mlp_K(m) != 2)
        rc |= fail("topology accessors wrong");
    if (mlp_layer_count(m) != 6) rc |= fail("layer count != 6");

    /* forward determinism + softmax */
    float z[10]; for (int i = 0; i < 10; i++) z[i] = (i % 3) * 0.3f - 0.3f;
    float s1[2], s2[2];
    mlp_forward(m, z, s1);
    mlp_forward(m, z, s2);
    if (s1[0] != s2[0] || s1[1] != s2[1]) rc |= fail("forward nondeterministic");
    float sm = expf(s1[0]) + expf(s1[1]);
    if (fabsf((expf(s1[0]) / sm) + (expf(s1[1]) / sm) - 1.0f) > 1e-4f)
        rc |= fail("softmax does not sum to 1");

    /* 2-class separability: A=[1,0,...], B=[0,1,...] must become separable */
    float zA[10], zB[10];
    for (int i = 0; i < 10; i++) { zA[i] = 0; zB[i] = 0; }
    zA[0] = 1.0f; zA[1] = -0.5f; zA[3] = 0.8f;
    zB[2] = 1.0f; zB[4] = 0.6f; zB[5] = -0.9f; zB[7] = 0.3f;
    for (int t = 0; t < 3000; t++) {
        mlp_forward(m, zA, s1); mlp_train_step(m, zA, 0); mlp_apply_plain(m, 0.3f);
        mlp_forward(m, zB, s1); mlp_train_step(m, zB, 1); mlp_apply_plain(m, 0.3f);
    }
    mlp_forward(m, zA, s1); mlp_forward(m, zB, s2);
    float a0 = expf(s1[0]), a1 = expf(s1[1]); float sa = a0 + a1; a0 /= sa; a1 /= sa;
    float b0 = expf(s2[0]), b1 = expf(s2[1]); float sb = b0 + b1; b0 /= sb; b1 /= sb;
    if (!(a0 > a1 && b1 > b0)) rc |= fail("2-class not separable (gradient broken)");

    /* save/load roundtrip preserves topology */
    float zm[10] = {0}, zs[10] = {1};
    if (mlp_save(m, zm, zs, 10, "/tmp/_mlp_test.wts") != 0) rc |= fail("mlp_save failed");
    MLP *m2 = NULL; float zm2[10], zs2[10]; int dim2 = 0;
    if (mlp_load("/tmp/_mlp_test.wts", &m2, zm2, zs2, &dim2) != 0) rc |= fail("mlp_load failed");
    else {
        if (mlp_din(m2) != 10 || dim2 != 10) rc |= fail("load topology mismatch");
        mlp_destroy(m2);
    }

    mlp_destroy(m);
    if (rc == 0) printf("test_mlp: ALL PASS\n");
    return rc;
}
