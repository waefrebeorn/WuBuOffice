/* wubupad_bridge.c -- cross-repo bridge: reuse WuBuPad's from-scratch editor
 * core (Doc + UI headless + DONE search engine) inside WuBuOffice.
 *
 * WuBuOffice owns the document model / office formats; WuBuPad owns the fast
 * text-editing engine. This module lets an office app open a plain-text view
 * of content through WuBuPad's core without duplicating any editing logic.
 * Clean C11. See PLAN_BLITZ Phase E. */
#include "wubupad_bridge.h"

/* WuBuPad headers live in the sibling repo (../WuBuPad/src). */
#include "doc.h"
#include "ui/ui.h"
#include "ui/ui_headless.h"
#include "ui/ui_find.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* read an entire file into a malloc'd buffer (caller frees). */
static char *slurp(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = 0;
    fclose(f);
    if (out_len) *out_len = (size_t)r;
    return buf;
}

int wubupad_load(const char *path, char **out, size_t *out_len) {
    size_t raw = 0;
    char *text = slurp(path, &raw);
    if (!text) return -1;
    Doc *d = doc_create(text);
    free(text);
    char *t = doc_text(d);
    size_t len = strlen(t);
    char *cpy = malloc(len + 1);
    memcpy(cpy, t, len + 1);
    free(t);
    doc_free(d);
    *out = cpy;
    *out_len = len;
    return 0;
}

int wubupad_stats(const char *path, size_t *lines, size_t *chars) {
    size_t raw = 0;
    char *text = slurp(path, &raw);
    if (!text) return -1;
    Doc *d = doc_create(text);
    free(text);
    if (lines) *lines = doc_lines(d);
    if (chars) *chars = doc_length(d);
    doc_free(d);
    return 0;
}

int wubupad_find_replace(const char *path,
                         const char *find, int regex, int icase,
                         const char *repl,
                         char **out, size_t *out_len) {
    size_t raw = 0;
    char *text = slurp(path, &raw);
    if (!text) return -1;
    Doc *d = doc_create(text);
    free(text);

    UI *ui = ui_create(d, ui_headless_backend(), 80, 24);
    long n = ui_find(ui, find, regex, icase);
    int rc = 0;
    if (n > 0) {
        long replaced = ui_find_replace_all_in_doc(ui, repl);
        (void)replaced;
        char *t = doc_text(d);
        size_t len = strlen(t);
        char *cpy = malloc(len + 1);
        memcpy(cpy, t, len + 1);
        free(t);
        *out = cpy;
        *out_len = len;
    } else {
        /* nothing matched: return original */
        char *t = doc_text(d);
        size_t len = strlen(t);
        char *cpy = malloc(len + 1);
        memcpy(cpy, t, len + 1);
        free(t);
        *out = cpy;
        *out_len = len;
        rc = 1; /* 1 = no matches (still produced original text) */
    }
    ui_free(ui);
    doc_free(d);
    return rc;
}
