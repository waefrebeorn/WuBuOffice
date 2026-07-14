/* cell_csv.c -- CSV / TSV import & export for the wubucell spreadsheet model.
 * See cell_csv.h. RFC 4180 compliant. Clean-room C11, no third-party code. */

#include "cell_csv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- import ---- */

/* Return 1 if `s` is a clean decimal number (optionally signed, one '.', with
 * at least one digit) that strtod would consume entirely. */
static int is_number(const char *s) {
    if (!s || !*s) return 0;
    char *end = NULL;
    strtod(s, &end);
    return end && *end == '\0';
}

/* Parse one RFC 4180 record starting at *pp. Fills `fields`/`flens` (up to
 * `cap`), returns field count. Advances *pp past the record's line terminator.
 * Handles quoted fields with embedded delim, CR/LF, and doubled quotes. */
static int parse_record(const char **pp, const char *end, char delim,
                        char **fields, size_t *flens, int cap) {
    const char *p = *pp;
    int nf = 0;
    while (nf < cap) {
        /* one field */
        char *buf = NULL; size_t len = 0, bcap = 0;
        int quoted = 0;
        if (p < end && *p == '"') { quoted = 1; p++; }
        for (;;) {
            if (quoted) {
                if (p >= end) break;
                if (*p == '"') {
                    if (p + 1 < end && p[1] == '"') { /* escaped quote */
                        if (len + 1 >= bcap) { bcap = bcap ? bcap * 2 : 16; buf = realloc(buf, bcap); }
                        buf[len++] = '"'; p += 2; continue;
                    }
                    p++; quoted = 0; continue;   /* closing quote */
                }
                if (len + 1 >= bcap) { bcap = bcap ? bcap * 2 : 16; buf = realloc(buf, bcap); }
                buf[len++] = *p++;
            } else {
                if (p >= end || *p == delim || *p == '\n' || *p == '\r') break;
                if (len + 1 >= bcap) { bcap = bcap ? bcap * 2 : 16; buf = realloc(buf, bcap); }
                buf[len++] = *p++;
            }
        }
        if (!buf) { buf = malloc(1); }
        buf[len] = '\0';
        fields[nf] = buf; flens[nf] = len; nf++;

        if (p < end && *p == delim) { p++; continue; }  /* next field */
        break;                                           /* end of record */
    }
    /* consume line terminator (CRLF, LF, or CR) */
    if (p < end && *p == '\r') p++;
    if (p < end && *p == '\n') p++;
    *pp = p;
    return nf;
}

int wubucell_read_csv(const char *path, char delim, wubucell_book **out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *data = malloc(sz ? (size_t)sz : 1);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return -1; }
    fclose(f);

    wubucell_book *b = wubucell_create();
    if (!b) { free(data); return -1; }
    int sh = wubucell_sheet(b, "Sheet1");

    const char *p = data, *end = data + sz;
    int row = 0;
    enum { MAXF = 4096 };
    char *fields[MAXF]; size_t flens[MAXF];
    while (p < end) {
        int nf = parse_record(&p, end, delim, fields, flens, MAXF);
        /* Skip a trailing empty record (file ends with a newline). */
        int all_empty = 1;
        for (int i = 0; i < nf; i++) if (flens[i]) { all_empty = 0; break; }
        if (!(all_empty && nf <= 1)) {
            row++;
            for (int i = 0; i < nf; i++) {
                if (flens[i] > 0) {
                    if (is_number(fields[i]))
                        wubucell_cell_n(b, sh, i + 1, row, strtod(fields[i], NULL));
                    else
                        wubucell_cell_s(b, sh, i + 1, row, fields[i]);
                }
            }
        }
        for (int i = 0; i < nf; i++) free(fields[i]);
    }

    free(data);
    *out = b;
    return 0;
}

/* ---- export ---- */

/* Write one field with RFC 4180 quoting when needed. */
static void write_field(FILE *f, const char *s, char delim) {
    int need_quote = 0;
    for (const char *c = s; *c; c++)
        if (*c == delim || *c == '"' || *c == '\n' || *c == '\r') { need_quote = 1; break; }
    if (!need_quote) { fputs(s, f); return; }
    fputc('"', f);
    for (const char *c = s; *c; c++) {
        if (*c == '"') fputc('"', f);   /* double it */
        fputc(*c, f);
    }
    fputc('"', f);
}

int wubucell_write_csv(const wubucell_book *b, int sheet, char delim, const char *path) {
    int max_col = 0, max_row = 0;
    if (wubucell_sheet_dims(b, sheet, &max_col, &max_row) != 0) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    for (int r = 1; r <= max_row; r++) {
        for (int c = 1; c <= max_col; c++) {
            if (c > 1) fputc(delim, f);
            wubucell_ckind k; const char *text = NULL; double num = 0, cached = 0;
            if (wubucell_get(b, sheet, c, r, &k, &text, &num, &cached) == 0) {
                char numbuf[64];
                if (k == WUBUCELL_STR) {
                    write_field(f, text ? text : "", delim);
                } else if (k == WUBUCELL_NUM) {
                    snprintf(numbuf, sizeof numbuf, "%g", num);
                    fputs(numbuf, f);
                } else { /* formula: emit the cached numeric result */
                    snprintf(numbuf, sizeof numbuf, "%g", cached);
                    fputs(numbuf, f);
                }
            }
        }
        fputs("\r\n", f);   /* RFC 4180 uses CRLF */
    }
    fclose(f);
    return 0;
}
