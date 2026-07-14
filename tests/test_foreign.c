/* test_foreign -- robustness gate: read OOXML files produced by INDEPENDENT
 * tools (openpyxl, python-pptx), not by WuBuOffice. This proves "control of
 * destiny": our readers consume real third-party files, not just our own
 * output, and our formula engine supplies values the other tool omitted.
 *
 * The foreign files are generated at test time via
 * tests/conformance/produce_foreign.py. If the optional Python libraries are
 * unavailable, the producer exits 2 and this test SKIPs (returns 0) so the
 * sanitizer/CI gate still passes without the deps. */

#include "../apps/wubucell/cell.h"
#include "../apps/wubucell/cell_read.h"
#include "../apps/wubushow/show.h"
#include "../apps/wubushow/show_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_xlsx(const char *path) {
    wubucell_book *b = NULL;
    if (wubucell_read(path, &b) != 0) { printf("FAIL xlsx read %s\n", path); return 1; }
    int ok = 1;
    if (wubucell_sheet_count(b) != 2) { ok = 0; printf("FAIL xlsx sheets=%d (want 2)\n", wubucell_sheet_count(b)); }

    const char *a1 = NULL; double b2 = 0, b3 = 0, b4c = 0; wubucell_ckind k4 = WUBUCELL_NUM;
    if (wubucell_get(b, 1, 1, 1, NULL, &a1, NULL, NULL) != 0 || !a1 || strcmp(a1, "Item") != 0)
        { ok = 0; printf("FAIL xlsx S1 A1=%s\n", a1 ? a1 : "<null>"); }
    if (wubucell_get(b, 1, 2, 2, NULL, NULL, &b2, NULL) != 0 || b2 != 1200.5)
        { ok = 0; printf("FAIL xlsx S1 B2=%g\n", b2); }
    if (wubucell_get(b, 1, 2, 3, NULL, NULL, &b3, NULL) != 0 || b3 != 320)
        { ok = 0; printf("FAIL xlsx S1 B3=%g\n", b3); }
    /* The crucial one: openpyxl wrote <f>SUM(B2:B3)</f> with NO cached <v>.
     * Our engine must have recomputed 1520.5. */
    if (wubucell_get(b, 1, 2, 4, &k4, NULL, NULL, &b4c) != 0 || k4 != WUBUCELL_FORM || b4c != 1520.5)
        { ok = 0; printf("FAIL xlsx S1 B4 kind=%d cached=%g (want form/1520.5)\n", (int)k4, b4c); }

    /* second sheet must exist and carry its data (sheet-mapping robustness) */
    const char *s2a1 = NULL; double s2b1 = 0;
    if (wubucell_get(b, 2, 1, 1, NULL, &s2a1, NULL, NULL) != 0 || !s2a1 || strcmp(s2a1, "Total") != 0)
        { ok = 0; printf("FAIL xlsx S2 A1=%s\n", s2a1 ? s2a1 : "<null>"); }
    if (wubucell_get(b, 2, 2, 1, NULL, NULL, &s2b1, NULL) != 0 || s2b1 != 1520.5)
        { ok = 0; printf("FAIL xlsx S2 B1=%g\n", s2b1); }

    wubucell_free(b);
    return ok ? 0 : 1;
}

static int check_pptx(const char *path) {
    wubushow_pres *p = NULL;
    if (wubushow_read(path, &p) != 0) { printf("FAIL pptx read %s\n", path); return 1; }
    int ok = 1;
    if (wubushow_slide_count(p) != 2) { ok = 0; printf("FAIL pptx slides=%d (want 2)\n", wubushow_slide_count(p)); }
    else {
        const char *t0 = NULL, *b0 = NULL, *t1 = NULL, *b1 = NULL;
        wubushow_slide_get(p, 0, &t0, &b0);
        wubushow_slide_get(p, 1, &t1, &b1);
        /* python-pptx names the title shape "Title 1" and marks it with
         * <p:ph type="title">; our reader must classify by placeholder type,
         * not exact name, and must NOT leak the title into the body. */
        if (!t0 || strcmp(t0, "Foreign Slide One") != 0) { ok = 0; printf("FAIL pptx s1 title=%s\n", t0 ? t0 : "<null>"); }
        if (!b0 || strcmp(b0, "Alpha bullet.\nBeta bullet.") != 0) { ok = 0; printf("FAIL pptx s1 body=%s\n", b0 ? b0 : "<null>"); }
        if (!t1 || strcmp(t1, "Foreign Slide Two") != 0) { ok = 0; printf("FAIL pptx s2 title=%s\n", t1 ? t1 : "<null>"); }
        if (!b1 || strcmp(b1, "Only one.") != 0) { ok = 0; printf("FAIL pptx s2 body=%s\n", b1 ? b1 : "<null>"); }
    }
    wubushow_free(p);
    return ok ? 0 : 1;
}

int main(void) {
    const char *script = "tests/conformance/produce_foreign.py";
    #ifdef FOREIGN_SCRIPT
    script = FOREIGN_SCRIPT;
    #endif
    const char *py = getenv("WUBU_CONFORMANCE_PYTHON");
    if (!py) py = "python3";

    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s %s /tmp/foreign_test", py, script);
    int rc = system(cmd);
    int code = (rc == -1) ? -1 : (rc >> 8) & 0xff;   /* WEXITSTATUS */
    if (code == 2) { printf("FOREIGN SKIPPED (openpyxl/python-pptx unavailable)\n"); return 0; }
    if (code != 0) { printf("FAIL producing foreign files (rc=%d)\n", code); return 1; }

    int failed = 0;
    if (check_xlsx("/tmp/foreign_test/foreign.xlsx") != 0) failed = 1; else printf("FOREIGN XLSX OK\n");
    if (check_pptx("/tmp/foreign_test/foreign.pptx") != 0) failed = 1; else printf("FOREIGN PPTX OK\n");
    if (failed) { printf("FOREIGN READ FAILED\n"); return 1; }
    printf("FOREIGN READ PASSED (openpyxl + python-pptx produced; WuBuOffice consumed)\n");
    return 0;
}
