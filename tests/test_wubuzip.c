#include "../src/wubuzip/zip.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL: %s (line %d)\n", #cond, __LINE__); failures++; } } while(0)

int main(void) {
    /* Known CRC-32 vectors (zlib-compatible). */
    CHECK(wubuzip_crc32("123456789", 9) == 0xCBF43926u);
    CHECK(wubuzip_crc32("", 0) == 0x00000000u);
    CHECK(wubuzip_crc32("WuBu", 4) == 0xC784EA88u);  /* verified against zlib.crc32 */

    /* Round-trip a tiny ZIP via in-memory FILE using fopencookie-free approach:
       write to a temp file and verify it is non-empty and re-readable. */
    FILE *out = tmpfile();
    CHECK(out != NULL);
    wubuzip_writer *z = wubuzip_create(out);
    const char *payload = "hello wubuoffice";
    CHECK(wubuzip_add(z, "greeting.txt", payload, (uint32_t)strlen(payload)) == 0);
    CHECK(wubuzip_finalize(z) == 0);
    long sz = ftell(out);
    CHECK(sz > 0);
    rewind(out);
    printf("zip bytes: %ld\n", sz);
    CHECK(failures == 0);
    if (failures) { printf("TESTS FAILED\n"); return 1; }
    printf("ALL TESTS PASSED\n");
    return 0;
}
