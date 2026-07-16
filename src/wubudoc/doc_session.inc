#include "wubudoc_internal.h"

DocSession *doc_session_create(void) {
    DocSession *s = malloc(sizeof *s);
    if (!s) return NULL;
    memset(s, 0, sizeof *s);
    return s;
}

static void handle_free(DocHandle *d) {
    if (!d) return;
    free(d->kind); free(d->source); free(d->text);
    j_free((JVal *)d->model);
    for (size_t i = 0; i < d->media_n; i++) { free(d->media_name[i]); free(d->media_data[i]); }
    free(d->media_name); free(d->media_data); free(d->media_len);
    memset(d, 0, sizeof *d);
}

void doc_session_free(DocSession *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) handle_free(&s->h[i]);
    free(s->h);
    if (s->ocr_bank) ocr_fontbank_free(s->ocr_bank);
    free(s);
}

size_t doc_count(const DocSession *s) { return s ? s->n : 0; }

static long session_add(DocSession *s, const char *kind, const char *source) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 4;
        s->h = realloc(s->h, s->cap * sizeof *s->h);
        if (!s->h) return -1;
    }
    DocHandle *d = &s->h[s->n];
    memset(d, 0, sizeof *d);
    d->kind = strdup(kind ? kind : "?");
    d->source = source ? strdup(source) : strdup("");
    long id = (long)(s->n);
    s->n++;
    return id;
}
static DocHandle *get(const DocSession *s, long id) {
    if (id < 0 || (size_t)id >= s->n) return NULL;
    return &s->h[id];
}
