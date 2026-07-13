#include "deflate.h"
#include "inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Round-trip: wubuzip_deflate() then wubuzip_inflate() must reproduce the
 * input exactly, and compression must actually shrink typical XML. */

static int roundtrip(const uint8_t *in, size_t n, int *shrank) {
    uint8_t *comp = NULL; size_t cl = 0;
    if (wubuzip_deflate(in, n, &comp, &cl) != 0) { printf("deflate failed\n"); return 1; }
    *shrank = (int)(cl < n);
    uint8_t *out = NULL; size_t ol = 0;
    if (wubuzip_inflate(comp, cl, &out, &ol, 0) != 0) {
        printf("inflate failed (n=%zu cl=%zu)\n", n, cl); free(comp); return 1;
    }
    int bad = (ol != n) || (n && memcmp(out, in, n) != 0);
    if (bad) printf("mismatch n=%zu ol=%zu\n", n, ol);
    free(comp); free(out);
    return bad ? 1 : 0;
}

int main(void) {
    int fails = 0;

    /* 1) empty */
    { int s; fails += roundtrip((const uint8_t*)"", 0, &s); }

    /* 2) single byte (must fall back to store and still round-trip) */
    { uint8_t b = 'A'; int s; fails += roundtrip(&b, 1, &s); }

    /* 3) highly repetitive (should compress well) */
    {
        size_t n = 4000; uint8_t *buf = malloc(n);
        for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)("AAAAAAABBBBCCC"[(i/3) % 13]);
        int s = 0; fails += roundtrip(buf, n, &s);
        if (!s) { printf("repetitive data did not shrink\n"); fails++; }
        free(buf);
    }

    /* 4) realistic OOXML-ish XML (should shrink) */
    {
        const char *xml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            "<w:body><w:p><w:r><w:t>WuBuOffice ground-up C11 SLERM of OOXML</w:t></w:r></w:p>"
            "<w:p><w:r><w:t>WuBuOffice ground-up C11 SLERM of OOXML</w:t></w:r></w:p>"
            "<w:p><w:r><w:t>WuBuOffice ground-up C11 SLERM of OOXML</w:t></w:r></w:p>"
            "<w:tbl><w:tr><w:tc><w:p><w:r><w:t>wubuzip</w:t></w:r></w:p></w:tc>"
            "<w:tc><w:p><w:r><w:t>wubuxml</w:t></w:r></w:p></w:tc></w:tr></w:tbl>"
            "</w:body></w:document>";
        int s = 0;
        fails += roundtrip((const uint8_t*)xml, strlen(xml), &s);
        if (!s) { printf("xml data did not shrink\n"); fails++; }
    }

    /* 5) random-ish (should at least round-trip, store fallback ok) */
    {
        size_t n = 2000; uint8_t *buf = malloc(n);
        uint32_t x = 12345;
        for (size_t i = 0; i < n; i++) { x = x*1103515245u + 12345u; buf[i] = (uint8_t)(x >> 16); }
        int s; fails += roundtrip(buf, n, &s);
        free(buf);
    }

    if (fails) { printf("DEFLATE ROUNDTRIP FAILED: %d\n", fails); return 1; }
    printf("DEFLATE ROUNDTRIP OK\n");
    return 0;
}
