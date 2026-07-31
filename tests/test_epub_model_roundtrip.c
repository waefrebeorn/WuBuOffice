/* test_epub_model_roundtrip.c -- EPUB write -> unified-model load round trip.
 *
 * Proves the loader half (wubumodel_load_epub, EXP-82) is the inverse of
 * the writer half (epub_write, src/wubuepub/epub.c): build a model with a
 * heading, paragraphs, a table and a link, write it to .epub, read it back
 * into a fresh model, and assert the structure survived.
 *
 * The writer emits un-namespaced XHTML with this shape (see epub.c emit_block):
 *   <hN>..</hN>   -> SECTION + PARAGRAPH(HeadingN)
 *   <p>..</p>      -> PARAGRAPH
 *   <table><tr><td> -> TABLE -> CELL(row) -> CELL(cell) -> RUN
 *   <a href>text</a> -> LINK(target) + child RUN(text) */
#include "epub.h"
#include <unistd.h>
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

static wubumodel_doc *make_doc(void){
    wubumodel_doc *d = wubumodel_doc_create();

    /* Section 1: title (heading) + a paragraph */
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *h   = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node_apply_named_style(h, "Heading1");
    wubumodel_node *hr  = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(hr, "Chapter One");
    wubumodel_node_append(d, h, hr);
    wubumodel_node_append(d, sec, h);

    wubumodel_node *p = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *pr = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(pr, "The quick brown fox.");
    wubumodel_node_append(d, p, pr);
    wubumodel_node_append(d, sec, p);

    /* a link inside a paragraph */
    wubumodel_node *pl = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *link = wubumodel_node_create(d, WUBUMODEL_LINK);
    wubumodel_node_set_link(link, "https://example.com");
    wubumodel_node *lr = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(lr, "visit example");
    wubumodel_node_append(d, link, lr);
    wubumodel_node_append(d, pl, link);
    wubumodel_node_append(d, sec, pl);

    /* a table: 1 row x 2 cols */
    wubumodel_node *tbl = wubumodel_node_create(d, WUBUMODEL_TABLE);
    wubumodel_node *row = wubumodel_node_create(d, WUBUMODEL_CELL);
    wubumodel_node *c1  = wubumodel_node_create(d, WUBUMODEL_CELL);
    wubumodel_node *c1r = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(c1r, "A1");
    wubumodel_node_append(d, c1, c1r);
    wubumodel_node *c2  = wubumodel_node_create(d, WUBUMODEL_CELL);
    wubumodel_node *c2r = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(c2r, "B1");
    wubumodel_node_append(d, c2, c2r);
    wubumodel_node_append(d, row, c1);
    wubumodel_node_append(d, row, c2);
    wubumodel_node_append(d, tbl, row);
    wubumodel_node_append(d, sec, tbl);

    return d;
}

/* walk helpers */
static int count_kind(wubumodel_node *root, wubumodel_kind k){
    if (!root) return 0;
    int n = (wubumodel_node_kind(root) == k) ? 1 : 0;
    for (wubumodel_node *c = wubumodel_node_first_child(root); c;
         c = wubumodel_node_next_sibling(c))
        n += count_kind(c, k);
    return n;
}
static const char *find_run_text(wubumodel_node *root, const char *needle){
    if (!root) return NULL;
    if (wubumodel_node_kind(root) == WUBUMODEL_RUN){
        const char *t = wubumodel_run_text(root);
        if (t && strcmp(t, needle) == 0) return t;
    }
    for (wubumodel_node *c = wubumodel_node_first_child(root); c;
         c = wubumodel_node_next_sibling(c)){
        const char *r = find_run_text(c, needle);
        if (r) return r;
    }
    return NULL;
}
static int has_link_target(wubumodel_node *root, const char *href){
    if (!root) return 0;
    if (wubumodel_node_kind(root) == WUBUMODEL_LINK){
        const char *l = wubumodel_node_link(root);
        if (l && strcmp(l, href) == 0) return 1;
    }
    for (wubumodel_node *c = wubumodel_node_first_child(root); c;
         c = wubumodel_node_next_sibling(c))
        if (has_link_target(c, href)) return 1;
    return 0;
}

int main(void){
    const char *ep = "/tmp/wubu_ep_rt.epub";
    unlink(ep);

    wubumodel_doc *d = make_doc();
    CHECK(epub_write(d, ep, "Round Trip Book", "en") == 0, "epub_write ok");
    wubumodel_doc_destroy(d);

    wubumodel_doc *back = NULL;
    CHECK(wubumodel_load_epub(ep, &back) == 0 && back, "load_epub ok");

    if (back){
        wubumodel_node *root = wubumodel_doc_root(back);
        CHECK(root != NULL, "has root section");
        CHECK(count_kind(root, WUBUMODEL_SECTION) >= 1, ">=1 SECTION");
        CHECK(count_kind(root, WUBUMODEL_PARAGRAPH) >= 3, ">=3 PARAGRAPH");
        CHECK(count_kind(root, WUBUMODEL_TABLE) == 1, "1 TABLE");
        CHECK(count_kind(root, WUBUMODEL_CELL) == 3, "3 CELL (1 row + 2 cols)");
        CHECK(count_kind(root, WUBUMODEL_LINK) == 1, "1 LINK");
        CHECK(find_run_text(root, "Chapter One") != NULL, "heading text survived");
        CHECK(find_run_text(root, "The quick brown fox.") != NULL, "paragraph text survived");
        CHECK(find_run_text(root, "A1") != NULL, "cell A1 survived");
        CHECK(find_run_text(root, "B1") != NULL, "cell B1 survived");
        CHECK(has_link_target(root, "https://example.com"), "link target survived");
        wubumodel_doc_destroy(back);
    }

    /* error path: not an epub */
    CHECK(wubumodel_load_epub("/nonexistent.epub", &back) == -1,
          "missing file -> -1");

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: epub writer<->model loader round trip (heading/para/table/link)\n");
    return 0;
}
