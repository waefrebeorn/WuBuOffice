/* test_legacy_pdf -- legacy binary readers (.xls/.doc/.ppt) + PDF writer.
 *
 * Two independent-oracle gates:
 *   1. xlwt produces a real .xls; our BIFF8 reader must recover every string
 *      and numeric value. SKIPs cleanly if xlwt is absent.
 *   2. our PDF writer renders a dm_doc; pypdf + pdfminer must parse it and find
 *      the expected text. SKIPs cleanly if pypdf/pdfminer are absent.
 *
 * When genuine Microsoft CFB samples exist on the host, they are additionally
 * decoded to prove the container + text decoders on real third-party binaries
 * (best-effort; never fails the suite if the samples are missing). */

#include "../apps/wubulegacy/legacy.h"
#include "../apps/wubupdf/pdf.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubushow/show.h"
#include "../apps/wubuedit/docmodel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int run(const char *cmd) {
    int rc = system(cmd);
    return (rc == -1) ? -1 : ((rc >> 8) & 0xff);
}
static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }

#ifndef LEGACY_ORACLE
#define LEGACY_ORACLE "tests/conformance/legacy_oracle.py"
#endif

int main(void) {
    if (system("rm -rf /tmp/legtest && mkdir -p /tmp/legtest") != 0) { /* non-fatal */ }
    const char *PY = getenv("WUBU_CONFORMANCE_PYTHON"); if (!PY) PY = "python3";
    int fail = 0, skipped = 0;

    /* ---------- Gate 1: .xls via xlwt ---------- */
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s %s produce_xls /tmp/legtest/legacy.xls", PY, LEGACY_ORACLE);
    int prc = run(cmd);
    if (prc == 2 || !file_exists("/tmp/legtest/legacy.xls")) {
        printf("(xls oracle skipped: xlwt unavailable)\n");
        skipped = 1;
    } else {
        wubucell_book *b = NULL;
        if (wubulegacy_read_xls("/tmp/legtest/legacy.xls", &b) != 0 || !b) {
            printf("FAIL read_xls\n"); fail = 1;
        } else {
            if (wubucell_sheet_count(b) < 2) { printf("FAIL xls sheet count %d\n", wubucell_sheet_count(b)); fail = 1; }
            wubucell_ckind k; const char *t = NULL; double num = 0, cached = 0;
            /* Data!A1 = "Item" */
            if (wubucell_get(b, 1, 1, 1, &k, &t, &num, &cached) != 0 || !t || strcmp(t, "Item") != 0) {
                printf("FAIL xls A1 string (got '%s')\n", t ? t : "(null)"); fail = 1;
            }
            /* Data!C4 = 1520.5 */
            if (wubucell_get(b, 1, 3, 4, &k, &t, &num, &cached) != 0 || fabs(num - 1520.5) > 1e-9) {
                printf("FAIL xls C4 number (got %g)\n", num); fail = 1;
            }
            /* Data!C2 = 19.99 (RK/number path) */
            if (wubucell_get(b, 1, 3, 2, &k, &t, &num, &cached) != 0 || fabs(num - 19.99) > 1e-6) {
                printf("FAIL xls C2 number (got %g)\n", num); fail = 1;
            }
            /* Notes!A1 = string on the second sheet */
            if (wubucell_get(b, 2, 1, 1, &k, &t, &num, &cached) != 0 || !t || strcmp(t, "Second sheet string") != 0) {
                printf("FAIL xls sheet2 A1 (got '%s')\n", t ? t : "(null)"); fail = 1;
            }
            wubucell_free(b);
            if (!fail) printf("xls (xlwt oracle) OK: strings + numbers across 2 sheets\n");
        }
    }

    /* ---------- Gate 2: PDF writer via pypdf/pdfminer ---------- */
    {
        dm_doc d; memset(&d, 0, sizeof d);
        d.cap = 8; d.blocks = calloc(d.cap, sizeof *d.blocks);
        /* heading */
        d.blocks[d.n].kind = DM_BLOCK_PARA;
        d.blocks[d.n].para.style = strdup("Heading1");
        d.blocks[d.n].para.text = strdup("WuBuPDF Gate");
        d.n++;
        /* body long enough to wrap */
        d.blocks[d.n].kind = DM_BLOCK_PARA;
        d.blocks[d.n].para.text = strdup("The quick brown fox jumps over the lazy dog and keeps going for long enough that the line must wrap across the usable page width at least once here.");
        d.n++;
        /* a table */
        d.blocks[d.n].kind = DM_BLOCK_TABLE;
        d.blocks[d.n].table.rows = 2; d.blocks[d.n].table.cols = 2;
        d.blocks[d.n].table.cells = calloc(4, sizeof(dm_para *));
        const char *cellv[4] = {"Name", "Price", "Widget", "1999"};
        for (int i = 0; i < 4; i++) {
            dm_para *cp = calloc(1, sizeof *cp);
            cp->text = strdup(cellv[i]);
            d.blocks[d.n].table.cells[i] = cp;
        }
        d.n++;

        if (wubupdf_write(&d, "/tmp/legtest/out.pdf") != 0 || !file_exists("/tmp/legtest/out.pdf")) {
            printf("FAIL pdf write\n"); fail = 1;
        } else {
            snprintf(cmd, sizeof cmd,
                "%s %s check_pdf /tmp/legtest/out.pdf 'WuBuPDF Gate' 'quick brown fox' 'Widget' '1999'",
                PY, LEGACY_ORACLE);
            int o = run(cmd);
            if (o == 2) { printf("(pdf oracle skipped: pypdf/pdfminer unavailable)\n"); skipped = 1; }
            else if (o != 0) { printf("FAIL pdf oracle (rc=%d)\n", o); fail = 1; }
            else printf("pdf (pypdf+pdfminer oracle) OK: heading + wrapped body + table text\n");
        }
        wubuedit_docmodel_free(&d);
    }

    /* ---------- Best-effort: genuine Microsoft CFB samples on the host ---------- */
    {
        const char *doc = "/mnt/c/Windows/System32/MSDRM/MsoIrmProtector.doc";
        const char *ppt = "/mnt/c/Windows/System32/MSDRM/MsoIrmProtector.ppt";
        const char *xls = "/mnt/c/Windows/System32/MSDRM/MsoIrmProtector.xls";
        if (file_exists(doc)) {
            dm_doc dd; if (wubulegacy_read_doc(doc, &dd) == 0) {
                int found = 0;
                for (size_t i = 0; i < dd.n; i++)
                    if (dd.blocks[i].kind == DM_BLOCK_PARA && dd.blocks[i].para.text &&
                        strstr(dd.blocks[i].para.text, "IRM protected")) found = 1;
                if (found) printf("real MS .doc decoded OK (piece table)\n");
                else printf("(real MS .doc present but no expected text; non-fatal)\n");
                wubuedit_docmodel_free(&dd);
            }
        }
        if (file_exists(ppt)) {
            wubushow_pres *pp = NULL;
            if (wubulegacy_read_ppt(ppt, &pp) == 0 && pp) {
                if (wubushow_slide_count(pp) > 0) printf("real MS .ppt decoded OK (%d slide[s])\n", wubushow_slide_count(pp));
                wubushow_free(pp);
            }
        }
        if (file_exists(xls)) {
            wubucell_book *bb = NULL;
            if (wubulegacy_read_xls(xls, &bb) == 0 && bb) { printf("real MS .xls opened OK (%d sheet[s])\n", wubucell_sheet_count(bb)); wubucell_free(bb); }
        }
    }

    if (fail) { printf("LEGACY+PDF TEST FAILED\n"); return 1; }
    if (skipped) printf("(some oracles skipped; core paths exercised where libs present)\n");
    printf("LEGACY+PDF TEST PASSED\n");
    return 0;
}
