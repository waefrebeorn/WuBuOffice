#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "package.h"
#include "../wubuzip/zip.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct default_t { char *ext; char *ct; };
struct override_t { char *part; char *ct; };
struct rel_t { char *source; char *target; char *rtype; char *id; };

struct wubuoxml_package {
    wubuzip_writer *z;
    struct default_t *dt; size_t ndt, capdt;
    struct override_t *ov; size_t nov, capov;
    struct rel_t *rl; size_t nrl, caprl;
    unsigned rid;
};

wubuoxml_package *wubuoxml_create(FILE *out) {
    wubuoxml_package *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->z = wubuzip_create(out);
    if (!p->z) { free(p); return NULL; }
    p->rid = 0;
    return p;
}

static int push_default(wubuoxml_package *p, char *ext, char *ct) {
    if (p->ndt == p->capdt) {
        p->capdt = p->capdt ? p->capdt * 2 : 8;
        p->dt = realloc(p->dt, p->capdt * sizeof(*p->dt));
        if (!p->dt) return -1;
    }
    p->dt[p->ndt].ext = ext; p->dt[p->ndt].ct = ct; p->ndt++;
    return 0;
}
static int push_override(wubuoxml_package *p, char *part, char *ct) {
    if (p->nov == p->capov) {
        p->capov = p->capov ? p->capov * 2 : 8;
        p->ov = realloc(p->ov, p->capov * sizeof(*p->ov));
        if (!p->ov) return -1;
    }
    p->ov[p->nov].part = part; p->ov[p->nov].ct = ct; p->nov++;
    return 0;
}

int wubuoxml_add_default_type(wubuoxml_package *p, const char *ext, const char *ct) {
    return push_default(p, strdup(ext), strdup(ct));
}
int wubuoxml_add_override(wubuoxml_package *p, const char *part, const char *ct) {
    return push_override(p, strdup(part), strdup(ct));
}

int wubuoxml_add_relationship(wubuoxml_package *p, const char *source, const char *target, const char *rtype) {
    if (p->nrl == p->caprl) {
        p->caprl = p->caprl ? p->caprl * 2 : 16;
        p->rl = realloc(p->rl, p->caprl * sizeof(*p->rl));
        if (!p->rl) return -1;
    }
    char id[16];
    snprintf(id, sizeof id, "rId%u", ++p->rid);
    struct rel_t *r = &p->rl[p->nrl++];
    r->source = strdup(source ? source : "");
    r->target = strdup(target);
    r->rtype = strdup(rtype);
    r->id = strdup(id);
    return 0;
}

int wubuoxml_add_part(wubuoxml_package *p, const char *path, const void *data, size_t size) {
    return wubuzip_add(p->z, path, data, (uint32_t)size);
}

/* Build the rels part path for a given source. source == "" -> "_rels/.rels". */
static char *rels_path_for(const char *source) {
    if (!source || source[0] == '\0') return strdup("_rels/.rels");
    const char *slash = strrchr(source, '/');
    size_t dirlen = slash ? (size_t)(slash - source) + 1 : 0;
    size_t baselen = slash ? strlen(slash + 1) : strlen(source);
    size_t need = dirlen + 6 /* "_rels/" */ + baselen + 5 /* .rels */ + 1;
    char *buf = malloc(need);
    char *q = buf;
    if (dirlen) { memcpy(q, source, dirlen); q += dirlen; }
    memcpy(q, "_rels/", 6); q += 6;
    memcpy(q, slash ? slash + 1 : source, baselen); q += baselen;
    memcpy(q, ".rels", 5); q += 5;
    *q = '\0';
    return buf;
}

int wubuoxml_finalize(wubuoxml_package *p) {
    /* 1) [Content_Types].xml */
    {
        char *buf = NULL; size_t len = 0; FILE *m = open_memstream(&buf, &len);
        fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n", m);
        fputs("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n", m);
        for (size_t i = 0; i < p->ndt; i++)
            fprintf(m, "  <Default Extension=\"%s\" ContentType=\"%s\"/>\n", p->dt[i].ext, p->dt[i].ct);
        for (size_t i = 0; i < p->nov; i++)
            fprintf(m, "  <Override PartName=\"%s\" ContentType=\"%s\"/>\n", p->ov[i].part, p->ov[i].ct);
        fputs("</Types>\n", m);
        fflush(m); fclose(m);
        wubuzip_add(p->z, "[Content_Types].xml", buf, (uint32_t)len);
        free(buf);
    }

    /* 2) one .rels part per distinct source */
    for (size_t s = 0; s < p->nrl; s++) {
        const char *src = p->rl[s].source;
        int handled = 0;
        /* skip if an earlier relationship with same source already produced this rels part */
        for (size_t j = 0; j < s; j++)
            if (strcmp(p->rl[j].source, src) == 0) { handled = 1; break; }
        if (handled) continue;

        char *rpath = rels_path_for(src);
        char *buf = NULL; size_t len = 0; FILE *m = open_memstream(&buf, &len);
        fputs("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n", m);
        fputs("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n", m);
        for (size_t i = 0; i < p->nrl; i++) {
            if (strcmp(p->rl[i].source, src) != 0) continue;
            fprintf(m, "  <Relationship Id=\"%s\" Type=\"%s\" Target=\"%s\"/>\n",
                    p->rl[i].id, p->rl[i].rtype, p->rl[i].target);
        }
        fputs("</Relationships>\n", m);
        fflush(m); fclose(m);
        wubuzip_add(p->z, rpath, buf, (uint32_t)len);
        free(buf); free(rpath);
    }

    int rc = wubuzip_finalize(p->z);

    for (size_t i = 0; i < p->ndt; i++) { free(p->dt[i].ext); free(p->dt[i].ct); }
    for (size_t i = 0; i < p->nov; i++) { free(p->ov[i].part); free(p->ov[i].ct); }
    for (size_t i = 0; i < p->nrl; i++) { free(p->rl[i].source); free(p->rl[i].target); free(p->rl[i].rtype); free(p->rl[i].id); }
    free(p->dt); free(p->ov); free(p->rl);
    free(p);
    return rc;
}
