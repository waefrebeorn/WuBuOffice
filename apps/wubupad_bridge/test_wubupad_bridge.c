/* test_wubupad_bridge.c -- verify WuBuOffice reuses WuBuPad's editor core
 * (Phase E cross-repo bridge). Writes a temp file, runs stats + find/replace
 * through wubupad_bridge, and asserts correct behavior. */
#include "wubupad_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL: %s\n", m); failures++; } } while(0)

int main(void) {
    const char *path = "/tmp/wubupad_bridge_test.txt";
    FILE *f = fopen(path, "wb");
    if (!f) { printf("FAIL: cannot create temp file\n"); return 1; }
    const char *src = "alpha\nbeta\nalpha gamma\n";
    fwrite(src, 1, strlen(src), f);
    fclose(f);

    /* stats */
    size_t lines = 0, chars = 0;
    CHECK(wubupad_stats(path, &lines, &chars) == 0, "stats ok");
    CHECK(lines == 4, "4 lines (trailing newline => empty 4th)");
    CHECK(chars == strlen(src), "char count matches");

    /* find/replace literal alpha -> X */
    char *out = NULL; size_t len = 0;
    int rc = wubupad_find_replace(path, "alpha", 0, 0, "X", &out, &len);
    CHECK(rc == 0, "replaced (rc 0)");
    CHECK(strstr(out, "X") != NULL, "X present");
    CHECK(strstr(out, "alpha") == NULL, "alpha gone");
    CHECK(strstr(out, "X gamma") != NULL, "alpha gamma -> X gamma");
    free(out);

    /* no-match case */
    out = NULL; len = 0;
    rc = wubupad_find_replace(path, "zzz", 0, 0, "Q", &out, &len);
    CHECK(rc == 1, "no-match rc 1");
    CHECK(strcmp(out, src) == 0, "original unchanged on no match");
    free(out);

    remove(path);
    if (failures) { printf("FAILED (%d)\n", failures); return 1; }
    printf("PASS: wubupad_bridge (cross-repo reuse of WuBuPad core)\n");
    return 0;
}
