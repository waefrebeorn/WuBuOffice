/* test_csv -- RFC 4180 CSV/TSV import + export round-trip for wubucell.
 *
 * Exercises the tricky cases: a field with an embedded delimiter, a field with
 * an embedded newline, doubled ("escaped") quotes, and numeric auto-detection.
 * Then round-trips book -> CSV -> book and checks the values survive. */

#include "../apps/wubucell/cell.h"
#include "../apps/wubucell/cell_csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_str(wubucell_book *b, int c, int r, const char *want) {
    wubucell_ckind k; const char *t = NULL; double n = 0, ca = 0;
    if (wubucell_get(b, 1, c, r, &k, &t, &n, &ca) != 0 || k != WUBUCELL_STR || !t || strcmp(t, want) != 0) {
        printf("FAIL [%d,%d] want str '%s' got '%s'\n", r, c, want, t ? t : "<none>");
        return 1;
    }
    return 0;
}
static int expect_num(wubucell_book *b, int c, int r, double want) {
    wubucell_ckind k; const char *t = NULL; double n = 0, ca = 0;
    if (wubucell_get(b, 1, c, r, &k, &t, &n, &ca) != 0 || k != WUBUCELL_NUM || n != want) {
        printf("FAIL [%d,%d] want num %g got %g\n", r, c, want, n);
        return 1;
    }
    return 0;
}

int main(void) {
    const char *in = "/tmp/test_csv_in.csv";
    /* write a tricky RFC 4180 file */
    FILE *f = fopen(in, "wb");
    if (!f) { printf("FAIL open\n"); return 1; }
    fputs("Name,Amount,Note\r\n", f);
    fputs("\"Smith, John\",1200.5,\"line1\nline2\"\r\n", f);
    fputs("O'Brien,320,\"quote\"\"here\"\"\"\r\n", f);
    fclose(f);

    wubucell_book *b = NULL;
    if (wubucell_read_csv(in, ',', &b) != 0) { printf("FAIL read_csv\n"); return 1; }

    int rc = 0;
    int mc = 0, mr = 0; wubucell_sheet_dims(b, 1, &mc, &mr);
    if (mc != 3 || mr != 3) { printf("FAIL dims %dx%d (want 3x3)\n", mc, mr); rc = 1; }
    rc |= expect_str(b, 1, 1, "Name");
    rc |= expect_str(b, 1, 2, "Smith, John");     /* embedded delimiter */
    rc |= expect_num(b, 2, 2, 1200.5);            /* numeric auto-detect */
    rc |= expect_str(b, 3, 2, "line1\nline2");    /* embedded newline */
    rc |= expect_str(b, 1, 3, "O'Brien");
    rc |= expect_num(b, 2, 3, 320);
    rc |= expect_str(b, 3, 3, "quote\"here\"");    /* doubled quotes */

    /* round-trip: book -> CSV -> book */
    const char *out = "/tmp/test_csv_out.csv";
    if (wubucell_write_csv(b, 1, ',', out) != 0) { printf("FAIL write_csv\n"); rc = 1; }
    wubucell_book *b2 = NULL;
    if (wubucell_read_csv(out, ',', &b2) != 0) { printf("FAIL reread\n"); rc = 1; }
    else {
        rc |= expect_str(b2, 1, 2, "Smith, John");
        rc |= expect_num(b2, 2, 2, 1200.5);
        rc |= expect_str(b2, 3, 2, "line1\nline2");
        rc |= expect_str(b2, 3, 3, "quote\"here\"");
        wubucell_free(b2);
    }

    /* TSV smoke test: delimiter independence */
    const char *tsv = "/tmp/test_csv_out.tsv";
    if (wubucell_write_csv(b, 1, '\t', tsv) != 0) { printf("FAIL write_tsv\n"); rc = 1; }
    wubucell_book *b3 = NULL;
    if (wubucell_read_csv(tsv, '\t', &b3) != 0) { printf("FAIL read_tsv\n"); rc = 1; }
    else { rc |= expect_str(b3, 1, 2, "Smith, John"); wubucell_free(b3); }

    wubucell_free(b);
    if (rc) { printf("CSV TEST FAILED\n"); return 1; }
    printf("CSV TEST PASSED (RFC 4180 quoting + newline + numeric + TSV round-trip)\n");
    return 0;
}
