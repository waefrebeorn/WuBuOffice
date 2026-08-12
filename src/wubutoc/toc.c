/* toc.c -- DOC-54 table-of-contents generator. See toc.h. */
#include "toc.h"
#include "model.h"
#include "ublayout.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    int   level;
    int   page;    /* 1-based; 0 unknown */
    void *node;
} TocEntry;

struct Toc {
    TocEntry *e;
    int n, cap;
};

static int toc_push(Toc *t, char *title, int level, int page, void *node){
    if (t->n >= t->cap){
        int nc = t->cap? t->cap*2 : 16;
        TocEntry *ne = realloc(t->e, nc*sizeof *ne);
        if (!ne) return -1;
        t->e = ne; t->cap = nc;
    }
    t->e[t->n].title = title; t->e[t->n].level = level;
    t->e[t->n].page = page;   t->e[t->n].node = node;
    t->n++;
    return 0;
}

/* concatenated text of a paragraph's RUN children (malloc'd) */
static char *para_text(wubumodel_node *para){
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = 0;
    for (wubumodel_node *r = wubumodel_node_first_child(para);
         r; r = wubumodel_node_next_sibling(r)){
        if (wubumodel_node_kind(r) != WUBUMODEL_RUN) continue;
        const char *tx = wubumodel_run_text(r);
        if (!tx) continue;
        size_t add = strlen(tx);
        if (len + add + 1 > cap){
            while (len + add + 1 > cap) cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb){ free(buf); return NULL; }
            buf = nb;
        }
        memcpy(buf+len, tx, add); len += add; buf[len] = 0;
    }
    return buf;
}

/* 1-based page of a paragraph: find any laid-out run whose user pointer is
 * one of this paragraph's RUN children. */
static int para_page(wubulayout_doc *L, wubumodel_node *para){
    if (!L) return 0;
    for (int pg = 0; pg < wubulayout_page_count(L); pg++){
        int nr = wubulayout_run_count(L, pg);
        for (int i = 0; i < nr; i++){
            const wubulayout_run *r = wubulayout_run_at(L, pg, i);
            if (!r || !r->user) continue;
            for (wubumodel_node *c = wubumodel_node_first_child(para);
                 c; c = wubumodel_node_next_sibling(c))
                if ((void*)c == r->user) return pg + 1;
        }
    }
    return 0;
}

/* heading level of a paragraph via style prop "heading" = "1".."6"; 0 = not
 * a heading. Also recognizes common heading style NAMES ("Heading1", "Heading
 * 1", "Title") so markdown/sample docs (which set the style name) produce a
 * navigator/TOC outline. */
static int ci_eq(const char *a, const char *b, size_t n){
    for (size_t i=0;i<n;i++){
        if (!a[i]) return 0;
        unsigned char x=a[i], y=b[i];
        if (x>='A'&&x<='Z') x=(unsigned char)(x-'A'+'a');
        if (y>='A'&&y<='Z') y=(unsigned char)(y-'A'+'a');
        if (x!=y) return 0;
    }
    return 1;
}
static int heading_level(wubumodel_node *para){
    wubumodel_style *st = wubumodel_node_style(para);
    if (!st) return 0;
    const char *h = wubumodel_style_get_prop(st, "heading");
    if (h && *h){
        int lv = atoi(h);
        if (lv >= 1 && lv <= 6) return lv;
    }
    const char *nm = wubumodel_style_get_prop(st, "name");
    if (nm){
        if (ci_eq(nm, "Title", 5) && !nm[5]) return 1;
        const char *p = nm;
        while (*p && (p - nm) < 20){
            if (ci_eq(p, "Heading", 7)){
                const char *q = p + 7;
                while (*q == ' ') q++;
                if (*q >= '1' && *q <= '6') return *q - '0';
            }
            p++;
        }
    }
    return 0;
}

static void walk(Toc *t, wubulayout_doc *L, wubumodel_node *n){
    for (; n; n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) == WUBUMODEL_PARAGRAPH){
            int lv = heading_level(n);
            if (lv){
                char *title = para_text(n);
                if (title && *title)
                    toc_push(t, title, lv, para_page(L, n), n);
                else
                    free(title);
            }
        } else {
            wubumodel_node *c = wubumodel_node_first_child(n);
            if (c) walk(t, L, c);
        }
    }
}

Toc *toc_build(void *model_doc, void *root, void *layout){
    Toc *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    wubumodel_node *start = root ? (wubumodel_node*)root
        : wubumodel_doc_root((wubumodel_doc*)model_doc);
    if (start) walk(t, (wubulayout_doc*)layout, start);
    return t;
}

void toc_free(Toc *t){
    if (!t) return;
    for (int i = 0; i < t->n; i++) free(t->e[i].title);
    free(t->e);
    free(t);
}

int toc_count(const Toc *t){ return t ? t->n : 0; }
const char *toc_title(const Toc *t, int i){
    return (t && i >= 0 && i < t->n) ? t->e[i].title : NULL;
}
int toc_level(const Toc *t, int i){
    return (t && i >= 0 && i < t->n) ? t->e[i].level : 0;
}
int toc_page(const Toc *t, int i){
    return (t && i >= 0 && i < t->n) ? t->e[i].page : 0;
}
void *toc_node(const Toc *t, int i){
    return (t && i >= 0 && i < t->n) ? t->e[i].node : NULL;
}

char *toc_text(const Toc *t){
    if (!t) return NULL;
    size_t cap = 256, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = 0;
    for (int i = 0; i < t->n; i++){
        char line[512];
        int ind = (t->e[i].level - 1) * 2; if (ind < 0) ind = 0; if (ind > 10) ind = 10;
        if (t->e[i].page > 0)
            snprintf(line, sizeof line, "%*s%s .... p%d\n", ind, "", t->e[i].title, t->e[i].page);
        else
            snprintf(line, sizeof line, "%*s%s\n", ind, "", t->e[i].title);
        size_t add = strlen(line);
        if (len + add + 1 > cap){
            while (len + add + 1 > cap) cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb){ free(buf); return NULL; }
            buf = nb;
        }
        memcpy(buf+len, line, add); len += add; buf[len] = 0;
    }
    return buf;
}
