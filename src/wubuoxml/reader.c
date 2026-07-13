#include "reader.h"
#include "docx_text.h"
#include "rels_path.h"
#include "../wubuzip/reader.h"
#include "../wubuzip/io_le.h"

#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void parse_rels(const char *rels_xml, size_t len, wubuoxml_part *part) {
    const char *s = rels_xml;
    size_t cap = 8;
    part->nrel = 0;
    part->rel_targets = malloc(cap * sizeof(char *));
    char **ids = malloc(cap * sizeof(char *));
    if (!part->rel_targets || !ids) {
        free(part->rel_targets);
        free(ids);
        part->rel_targets = NULL;
        return;
    }
    for (size_t i = 0; i + 12 <= len; ) {
        if (strncmp(s + i, "<Relationship", 12) != 0) { i++; continue; }
        char id[64] = "", tgt[1024] = "";
        const char *p = s + i;
        const char *q;
        if ((q = strstr(p, "Id=\"")))     { q += 4; size_t j = 0; while (*q && *q != '"' && j < 63) id[j++] = *q++; id[j] = '\0'; }
        if ((q = strstr(p, "Target=\""))) { q += 8; size_t j = 0; while (*q && *q != '"' && j < 1023) tgt[j++] = *q++; tgt[j] = '\0'; }
        if (tgt[0]) {
            if (part->nrel == cap) {
                cap *= 2;
                part->rel_targets = realloc(part->rel_targets, cap * sizeof(char *));
                ids = realloc(ids, cap * sizeof(char *));
            }
            part->rel_targets[part->nrel] = xstrdup(tgt);
            ids[part->nrel] = xstrdup(id);
            part->nrel++;
        }
        const char *end = strstr(p, "/>");
        if (!end) end = strstr(p, ">");
        if (!end) break;
        i = (size_t)(end - s) + 1;
    }
    for (size_t k = 0; k < part->nrel; k++) free(ids[k]);
    free(ids);
}

int wubuoxml_read(const uint8_t *data, size_t len, wubuoxml_package *p) {
    memset(p, 0, sizeof *p);
    wubuzip_archive z;
    if (wubuzip_open(data, len, &z) != 0) return -1;
    p->n = wubuzip_count(&z);
    p->parts = calloc(p->n ? p->n : 1, sizeof *p->parts);
    if (!p->parts) { wubuzip_close(&z); return -1; }

    for (size_t i = 0; i < p->n; i++) {
        const char *nm = wubuzip_name(&z, i);
        uint8_t *bytes = NULL;
        size_t blen = 0;
        wubuzip_extract(&z, i, &bytes, &blen);
        wubuoxml_part *pt = &p->parts[i];
        pt->name = xstrdup(nm);
        pt->bytes = bytes;
        pt->len = blen;
        pt->content_type = NULL;
    }

    /* parse .rels rooted at each part */
    for (size_t i = 0; i < p->n; i++) {
        const char *nm = p->parts[i].name;
        char *rpath = wubuoxml_rels_path_for(nm);
        size_t ri = wubuzip_find(&z, rpath);
        if (ri != (size_t)-1) {
            uint8_t *rb = NULL;
            size_t rl = 0;
            wubuzip_extract(&z, ri, &rb, &rl);
            if (rb) {
                parse_rels((const char *)rb, rl, &p->parts[i]);
                free(rb);
            }
        }
        free(rpath);
    }
    wubuzip_close(&z);
    return 0;
}

size_t wubuoxml_part_count(const wubuoxml_package *p) { return p->n; }
const wubuoxml_part *wubuoxml_part_at(const wubuoxml_package *p, size_t i) { return &p->parts[i]; }

const wubuoxml_part *wubuoxml_part_find(const wubuoxml_package *p, const char *path) {
    for (size_t i = 0; i < p->n; i++) {
        const char *nm = p->parts[i].name;
        if (strcmp(nm, path) == 0) return &p->parts[i];
        if (path[0] == '/' && strcmp(nm, path + 1) == 0) return &p->parts[i];
    }
    return NULL;
}

void wubuoxml_free(wubuoxml_package *p) {
    for (size_t i = 0; i < p->n; i++) {
        free(p->parts[i].name);
        free(p->parts[i].bytes);
        free(p->parts[i].content_type);
        for (size_t k = 0; k < p->parts[i].nrel; k++) free(p->parts[i].rel_targets[k]);
        free(p->parts[i].rel_targets);
    }
    free(p->parts);
    p->parts = NULL;
    p->n = 0;
}
