/* test_flat_epub -- flat ODF (.fodt/.fods/.fodp) round trips + EPUB writer.
 *
 * Gate 1 (flat ODF): write each flat variant from a model, validate the file is
 * well-formed <office:document> XML with the right inner root + expected text
 * (stdlib XML oracle, never skips), then read it back with our flat reader and
 * assert the model survived.
 *
 * Gate 2 (EPUB): render a multi-chapter dm_doc to .epub and validate it with
 * EbookLib (independent third-party EPUB library). SKIPs cleanly if absent. */

#include "../apps/wubuodf/odf.h"
#include "../apps/wubudoc/epub.h"
#include "../apps/wubuedit/docmodel.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubushow/show.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(const char *cmd) { int rc = system(cmd); return (rc == -1) ? -1 : ((rc >> 8) & 0xff); }
static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }

#ifndef FLAT_EPUB_ORACLE
#define FLAT_EPUB_ORACLE "tests/conformance/flat_epub_oracle.py"
#endif

int main(void) {
    if (system("rm -rf /tmp/fetest && mkdir -p /tmp/fetest") != 0) { /* non-fatal */ }
    const char *PY = getenv("WUBU_CONFORMANCE_PYTHON"); if (!PY) PY = "python3";
    int fail = 0, skipped = 0;
    char cmd[1024];

    /* ---------- Gate 1a: flat ODT ---------- */
    {
        dm_doc d; memset(&d, 0, sizeof d);
        d.cap = 4; d.blocks = calloc(d.cap, sizeof *d.blocks);
        d.blocks[d.n].kind = DM_BLOCK_PARA; d.blocks[d.n].para.style = strdup("Heading1"); d.blocks[d.n].para.text = strdup("Flat Title"); d.n++;
        d.blocks[d.n].kind = DM_BLOCK_PARA; d.blocks[d.n].para.text = strdup("Flat body paragraph."); d.n++;
        if (wubuodf_write_fodt(&d, "/tmp/fetest/a.fodt") != 0) { printf("FAIL write fodt\n"); fail = 1; }
        wubuedit_docmodel_free(&d);

        snprintf(cmd, sizeof cmd, "%s %s check_flat /tmp/fetest/a.fodt office:text 'Flat Title' 'Flat body paragraph.'", PY, FLAT_EPUB_ORACLE);
        if (run(cmd) != 0) { printf("FAIL fodt oracle\n"); fail = 1; }

        dm_doc r; if (wubuodf_read_fodt("/tmp/fetest/a.fodt", &r) != 0) { printf("FAIL read fodt\n"); fail = 1; }
        else {
            int found_title = 0, found_body = 0;
            for (size_t i = 0; i < r.n; i++) if (r.blocks[i].kind == DM_BLOCK_PARA && r.blocks[i].para.text) {
                if (strcmp(r.blocks[i].para.text, "Flat Title") == 0) found_title = 1;
                if (strcmp(r.blocks[i].para.text, "Flat body paragraph.") == 0) found_body = 1;
            }
            if (!found_title || !found_body) { printf("FAIL fodt roundtrip (title=%d body=%d)\n", found_title, found_body); fail = 1; }
            wubuedit_docmodel_free(&r);
        }
    }

    /* ---------- Gate 1b: flat ODS ---------- */
    {
        wubucell_book *b = wubucell_create();
        int sh = wubucell_sheet(b, "Data");
        wubucell_cell_s(b, sh, 1, 1, "Item");
        wubucell_cell_n(b, sh, 2, 1, 1520.5);
        if (wubuodf_write_fods(b, "/tmp/fetest/a.fods") != 0) { printf("FAIL write fods\n"); fail = 1; }
        wubucell_free(b);

        snprintf(cmd, sizeof cmd, "%s %s check_flat /tmp/fetest/a.fods office:spreadsheet 'Item' '1520.5'", PY, FLAT_EPUB_ORACLE);
        if (run(cmd) != 0) { printf("FAIL fods oracle\n"); fail = 1; }

        wubucell_book *rb = NULL;
        if (wubuodf_read_fods("/tmp/fetest/a.fods", &rb) != 0 || !rb) { printf("FAIL read fods\n"); fail = 1; }
        else {
            wubucell_ckind k; const char *t = NULL; double num = 0, cached = 0;
            if (wubucell_get(rb, 1, 1, 1, &k, &t, &num, &cached) != 0 || !t || strcmp(t, "Item") != 0) { printf("FAIL fods A1\n"); fail = 1; }
            wubucell_free(rb);
        }
    }

    /* ---------- Gate 1c: flat ODP ---------- */
    {
        wubushow_pres *p = wubushow_create();
        wubushow_slide(p, "Slide Alpha", "line one\nline two");
        if (wubuodf_write_fodp(p, "/tmp/fetest/a.fodp") != 0) { printf("FAIL write fodp\n"); fail = 1; }
        wubushow_free(p);

        snprintf(cmd, sizeof cmd, "%s %s check_flat /tmp/fetest/a.fodp office:presentation 'Slide Alpha' 'line one'", PY, FLAT_EPUB_ORACLE);
        if (run(cmd) != 0) { printf("FAIL fodp oracle\n"); fail = 1; }

        wubushow_pres *rp = NULL;
        if (wubuodf_read_fodp("/tmp/fetest/a.fodp", &rp) != 0 || !rp) { printf("FAIL read fodp\n"); fail = 1; }
        else {
            if (wubushow_slide_count(rp) < 1) { printf("FAIL fodp slide count\n"); fail = 1; }
            else {
                const char *title = NULL, *body = NULL;
                wubushow_slide_get(rp, 0, &title, &body);
                if (!title || strcmp(title, "Slide Alpha") != 0) { printf("FAIL fodp title (got '%s')\n", title ? title : "(null)"); fail = 1; }
            }
            wubushow_free(rp);
        }
    }

    /* ---------- Gate 2: EPUB via EbookLib ---------- */
    {
        dm_doc d; memset(&d, 0, sizeof d);
        d.cap = 8; d.blocks = calloc(d.cap, sizeof *d.blocks);
        d.blocks[d.n].kind = DM_BLOCK_PARA; d.blocks[d.n].para.style = strdup("Heading1"); d.blocks[d.n].para.text = strdup("Chapter One"); d.n++;
        d.blocks[d.n].kind = DM_BLOCK_PARA; d.blocks[d.n].para.text = strdup("The first chapter body text."); d.n++;
        d.blocks[d.n].kind = DM_BLOCK_PARA; d.blocks[d.n].para.style = strdup("Heading1"); d.blocks[d.n].para.text = strdup("Chapter Two"); d.n++;
        d.blocks[d.n].kind = DM_BLOCK_PARA; d.blocks[d.n].para.text = strdup("The second chapter body text."); d.n++;
        if (wubudoc_write_epub(&d, "/tmp/fetest/book.epub") != 0 || !file_exists("/tmp/fetest/book.epub")) { printf("FAIL write epub\n"); fail = 1; }
        wubuedit_docmodel_free(&d);

        snprintf(cmd, sizeof cmd, "%s %s check_epub /tmp/fetest/book.epub 'Chapter One' 'Chapter Two' 'first chapter body' 'second chapter body'", PY, FLAT_EPUB_ORACLE);
        int o = run(cmd);
        if (o == 2) { printf("(epub oracle skipped: EbookLib unavailable)\n"); skipped = 1; }
        else if (o != 0) { printf("FAIL epub oracle (rc=%d)\n", o); fail = 1; }
        else printf("epub (EbookLib oracle) OK: 2 chapters, nav + spine\n");
    }

    if (fail) { printf("FLAT+EPUB TEST FAILED\n"); return 1; }
    if (skipped) printf("(epub oracle skipped; flat-ODF gates exercised regardless)\n");
    printf("FLAT+EPUB TEST PASSED\n");
    return 0;
}
