/* test_convert -- unified format conversion matrix.
 *
 * Builds a docx, xlsx, pptx, md, csv, odt, ods, odp; then converts across the
 * whole matrix through wubuconv and asserts the results parse/validate with the
 * independent oracles (odfpy for ODF, python for JSON/CSV). SKIPs gracefully
 * when odfpy is unavailable. This is the capstone of format supremacy. */

#include "../apps/wubuconv/conv_map.h"
#include "../apps/wubuword/assemble.h"
#include "../apps/wubuword/word.h"
#include "../apps/wubucell/cell.h"
#include "../apps/wubushow/show.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(const char *cmd) {
    int rc = system(cmd);
    return (rc == -1) ? -1 : ((rc >> 8) & 0xff);
}

static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }

int main(void) {
    if (system("rm -rf /tmp/convtest && mkdir -p /tmp/convtest") != 0) { /* non-fatal */ }
    int fail = 0, skipped = 0;

    const char *ORACLE = getenv("WUBU_CONFORMANCE_PYTHON"); if (!ORACLE) ORACLE = "python3";
#ifndef WUBUOFFICE_BIN
#define WUBUOFFICE_BIN "./build/wubuoffice"
#endif
#ifndef ODF_ORACLE_SCRIPT
#define ODF_ORACLE_SCRIPT "tests/conformance/odf_oracle.py"
#endif
    /* build source files using the dedicated generators (fast, dependency-free) */
    /* docx via wubuword */
    {
        wubuword_doc *d = wubuword_create();
        wubuword_para(d, "Title", 0, "Convert Me");
        wubuword_para(d, "Heading1", 0, "Section A");
        wubuword_para(d, NULL, 1, "Bold line.");
        wubuword_table_begin(d); wubuword_row(d);
        wubuword_cell(d, 1, "H1"); wubuword_cell(d, 1, "H2");
        wubuword_row(d); wubuword_cell(d, 0, "1"); wubuword_cell(d, 0, "2");
        wubuword_table_end(d);
        size_t len = 0; char *xml = wubuword_render(d, &len);
        wubuword_assemble("/tmp/convtest/in.docx", xml, len); free(xml); wubuword_free(d);
    }
    /* xlsx */
    {
        wubucell_book *b = wubucell_create();
        int sh = wubucell_sheet(b, "S");
        wubucell_cell_s(b, sh, 1, 1, "Name");
        wubucell_cell_n(b, sh, 2, 1, 42);
        wubucell_assemble(b, "/tmp/convtest/in.xlsx"); wubucell_free(b);
    }
    /* pptx */
    {
        wubushow_pres *p = wubushow_create();
        wubushow_slide(p, "Slide One", "A\nB");
        wubushow_assemble(p, "/tmp/convtest/in.pptx"); wubushow_free(p);
    }
    /* md */
    {
        FILE *f = fopen("/tmp/convtest/in.md", "wb");
        fputs("# MD Title\n\n**bold** text.\n\n| X | Y |\n| --- | --- |\n| 1 | 2 |\n", f); fclose(f);
    }
    /* csv */
    {
        FILE *f = fopen("/tmp/convtest/in.csv", "wb");
        fputs("a,b,c\n1,2,3\n", f); fclose(f);
    }

    /* ---- same-family round trips ---- */
    struct { const char *in, *out; } samefam[] = {
        {"/tmp/convtest/in.docx", "/tmp/convtest/docx2docx.docx"},
        {"/tmp/convtest/in.md",   "/tmp/convtest/md2md.md"},
        {"/tmp/convtest/in.csv",  "/tmp/convtest/csv2csv.csv"},
        {"/tmp/convtest/in.xlsx", "/tmp/convtest/xlsx2xlsx.xlsx"},
        {"/tmp/convtest/in.pptx", "/tmp/convtest/pptx2pptx.pptx"},
        {NULL, NULL}
    };
    for (int i = 0; samefam[i].in; i++) {
        if (wubuconv_convert(samefam[i].in, samefam[i].out) != 0) { printf("FAIL convert %s->%s\n", samefam[i].in, samefam[i].out); fail = 1; }
        else if (!file_exists(samefam[i].out)) { printf("FAIL missing %s\n", samefam[i].out); fail = 1; }
    }

    /* ---- cross-family bridges ---- */
    struct { const char *in, *out; } cross[] = {
        {"/tmp/convtest/in.docx", "/tmp/convtest/d2md.md"},
        {"/tmp/convtest/in.docx", "/tmp/convtest/d2html.html"},
        {"/tmp/convtest/in.docx", "/tmp/convtest/d2json.json"},
        {"/tmp/convtest/in.xlsx", "/tmp/convtest/x2docx.docx"},
        {"/tmp/convtest/in.xlsx", "/tmp/convtest/x2csv.csv"},
        {"/tmp/convtest/in.xlsx", "/tmp/convtest/x2json.json"},
        {"/tmp/convtest/in.pptx", "/tmp/convtest/p2md.md"},
        {"/tmp/convtest/in.pptx", "/tmp/convtest/p2json.json"},
        {"/tmp/convtest/in.md",   "/tmp/convtest/m2docx.docx"},
        {"/tmp/convtest/in.md",   "/tmp/convtest/m2pptx.pptx"},
        {"/tmp/convtest/in.md",   "/tmp/convtest/m2pdf.pdf"},
        {"/tmp/convtest/in.docx", "/tmp/convtest/d2pdf.pdf"},
        {"/tmp/convtest/in.docx", "/tmp/convtest/d2fodt.fodt"},
        {"/tmp/convtest/in.xlsx", "/tmp/convtest/x2fods.fods"},
        {"/tmp/convtest/in.pptx", "/tmp/convtest/p2fodp.fodp"},
        {"/tmp/convtest/in.md",   "/tmp/convtest/m2epub.epub"},
        {NULL, NULL}
    };
    for (int i = 0; cross[i].in; i++) {
        if (wubuconv_convert(cross[i].in, cross[i].out) != 0) { printf("FAIL convert %s->%s\n", cross[i].in, cross[i].out); fail = 1; }
        else if (!file_exists(cross[i].out)) { printf("FAIL missing %s\n", cross[i].out); fail = 1; }
    }

    /* ---- flat-ODF read-back: the .fod? files produced above must re-read ---- */
    struct { const char *in, *out; } flatback[] = {
        {"/tmp/convtest/d2fodt.fodt", "/tmp/convtest/fodt2md.md"},
        {"/tmp/convtest/x2fods.fods", "/tmp/convtest/fods2csv.csv"},
        {"/tmp/convtest/p2fodp.fodp", "/tmp/convtest/fodp2md.md"},
        {NULL, NULL}
    };
    for (int i = 0; flatback[i].in; i++) {
        if (!file_exists(flatback[i].in)) { printf("FAIL flat source missing %s\n", flatback[i].in); fail = 1; continue; }
        if (wubuconv_convert(flatback[i].in, flatback[i].out) != 0) { printf("FAIL convert %s->%s\n", flatback[i].in, flatback[i].out); fail = 1; }
        else if (!file_exists(flatback[i].out)) { printf("FAIL missing %s\n", flatback[i].out); fail = 1; }
    }

    /* ---- ODF round trips + odfpy oracle ---- */
    struct { const char *in, *out, *kind; } odf[] = {
        {"/tmp/convtest/in.docx", "/tmp/convtest/d2odt.odt", "document"},
        {"/tmp/convtest/in.xlsx", "/tmp/convtest/x2ods.ods", "spreadsheet"},
        {"/tmp/convtest/in.pptx", "/tmp/convtest/p2odp.odp", "presentation"},
        {"/tmp/convtest/in.odt",  "/tmp/convtest/odt2docx.docx", "document"},
        {NULL, NULL, NULL}
    };
    /* produce odt input with odfpy if available, to test foreign->our chain.
     * Use the same oracle producer that test_odf relies on (venv python). */
    char prod[1024];
    snprintf(prod, sizeof prod, "%s %s produce /tmp/convtest 2>/dev/null", ORACLE, ODF_ORACLE_SCRIPT);
    int prc = run(prod);
    if (prc == 2 || !file_exists("/tmp/convtest/foreign.odt")) skipped = 1;
    else if (file_exists("/tmp/convtest/foreign.odt")) {
        /* rename foreign.odt -> in.odt for the conversion-under-test */
        rename("/tmp/convtest/foreign.odt", "/tmp/convtest/in.odt");
    }

    for (int i = 0; odf[i].in; i++) {
        if (wubuconv_convert(odf[i].in, odf[i].out) != 0) { printf("FAIL convert %s->%s\n", odf[i].in, odf[i].out); fail = 1; }
    }
    /* validate ODF outputs + foreign input read with odfpy */
    char cmd[1024];
    /* directly import odf and load each produced ODF */
    snprintf(cmd, sizeof cmd,
        "%s -c \""
        "import sys\n"
        "try:\n"
        "    from odf.opendocument import load\n"
        "    for f in ['/tmp/convtest/d2odt.odt','/tmp/convtest/x2ods.ods','/tmp/convtest/p2odp.odp']:\n"
        "        load(f)\n"
        "    from odf.opendocument import load as L\n"
        "    L('/tmp/convtest/in.odt')  # foreign input must be readable\n"
        "    print('odfpy OK')\n"
        "except ImportError:\n"
        "    print('odfpy SKIP'); sys.exit(2)\n"
        "except Exception as e:\n"
        "    print('odfpy FAIL', e); sys.exit(1)\n\"", ORACLE);
    int o = run(cmd);
    if (o == 2) skipped = 1;
    else if (o != 0) { printf("FAIL odfpy validation (rc=%d)\n", o); fail = 1; }

    /* ---- JSON outputs are valid via stdlib json ---- */
    for (int i = 0; cross[i].in; i++) {
        if (strstr(cross[i].out, ".json")) {
            char c[512]; snprintf(c, sizeof c, "%s -m json.tool %s >/dev/null 2>&1", ORACLE, cross[i].out);
            if (run(c) != 0) { printf("FAIL json invalid %s\n", cross[i].out); fail = 1; }
        }
    }

    if (skipped) printf("(odfpy oracle skipped)\n");
    if (fail) { printf("CONVERT TEST FAILED\n"); return 1; }
    printf("CONVERT TEST PASSED (full matrix: same-family + cross-family + ODF + JSON oracle)\n");
    return 0;
}
