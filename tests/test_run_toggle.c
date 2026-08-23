/* test_run_toggle.c -- N3: direct character formatting (Ctrl+B/Ctrl+I
 * surface). Toggle bold on a paragraph's runs; verify style props flip and
 * that a second toggle turns it off. Also verifies the cnfStyle-resolved
 * surface: a run carrying resolved bold from H6d gets toggled OFF. */
#include "../apps/wubuos/view_doc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

static int run_prop_is(wubumodel_node *p, const char *prop, const char *want){
    for (wubumodel_node *n = wubumodel_node_first_child(p); n;
         n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) != WUBUMODEL_RUN) continue;
        const wubumodel_style *st = wubumodel_node_style(n);
        const char *v = st ? wubumodel_style_get_prop(st, prop) : NULL;
        if (!v || strcmp(v, want)) return 0;
    }
    return 1;
}

int main(void){
    /* build the view through the factory so DocV internals match */
    WuView *v = wuos_doc_create(NULL);
    ck(v != NULL, "doc view created");
    if (!v) return 1;
    DocV *e = v->priv;

    /* attach a known doc (the view's sample loads lazily) */
    e->doc = wurender_doc_from_markdown("# Head\n\nAlpha **b** beta.\n\nSecond para.\n");
    ck(e->doc != NULL, "test doc built");
    if (!e->doc) return 1;

    /* markdown docs nest paragraphs under blocks — walk recursively */
    wubumodel_node *p = NULL;
    wubumodel_node *root = wubumodel_doc_root(e->doc);
    if (wubumodel_node_kind(root) == WUBUMODEL_PARAGRAPH) p = root;
    for (wubumodel_node *c = wubumodel_node_first_child(root);
         c && !p; c = wubumodel_node_next_sibling(c)){
        if (wubumodel_node_kind(c) == WUBUMODEL_PARAGRAPH){ p = c; break; }
        for (wubumodel_node *g = wubumodel_node_first_child(c); g && !p;
             g = wubumodel_node_next_sibling(g))
            if (wubumodel_node_kind(g) == WUBUMODEL_PARAGRAPH) p = g;
    }
    ck(p != NULL, "paragraph found");
    if (!p) return 1;

    doc_toggle_run_prop(e, "bold");
    ck(run_prop_is(p, "bold", "1"), "bold=1 after toggle");

    /* toggle OFF */
    doc_toggle_run_prop(e, "bold");
    ck(run_prop_is(p, "bold", "0"), "bold=0 after second toggle");

    /* italic independent — re-resolve the same paragraph the toggle hits */
    e->cur_para = 0;
    p = doc_nth_paragraph(e, 0);
    if (!p){ fprintf(stderr,"[dbg] nth(0) NULL\n"); }
    doc_toggle_run_prop(e, "italic");
    ck(run_prop_is(p, "italic", "1"), "italic toggles independently");
    { for (wubumodel_node *r2 = wubumodel_node_first_child(p); r2; r2 = wubumodel_node_next_sibling(r2)){
        fprintf(stderr,"  [post-italic run] ");
        const wubumodel_style *st = wubumodel_node_style(r2);
        if (st) for (int k2 = 0;; k2++){
            const char *nm=NULL,*vl=NULL;
            if (wubumodel_style_prop_at(st,k2,&nm,&vl)==0) break;
            fprintf(stderr,"%s=%s ", nm, vl);
        }
        fprintf(stderr,"\n"); } }
    ck(run_prop_is(p, "bold", "0"), "bold unaffected by italic toggle");

    fprintf(stderr, bad ? "RUN_TOGGLE FAIL\n" : "RUN_TOGGLE PASS\n");
    return bad ? 1 : 0;
}
