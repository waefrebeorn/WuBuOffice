/* nesttab.c -- nested tables builder/validator. See nesttab.h. */
#include "nesttab.h"
#include "model.h"

#include <stddef.h>

void *nesttab_build(void *docv, void *parentv, int rows, int cols){
    wubumodel_doc *doc = docv;
    wubumodel_node *parent = parentv;
    if (!doc || rows<=0 || cols<=0) return NULL;
    wubumodel_node *t = wubumodel_node_create(doc, WUBUMODEL_TABLE);
    if (!t) return NULL;
    if (parent && wubumodel_node_append(doc, parent, t) != 0) return NULL;
    for (int i=0;i<rows*cols;i++){
        wubumodel_node *c = wubumodel_node_create(doc, WUBUMODEL_CELL);
        if (!c || wubumodel_node_append(doc, t, c)!=0) return NULL;
        wubumodel_node *r = wubumodel_node_create(doc, WUBUMODEL_RUN);
        if (!r || wubumodel_node_append(doc, c, r)!=0) return NULL;
        wubumodel_run_set_text(r, "");
    }
    return t;
}

void *nesttab_nest(void *docv, void *cellv, int rows, int cols){
    wubumodel_node *cell = cellv;
    if (!cell || wubumodel_node_kind(cell) != WUBUMODEL_CELL) return NULL;
    return nesttab_build(docv, cell, rows, cols);
}

static int depth_walk(const wubumodel_node *n){
    if (!n) return 0;
    int best = 0;
    for (const wubumodel_node *c = wubumodel_node_first_child(n); c;
         c = wubumodel_node_next_sibling(c)){
        int d = depth_walk(c);
        if (d > best) best = d;
    }
    if (wubumodel_node_kind(n) == WUBUMODEL_TABLE) return best + 1;
    return best;
}
int nesttab_depth(const void *node){ return depth_walk((const wubumodel_node*)node); }

int nesttab_validate(const void *tablev){
    const wubumodel_node *t = tablev;
    if (!t || wubumodel_node_kind(t) != WUBUMODEL_TABLE) return 0;
    for (const wubumodel_node *c = wubumodel_node_first_child(t); c;
         c = wubumodel_node_next_sibling(c)){
        if (wubumodel_node_kind(c) != WUBUMODEL_CELL) return 0;
        /* recurse into nested tables inside this cell */
        for (const wubumodel_node *g = wubumodel_node_first_child(c); g;
             g = wubumodel_node_next_sibling(g))
            if (wubumodel_node_kind(g) == WUBUMODEL_TABLE &&
                !nesttab_validate(g)) return 0;
    }
    return 1;
}

void *nesttab_cell(void *tablev, int row, int col, int cols){
    wubumodel_node *t = tablev;
    if (!t || row<0 || col<0 || cols<=0 || col>=cols) return NULL;
    int want = row*cols + col, i = 0;
    for (wubumodel_node *c = wubumodel_node_first_child(t); c;
         c = wubumodel_node_next_sibling(c), i++)
        if (i == want) return c;
    return NULL;
}
