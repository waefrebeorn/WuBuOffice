/* a11ytree.c -- UI -> accessibility-tree serializer. See a11ytree.h. */
#include "a11ytree.h"
#include "model.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *kind_name(wubumodel_kind k){
    switch (k){
        case WUBUMODEL_SECTION: return "SECTION";
        case WUBUMODEL_PARAGRAPH: return "PARAGRAPH";
        case WUBUMODEL_RUN: return "RUN";
        case WUBUMODEL_TABLE: return "TABLE";
        case WUBUMODEL_IMAGE: return "IMAGE";
        case WUBUMODEL_LINK: return "LINK";
        case WUBUMODEL_HEADER: return "HEADER";
        case WUBUMODEL_FOOTER: return "FOOTER";
        case WUBUMODEL_COMMENT: return "COMMENT";
        default: return NULL;
    }
}

static void walk(const wubumodel_node *n, int depth, char **out, size_t *cap, size_t *len){
    for (; n; n = wubumodel_node_next_sibling(n)){
        wubumodel_kind k = wubumodel_node_kind(n);
        const char *rn = kind_name(k);
        if (k == WUBUMODEL_SECTION){
            /* a section is both a heading and a container */
            size_t need = (size_t)snprintf(NULL,0,"HEADING:%d\n", depth);
            while (*len+need+1 > *cap){ *cap*=2; char *no=realloc(*out,*cap); if(!no){free(*out);*out=NULL;return;} *out=no; }
            *len += (size_t)sprintf(*out+*len, "HEADING:%d\n", depth);
            walk(wubumodel_node_first_child(n), depth+1, out, cap, len);
        } else if (rn){
            const char *note = wubumodel_node_note(n);   /* IMAGE alt text, etc. */
            const char *link = (k==WUBUMODEL_LINK)? wubumodel_node_link(n) : NULL;
            size_t need = (size_t)snprintf(NULL,0,"%s: %s\n", rn, note? note : (link? link : ""));
            while (*len+need+1 > *cap){ *cap*=2; char *no=realloc(*out,*cap); if(!no){free(*out);*out=NULL;return;} *out=no; }
            *len += (size_t)sprintf(*out+*len, "%s: %s\n", rn, note? note : (link? link : ""));
            walk(wubumodel_node_first_child(n), depth, out, cap, len);
        } else {
            walk(wubumodel_node_first_child(n), depth, out, cap, len);
        }
    }
}

char *a11ytree_build(const void *doc){
    if (!doc) return NULL;
    size_t cap=256, len=0; char *out = malloc(cap);
    if (!out) return NULL;
    out[0]=0;
    const wubumodel_doc *d = (const wubumodel_doc*)doc;
    walk(wubumodel_doc_root(d), 1, &out, &cap, &len);
    if (!out) return NULL;
    out[len]=0;
    return out;
}

int a11ytree_count(const void *doc){
    char *t = a11ytree_build(doc);
    if (!t) return 0;
    int n=0; for (char *p=t; *p; p++) if (*p=='\n') n++;
    free(t);
    return n;
}
