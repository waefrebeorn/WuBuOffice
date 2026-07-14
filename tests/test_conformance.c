#include "../apps/wubuword/word.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubushow/show.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Conformance gate: generate one file of each type with our own apps, then
 * validate each against an INDEPENDENT oracle (Python stdlib xml +
 * openpyxl) via tests/conformance/validate.py. This catches conformance
 * regressions that our own reader would never notice (e.g. the table-per-row
 * and t="s" bugs we already fixed).
 *
 * If the oracle (python3 venv with openpyxl) is unavailable, the test SKIPs
 * (prints "(skipped)" and returns 0) so CI without the optional deps still
 * passes the sanitizer gate. */

static int run(const char *cmd) { return system(cmd); }

static int generate_all(void) {
    /* docx */
    {
        wubuword_doc *d = wubuword_create();
        wubuword_para(d, "Heading1", 0, "Conformance Title");
        wubuword_para(d, NULL, 1, "Bold line for conformance.");
        wubuword_table_begin(d);
        wubuword_row(d); wubuword_cell(d, 1, "A1"); wubuword_cell(d, 0, "B1");
        wubuword_row(d); wubuword_cell(d, 0, "A2"); wubuword_cell(d, 1, "B2");
        wubuword_table_end(d);
        size_t len = 0; char *doc = wubuword_render(d, &len); wubuword_free(d);
        int rc = wubuword_assemble("/tmp/conf.docx", doc, len); free(doc);
        if (rc) return -1;
    }
    /* xlsx with shared strings + a formula (assemble writes the file) */
    {
        wubucell_book *b = wubucell_create();
        int sh = wubucell_sheet(b, "Conformance");   /* 1-based sheet index */
        wubucell_cell_s(b, sh, 1, 1, "Item");
        wubucell_cell_s(b, sh, 2, 1, "Cost");
        wubucell_cell_s(b, sh, 1, 2, "Engine");
        wubucell_cell_n(b, sh, 2, 2, 1200.5);
        wubucell_cell_s(b, sh, 1, 3, "Docs");
        wubucell_cell_n(b, sh, 2, 3, 320);
        wubucell_cell_f(b, sh, 2, 4, "SUM(B2:B3)", 1520.5);
        if (wubucell_assemble(b, "/tmp/conf.xlsx") != 0) { wubucell_free(b); return -1; }
        wubucell_free(b);
    }
    /* pptx */
    {
        wubushow_pres *p = wubushow_create();
        wubushow_slide(p, "Conformance Slide", "First bullet.\nSecond bullet.");
        if (wubushow_assemble(p, "/tmp/conf.pptx") != 0) { wubushow_free(p); return -1; }
        wubushow_free(p);
    }
    return 0;
}

int main(void) {
    if (generate_all() != 0) { printf("FAIL generate\n"); return 1; }

    /* locate the oracle script (source tree, via CMake define) */
    const char *script = "tests/conformance/validate.py";
    #ifdef CONFORMANCE_SCRIPT
    script = CONFORMANCE_SCRIPT;
    #endif
    const char *py = getenv("WUBU_CONFORMANCE_PYTHON");
    if (!py) py = "python3";

    int failed = 0, skipped = 0, checked = 0;
    const char *files[] = {"/tmp/conf.docx", "/tmp/conf.xlsx", "/tmp/conf.pptx"};
    for (size_t i = 0; i < 3; i++) {
        char cmd[1024];
        snprintf(cmd, sizeof cmd, "%s %s %s", py, script, files[i]);
        int rc = run(cmd);
        if (rc == 2) { printf("conformance: %s -> oracle unavailable (skipped)\n", files[i]); skipped++; }
        else if (rc == 0) { printf("conformance: %s -> CONFORMANT\n", files[i]); checked++; }
        else { printf("conformance: %s -> FAILED\n", files[i]); failed++; }
    }

    if (failed) { printf("CONFORMANCE FAILED (%d)\n", failed); return 1; }
    if (skipped && !checked) { printf("CONFORMANCE SKIPPED (no oracle)\n"); return 0; }
    printf("CONFORMANCE PASSED (%d checked, %d skipped)\n", checked, skipped);
    return 0;
}
