#include "wubudoc_internal.h"

/* Flatten a wubudoc "document" / "ocr_page" model into readable text by
 * concatenating each block's "text" field (one paragraph per block). Returns a
 * malloc'd string (caller frees) or NULL if the model has no block text. */
static char *docmodel_flat_text(const char *json) {
    JVal *m = j_parse(json, NULL);
    if (!m) return NULL;
    const JVal *blocks = j_obj_get(m, "blocks");
    char *out = NULL;
    if (blocks && j_type(blocks) == J_ARR && j_len(blocks) > 0) {
        size_t cap = 1024, n = 0;
        out = calloc(cap, 1);
        for (size_t i = 0; i < j_len(blocks); i++) {
            const JVal *b = j_arr_at(blocks, i);
            const JVal *t = j_obj_get(b, "text");
            if (t && j_type(t) == J_STR) {
                const char *s = j_as_str(t);
                size_t al = strlen(s);
                while (n + al + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
                memcpy(out + n, s, al); n += al;
                out[n++] = '\n'; out[n] = '\0';
            }
        }
        if (n == 0) { free(out); out = NULL; }
    }
    j_free(m);
    return out;
}

/* Extract a displayable text projection for dropped/pasted bytes. Returns a
 * malloc'd string (caller frees), or NULL. For text kinds returns the raw
 * text; for office/container kinds it asks wubuconv for a Markdown projection;
 * for image/PDF kinds it returns the OCR'd / extracted text. */
char *doc_drop_text(DocSession *s, const uint8_t *data, size_t len) {
    if (!s || !data || len == 0) return NULL;
    long id = doc_open_auto(s, data, len);
    if (id < 0) return NULL;
    const char *t = doc_text(s, id);
    if (t && *t) return strdup(t);
    /* No raw text field: try to flatten a block-based JSON model (image OCR,
     * docx/odt via wubuconv, etc.) into paragraphs. */
    char *js = doc_json(s, id);
    if (js) {
        char *ft = docmodel_flat_text(js);
        free(js);
        if (ft) return ft;
    }
    /* Fallback: project container kinds to Markdown via the semantic engine. */
    const char *kind = doc_kind(s, id);
    uint8_t *mj = NULL; size_t mjlen = 0;
    if (kind && wubuconv_convert_mem(data, len, kind, "md", &mj, &mjlen) == 0 && mjlen > 0) {
        char *out = malloc(mjlen + 1);
        memcpy(out, mj, mjlen); out[mjlen] = '\0';
        free(mj);
        return out;
    }
    free(mj);
    return NULL;
}
/* ---------- accessors ---------- */
const char *doc_kind(const DocSession *s, long id) {
    DocHandle *d = get(s, id); return d ? d->kind : NULL;
}
const char *doc_source(const DocSession *s, long id) {
    DocHandle *d = get(s, id); return d ? d->source : NULL;
}
char *doc_json(const DocSession *s, long id) {
    DocHandle *d = get(s, id);
    if (!d) return NULL;
    return j_emit((JVal *)d->model);
}
const char *doc_text(const DocSession *s, long id) {
    DocHandle *d = get(s, id); return d ? d->text : NULL;
}
void doc_set_model(DocSession *s, long id, void *model) {
    DocHandle *d = get(s, id);
    if (!d) { j_free((JVal*)model); return; }
    j_free((JVal*)d->model);
    d->model = model;
}

int doc_add_media(DocSession *s, long id, const char *name, const uint8_t *data, size_t len) {
    DocHandle *d = get(s, id);
    if (!d || !name || !data) return -1;
    if (d->media_n == d->media_cap) {
        d->media_cap = d->media_cap ? d->media_cap * 2 : 4;
        d->media_name = realloc(d->media_name, d->media_cap * sizeof *d->media_name);
        d->media_data = realloc(d->media_data, d->media_cap * sizeof *d->media_data);
        d->media_len  = realloc(d->media_len,  d->media_cap * sizeof *d->media_len);
    }
    d->media_name[d->media_n] = strdup(name);
    d->media_data[d->media_n] = malloc(len);
    memcpy(d->media_data[d->media_n], data, len);
    d->media_len[d->media_n] = len;
    d->media_n++;
    return 0;
}
