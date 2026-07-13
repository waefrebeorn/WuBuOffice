#include "deflate.h"
#include "inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Round-trip: wubuzip_deflate() then wubuzip_inflate() must reproduce the
 * input exactly. Also cross-check against system zlib (reference oracle):
 *   - our stream must inflate under zlib (proves standards compliance)
 *   - a zlib stream (which uses dynamic blocks) must inflate under our decoder
 */

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

/* zlib must inflate our stream (proves our dynamic/fixed/stored blocks are
 * spec-compliant). */
static int zlib_decodes_ours(const uint8_t *in, size_t n) {
    uint8_t *comp = NULL; size_t cl = 0;
    if (wubuzip_deflate(in, n, &comp, &cl) != 0) return 1;
    z_stream z; memset(&z, 0, sizeof z);
    int rc = inflateInit2(&z, -MAX_WBITS); /* raw */
    if (rc != Z_OK) { free(comp); return 1; }
    uLongf ol = (uLongf)(n * 4) + 100;
    uint8_t *out = malloc(ol);
    z.next_in = comp; z.avail_in = (uInt)cl;
    z.next_out = out; z.avail_out = (uInt)ol;
    rc = inflate(&z, Z_FINISH);
    int ok = (rc == Z_STREAM_END && z.total_out == n && memcmp(out, in, n) == 0);
    if (!ok) printf("zlib could not decode our stream (n=%zu cl=%zu rc=%d)\n", n, cl, rc);
    inflateEnd(&z);
    free(comp); free(out);
    return ok ? 0 : 1;
}

/* our decoder must inflate a zlib stream (zlib emits dynamic blocks). */
static int ours_decodes_zlib(const uint8_t *in, size_t n) {
    z_stream z; memset(&z, 0, sizeof z);
    int rc = deflateInit2(&z, 9, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);
    if (rc != Z_OK) return 1;
    uLongf cs = (uLongf)(n * 2) + 100;
    uint8_t *comp = malloc(cs);
    z.next_in = (Bytef*)in; z.avail_in = (uInt)n;
    z.next_out = comp; z.avail_out = (uInt)cs;
    deflate(&z, Z_FINISH);
    uLong cl = z.total_out;
    deflateEnd(&z);

    uint8_t *out = NULL; size_t ol = 0;
    int ir = wubuzip_inflate(comp, cl, &out, &ol, 0);
    int ok = (ir == 0 && ol == n && memcmp(out, in, n) == 0);
    if (!ok) printf("our decoder failed on zlib stream (n=%zu cl=%lu)\n", n, cl);
    free(comp); free(out);
    return ok ? 0 : 1;
}

int main(void) {
    int fails = 0;

    /* 1) empty */
    { int s; fails += roundtrip((const uint8_t*)"", 0, &s); }
    /* 2) single byte (store fallback) */
    { uint8_t b = 'A'; int s; fails += roundtrip(&b, 1, &s); }
    /* 3) highly repetitive */
    {
        size_t n = 4000; uint8_t *buf = malloc(n);
        for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)("AAAAAAABBBBCCC"[(i/3) % 13]);
        int s = 0; fails += roundtrip(buf, n, &s);
        if (!s) { printf("repetitive did not shrink\n"); fails++; }
        fails += zlib_decodes_ours(buf, n);
        fails += ours_decodes_zlib(buf, n);
        free(buf);
    }
    /* 4) realistic OOXML-ish XML */
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
        fails += zlib_decodes_ours((const uint8_t*)xml, strlen(xml));
        fails += ours_decodes_zlib((const uint8_t*)xml, strlen(xml));
    }
    /* 5) random-ish (store fallback ok, still round-trips) */
    {
        size_t n = 2000; uint8_t *buf = malloc(n);
        uint32_t x = 12345;
        for (size_t i = 0; i < n; i++) { x = x*1103515245u + 12345u; buf[i] = (uint8_t)(x >> 16); }
        int s; fails += roundtrip(buf, n, &s);
        fails += zlib_decodes_ours(buf, n);
        fails += ours_decodes_zlib(buf, n);
        free(buf);
    }
    /* 6) two-block input (forces multi-block split) */
    {
        size_t n = 70000; uint8_t *buf = malloc(n);
        for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)("The quick brown fox jumps over the lazy dog. "[i % 45]);
        int s = 0; fails += roundtrip(buf, n, &s);
        if (!s) { printf("big text did not shrink\n"); fails++; }
        fails += zlib_decodes_ours(buf, n);
        fails += ours_decodes_zlib(buf, n);
        free(buf);
    }

    if (fails) { printf("DEFLATE ROUNDTRIP FAILED: %d\n", fails); return 1; }
    printf("DEFLATE ROUNDTRIP OK (dynamic+fixed+stored, zlib cross-checked)\n");
    return 0;
}
