#include "../src/wubuzip/inflate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Verify our from-scratch inflate decodes zlib-compressed data correctly. */
static int roundtrip(const char *text) {
    uLongf clen = compressBound((uLong)strlen(text));
    uint8_t *cbuf = malloc(clen + 16);
    if (compress2(cbuf, &clen, (const Bytef *)text, (uLong)strlen(text), Z_DEFAULT_COMPRESSION) != Z_OK) {
        free(cbuf); return 1;
    }
    /* skip zlib 2-byte header for raw-deflate test */
    uint8_t *out = NULL; size_t out_len = 0;
    if (wubuzip_inflate(cbuf + 2, clen - 2, &out, &out_len, 0) != 0) {
        free(cbuf); return 2;
    }
    int ok = (out_len == strlen(text)) && (out_len == 0 || memcmp(out, text, out_len) == 0);
    if (!ok) printf("MISMATCH: got %zu bytes '%.*s'\n", out_len, (int)out_len, out);
    free(out); free(cbuf);
    return ok ? 0 : 3;
}

int main(void) {
    const char *cases[] = {
        "",
        "hello wubuoffice",
        "AAAAAAAAAAAAAAAAAAAAAAA",                 /* repeat -> dynamic huffman */
        "The quick brown fox jumps over the lazy dog 0123456789 repeated repeated repeated repeated repeated",
        "WuBuOffice is a ground-up C11 SLERM of OOXML. No forks, no .NET, just bytes and a deterministic container engine."
    };
    int fail = 0;
    for (size_t i = 0; i < 5; i++) {
        int r = roundtrip(cases[i]);
        if (r != 0) { printf("FAIL case %zu (rc=%d)\n", i, r); fail = 1; }
        else printf("ok case %zu (len %zu)\n", i, strlen(cases[i]));
    }
    if (fail) { printf("INFLATE TESTS FAILED\n"); return 1; }
    printf("INFLATE TESTS PASSED\n");
    return 0;
}
