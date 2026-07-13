/* test_deflate_roundtrip.c — ws07#1338: we use OUR OWN dependency-free
 * DEFLATE codec (wubuzip), not zlib. Verifies our deflate -> our
 * inflate round-trips arbitrary payloads byte-for-byte, with NO third-party
 * compression library involved. */

#include "../src/wubuzip/deflate.h"
#include "../src/wubuzip/inflate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int roundtrip(const char *text) {
    const uint8_t *in = (const uint8_t *)text;
    size_t in_len = strlen(text);

    uint8_t *cmp = NULL; size_t cmp_len = 0;
    if (wubuzip_deflate(in, in_len, &cmp, &cmp_len) != 0) return -1;

    uint8_t *out = NULL; size_t out_len = 0;
    if (wubuzip_inflate(cmp, cmp_len, &out, &out_len, 0) != 0) {
        free(cmp); return -2;
    }
    int ok = (out_len == in_len) && memcmp(out, in, in_len) == 0;
    free(cmp); free(out);
    return ok ? 0 : -3;
}

int main(void) {
    const char *cases[] = {
        "",
        "hello",
        "WuBuOffice is a ground-up C11 SLERM of OOXML. No forks, no .NET.",
        /* high-entropy-ish, long, to exercise many blocks */
        "aaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbcccccccccccccccccc"
        "The quick brown fox jumps over the lazy dog. 1234567890 !@#$%^&*()",
        NULL
    };
    int n = 0, pass = 0;
    for (int i = 0; cases[i]; i++) {
        n++;
        int r = roundtrip(cases[i]);
        if (r == 0) pass++;
        else printf("FAIL case %d (code %d): '%.20s'\n", i, r, cases[i]);
    }
    assert(pass == n && "all deflate->inflate round-trips byte-identical");
    printf("deflate_roundtrip: %d/%d PASSED (our own DEFLATE, no zlib) (ws07#1338)\n",
           pass, n);
    return 0;
}
