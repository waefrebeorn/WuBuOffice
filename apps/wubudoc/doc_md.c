/* doc_md.c -- Markdown & HTML serializers for the WuBuOffice document model.
 * See doc_md.h. Clean-room C11, no third-party code. */

#include "doc_md.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- small helpers ---- */

static char *slurp(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *d = malloc(sz ? (size_t)sz + 1 : 1);
    if (!d) { fclose(f); return NULL; }
    if (fread(d, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(d); return NULL; }
    fclose(f);
    d[sz] = '\0';
    if (out_len) *out_len = (size_t)sz;
    return d;
}

/* Strip inline **bold** / __bold__ markers from `s` in place, returning 1 if
 * any were present (paragraph-level bold, matching the model's granularity). */
static int strip_bold(char *s) {
    int had = 0;
    char *w = s;
    for (char *r = s; *r; ) {
        if ((r[0] == '*' && r[1] == '*') || (r[0] == '_' && r[1] == '_')) {
            had = 1; r += 2; continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
    return had;
}

static char *rstrip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = '\0';
    return s;
}
static char *lstrip(char *s) { while (*s == ' ' || *s == '\t') s++; return s; }

/* Split a table row "| a | b |" into trimmed fields. Returns count; fields[]
 * point into `line` (which is modified in place). */
static int split_row(char *line, char **fields, int cap) {
    int n = 0;
    char *p = lstrip(line);
    if (*p == '|') p++;               /* optional leading pipe */
    char *start = p;
    for (; ; p++) {
        if (*p == '|' || *p == '\0') {
            char save = *p;
            *p = '\0';
            if (n < cap) fields[n++] = rstrip(lstrip(start));
            start = p + 1;
            if (save == '\0') break;
        }
    }
    /* drop a trailing empty field created by a trailing pipe */
    if (n > 0 && fields[n-1][0] == '\0') n--;
    return n;
}

static int is_table_sep(const char *line) {
    /* a separator row is only | : - and spaces, with at least one - */
    int dash = 0;
    for (const char *p = line; *p; p++) {
        if (*p == '-') dash = 1;
        else if (*p != '|' && *p != ':' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            return 0;
    }
    return dash;
}

/* ---- Markdown import ---- */

int wubudoc_read_md(const char *path, wubuword_doc **out) {
    size_t len = 0;
    char *data = slurp(path, &len);
    if (!data) return -1;

    wubuword_doc *d = wubuword_create();
    if (!d) { free(data); return -1; }

    /* iterate lines */
    char *save = data;
    char *line;
    char *cursor = data;
    /* We tokenize manually to keep embedded handling simple. */
    (void)save;
    while (cursor && *cursor) {
        char *nl = strchr(cursor, '\n');
        if (nl) *nl = '\0';
        line = cursor;

        char *t = lstrip(line);
        rstrip(t);

        if (t[0] == '\0') {
            /* blank line: paragraph break -- nothing to emit */
        } else if (is_table_sep(t) ) {
            /* separator handled as part of table lookahead below; skip if seen alone */
        } else if (t[0] == '#') {
            int level = 0;
            while (t[level] == '#') level++;
            char *txt = lstrip(t + level);
            const char *style = (level == 1) ? "Heading1" : (level == 2) ? "Heading2" : "Heading3";
            char *body = strdup(txt);
            int bold = strip_bold(body);
            wubuword_para(d, style, bold, body);
            free(body);
        } else if (t[0] == '-' || (t[0] == '*' && t[1] == ' ')) {
            /* bullet list item: emit "• text" as a normal paragraph */
            char *txt = lstrip(t + 1);
            char *body = malloc(strlen(txt) + 4);
            char *tmp = strdup(txt);
            int bold = strip_bold(tmp);
            sprintf(body, "\xE2\x80\xA2 %s", tmp);   /* U+2022 bullet */
            wubuword_para(d, NULL, bold, body);
            free(body); free(tmp);
        } else if (t[0] == '|') {
            /* table: gather consecutive pipe lines */
            wubuword_table_begin(d);
            int row_index = 0;
            char *tl = t;
            for (;;) {
                if (!is_table_sep(tl)) {
                    char *fields[64];
                    /* split_row modifies the buffer; work on a copy per row */
                    char *rowcopy = strdup(tl);
                    int nf = split_row(rowcopy, fields, 64);
                    wubuword_row(d);
                    int header = (row_index == 0);
                    for (int i = 0; i < nf; i++) {
                        char *cell = strdup(fields[i]);
                        int bold = strip_bold(cell) || header;
                        wubuword_cell(d, bold, cell);
                        free(cell);
                    }
                    free(rowcopy);
                    row_index++;
                }
                /* lookahead to next line */
                if (!nl) break;
                char *next = nl + 1;
                char *nnl = strchr(next, '\n');
                char *peek = next;
                /* trim to check */
                char *pt = lstrip(peek);
                if (pt[0] != '|') break;   /* table ended */
                if (nnl) *nnl = '\0';
                rstrip(pt);
                tl = pt;
                nl = nnl;
            }
            wubuword_table_end(d);
        } else {
            /* normal paragraph */
            char *body = strdup(t);
            int bold = strip_bold(body);
            wubuword_para(d, NULL, bold, body);
            free(body);
        }

        if (!nl) break;
        cursor = nl + 1;
    }

    free(data);
    *out = d;
    return 0;
}

/* ---- Markdown export (from a parsed dm_doc) ---- */

int wubudoc_write_md(const dm_doc *d, const char *path) {
    if (!d) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    for (size_t i = 0; i < d->n; i++) {
        const dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            const char *style = b->para.style;
            const char *txt = b->para.text ? b->para.text : "";
            const char *prefix = "";
            if (style) {
                if (strcmp(style, "Heading1") == 0 || strcmp(style, "Title") == 0) prefix = "# ";
                else if (strcmp(style, "Heading2") == 0) prefix = "## ";
                else if (strcmp(style, "Heading3") == 0) prefix = "### ";
            }
            if (b->para.bold && !*prefix) fprintf(f, "**%s**\n\n", txt);
            else fprintf(f, "%s%s\n\n", prefix, txt);
        } else { /* table */
            const dm_table *t = &b->table;
            for (size_t r = 0; r < t->rows; r++) {
                fputc('|', f);
                for (size_t c = 0; c < t->cols; c++) {
                    dm_para *cell = t->cells[r * t->cols + c];
                    fprintf(f, " %s |", (cell && cell->text) ? cell->text : "");
                }
                fputc('\n', f);
                if (r == 0) { /* header separator */
                    fputc('|', f);
                    for (size_t c = 0; c < t->cols; c++) fputs(" --- |", f);
                    fputc('\n', f);
                }
            }
            fputc('\n', f);
        }
    }
    fclose(f);
    return 0;
}

/* ---- HTML export ---- */

static void html_escape(FILE *f, const char *s) {
    for (; s && *s; s++) {
        switch (*s) {
            case '&': fputs("&amp;", f); break;
            case '<': fputs("&lt;", f); break;
            case '>': fputs("&gt;", f); break;
            case '"': fputs("&quot;", f); break;
            default: fputc(*s, f);
        }
    }
}

int wubudoc_write_html(const dm_doc *d, const char *path) {
    if (!d) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fputs("<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n", f);
    fputs("<title>WuBuOffice document</title>\n</head>\n<body>\n", f);

    for (size_t i = 0; i < d->n; i++) {
        const dm_block *b = &d->blocks[i];
        if (b->kind == DM_BLOCK_PARA) {
            const char *style = b->para.style;
            const char *txt = b->para.text ? b->para.text : "";
            const char *tag = NULL;
            if (style) {
                if (strcmp(style, "Heading1") == 0 || strcmp(style, "Title") == 0) tag = "h1";
                else if (strcmp(style, "Heading2") == 0) tag = "h2";
                else if (strcmp(style, "Heading3") == 0) tag = "h3";
            }
            if (tag) { fprintf(f, "<%s>", tag); html_escape(f, txt); fprintf(f, "</%s>\n", tag); }
            else if (b->para.bold) { fputs("<p><strong>", f); html_escape(f, txt); fputs("</strong></p>\n", f); }
            else { fputs("<p>", f); html_escape(f, txt); fputs("</p>\n", f); }
        } else {
            const dm_table *t = &b->table;
            fputs("<table border=\"1\">\n", f);
            for (size_t r = 0; r < t->rows; r++) {
                fputs("<tr>", f);
                for (size_t c = 0; c < t->cols; c++) {
                    dm_para *cell = t->cells[r * t->cols + c];
                    const char *cd = (cell && cell->text) ? cell->text : "";
                    int th = (r == 0) || (cell && cell->bold);
                    fputs(th ? "<th>" : "<td>", f);
                    html_escape(f, cd);
                    fputs(th ? "</th>" : "</td>", f);
                }
                fputs("</tr>\n", f);
            }
            fputs("</table>\n", f);
        }
    }

    fputs("</body>\n</html>\n", f);
    fclose(f);
    return 0;
}
