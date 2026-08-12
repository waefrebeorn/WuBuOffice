#include "wubumailmerge.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char **f; char **v; size_t n; } record;

struct wubumailmerge {
    record *recs;
    size_t n, cap;
};

wubumailmerge *wubumailmerge_create(void) { return (wubumailmerge *)calloc(1, sizeof(wubumailmerge)); }

void wubumailmerge_destroy(wubumailmerge *m) {
    if (!m) return;
    for (size_t i = 0; i < m->n; i++) {
        for (size_t j = 0; j < m->recs[i].n; j++) { free(m->recs[i].f[j]); free(m->recs[i].v[j]); }
        free(m->recs[i].f); free(m->recs[i].v);
    }
    free(m->recs);
    free(m);
}

int wubumailmerge_add_record(wubumailmerge *m, const wubumailmerge_field *fields) {
    if (!m || !fields) return -1;
    if (m->n == m->cap) {
        size_t nc = m->cap ? m->cap * 2 : 4;
        record *nr = (record *)realloc(m->recs, nc * sizeof(record));
        if (!nr) return -1;
        m->recs = nr; m->cap = nc;
    }
    size_t n = 0; while (fields[n].field) n++;
    record *r = &m->recs[m->n];
    r->f = n ? (char **)calloc(n, sizeof(char *)) : NULL;
    r->v = n ? (char **)calloc(n, sizeof(char *)) : NULL;
    if (n && (!r->f || !r->v)) { free(r->f); free(r->v); return -1; }
    r->n = n;
    for (size_t j = 0; j < n; j++) {
        r->f[j] = strdup(fields[j].field);
        r->v[j] = strdup(fields[j].value ? fields[j].value : "");
        if (!r->f[j] || !r->v[j]) { return -1; }
    }
    m->n++;
    return 0;
}

size_t wubumailmerge_record_count(const wubumailmerge *m) { return m ? m->n : 0; }

static const char *field_val(const record *r, const char *name) {
    for (size_t j = 0; j < r->n; j++) if (strcmp(r->f[j], name) == 0) return r->v[j];
    return NULL;
}

char *wubumailmerge_merge(const wubumailmerge *m, size_t i, const char *templ) {
    if (!m || i >= m->n || !templ) return NULL;
    const record *r = &m->recs[i];
    size_t tlen = strlen(templ);
    char *out = (char *)malloc(tlen + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t p = 0; p < tlen; ) {
        /* placeholder: ${name} or {name} */
        int braced = (templ[p] == '$' && p + 1 < tlen && templ[p+1] == '{');
        int open_at = braced ? (int)(p + 1) : (templ[p] == '{' ? (int)p : -1);
        if (open_at >= 0) {
            int close = -1;
            for (size_t q = (size_t)open_at + 1; q < tlen; q++) if (templ[q] == '}') { close = (int)q; break; }
            if (close > open_at) {
                size_t name_len = (size_t)close - (size_t)open_at - 1;
                char name[128]; size_t nl = name_len < 127 ? name_len : 127;
                memcpy(name, templ + open_at + 1, nl); name[nl] = 0;
                const char *v = field_val(r, name);
                if (v) {
                    size_t vl = strlen(v);
                    char *no = (char *)realloc(out, o + vl + tlen + 1);
                    if (!no) { free(out); return NULL; }
                    out = no;
                    memcpy(out + o, v, vl); o += vl;
                    p = (size_t)close + 1;
                    continue;
                }
            }
        }
        /* grow for literal char */
        if (o + 2 >= tlen + 1) {
            char *no = (char *)realloc(out, o + tlen + 2);
            if (!no) { free(out); return NULL; }
            out = no;
        }
        out[o++] = templ[p++];
    }
    out[o] = 0;
    return out;
}
