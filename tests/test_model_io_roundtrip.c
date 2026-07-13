/* test_model_io_roundtrip.c — ws05#0885 + ws05#0884/#082 + ws07#1342/#1334.
 * Structural round-trip: build a SECTION with TWO paragraphs and a
 * TABLE (row -> cell -> paragraph -> run), incl. XML-special chars
 * in the text -> write .docx -> load .docx -> assert the NODE
 * STRUCTURE and every run's text survive loss-lessly. Uses our
 * OWN from-scratch ZIP+DEFLATE (no zlib, ws07#1338). Also
 * wires doc_cache (instant re-open) + load_async (off-UI-thread).
 * Explicit CHECK (not assert) so failures surface under -DNDEBUG. */

#include "model.h"
#include "doc_cache.h"
#include "load_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CHECK(c, msg) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, msg); return 1; } } while (0)

#define PATH "/tmp/wubu_rt.docx"

/* Count runs/paragraphs under a node, and collect run texts. */
static int g_runs, g_pars, g_tables;
static void walk(wubumodel_node *n) {
    for (; n; n = wubumodel_node_next_sibling(n)) {
        wubumodel_kind k = wubumodel_node_kind(n);
        if (k == WUBUMODEL_PARAGRAPH) g_pars++;
        if (k == WUBUMODEL_RUN)        g_runs++;
        if (k == WUBUMODEL_TABLE)       g_tables++;
        walk(wubumodel_node_first_child(n));
    }
}

/* find the Nth RUN's text anywhere in the tree (depth-first) */
static const char *g_nth_text;
static void collect_run(wubumodel_node *n, int *i, int nth) {
    for (; n; n = wubumodel_node_next_sibling(n)) {
        if (wubumodel_node_kind(n) == WUBUMODEL_RUN && (*i)++ == nth) {
            g_nth_text = wubumodel_run_text(n);
            return;
        }
        collect_run(wubumodel_node_first_child(n), i, nth);
        if (g_nth_text) return;
    }
}
static const char *nth_run_text(wubumodel_doc *d, int nth) {
    g_nth_text = NULL;
    int i = 0;
    collect_run(wubumodel_doc_root(d), &i, nth);
    return g_nth_text;
}

static wubumodel_doc *build(void) {
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);

    /* paragraph 1 */
    wubumodel_node *p1 = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *r1 = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(r1, "Hello & <world> \"quotes\" 'apos' 123");
    wubumodel_node_append(d, p1, r1);
    wubumodel_node_append(d, sec, p1);

    /* paragraph 2 */
    wubumodel_node *p2 = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *r2 = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(r2, "second paragraph text");
    wubumodel_node_append(d, p2, r2);
    wubumodel_node_append(d, sec, p2);

    /* table: 1 row, 1 cell, 1 paragraph, 1 run */
    wubumodel_node *tbl = wubumodel_node_create(d, WUBUMODEL_TABLE);
    wubumodel_node *row = wubumodel_node_create(d, WUBUMODEL_CELL); /* row */
    wubumodel_node *cell = wubumodel_node_create(d, WUBUMODEL_CELL);
    wubumodel_node *cp = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *cr = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(cr, "cell value 42");
    wubumodel_node_append(d, cp, cr);
    wubumodel_node_append(d, cell, cp);
    wubumodel_node_append(d, row, cell);
    wubumodel_node_append(d, tbl, row);
    wubumodel_node_append(d, sec, tbl);
    return d;
}

static int load_fn(const char *path, wubumodel_doc **out) {
    return wubumodel_load_docx(path, out);
}
static wubumodel_doc *g_loaded;
static int g_cb;
static void on_done(const char *path, wubumodel_doc *doc, void *u) {
    (void)path; (void)u; g_cb++; g_loaded = doc;
}

int main(void) {
    wubumodel_doc *d = build();
    CHECK(wubumodel_write_docx(d, PATH) == 0, "write_docx");
    wubumodel_doc_destroy(d);

    /* --- sync load + STRUCTURAL round-trip --- */
    wubumodel_doc *d2 = NULL;
    CHECK(wubumodel_load_docx(PATH, &d2) == 0, "load_docx");
    CHECK(d2, "loaded doc");

    g_runs = g_pars = g_tables = 0;
    walk(wubumodel_doc_root(d2));
    CHECK(g_pars == 3, "3 paragraphs (2 + 1 in cell)");   /* p1,p2,cell-p */
    CHECK(g_runs  == 3, "3 runs");
    CHECK(g_tables == 1, "1 table");

    const char *t0 = nth_run_text(d2, 0);
    const char *t1 = nth_run_text(d2, 1);
    const char *t2 = nth_run_text(d2, 2);
    CHECK(t0 && strcmp(t0, "Hello & <world> \"quotes\" 'apos' 123") == 0, "run0 text fidelity");
    CHECK(t1 && strcmp(t1, "second paragraph text") == 0, "run1 text fidelity");
    CHECK(t2 && strcmp(t2, "cell value 42") == 0, "run2 (cell) text fidelity");

    /* --- cache: instant re-open returns same parsed doc --- */
    doc_cache *c = doc_cache_create();
    struct stat st;
    CHECK(stat(PATH, &st) == 0, "stat");
    CHECK(doc_cache_put(c, PATH, (long)st.st_mtime, (long long)st.st_size, d2) == 0, "cache put");
    CHECK(doc_cache_get(c, PATH) == d2, "cache re-open returns parsed doc");

    /* --- async: off-UI-thread load, callback on join-thread --- */
    load_async *la = load_async_create();
    g_cb = 0; g_loaded = NULL;
    CHECK(load_async_queue(la, PATH, load_fn, on_done, NULL) == 0, "async queue");
    load_async_join(la);
    CHECK(g_cb == 1, "async callback fired");
    CHECK(g_loaded, "async loaded doc");
    g_runs = g_pars = g_tables = 0;
    walk(wubumodel_doc_root(g_loaded));
    CHECK(g_runs == 3 && g_pars == 3 && g_tables == 1, "async load structure fidelity");
    const char *a0 = nth_run_text(g_loaded, 0);
    CHECK(a0 && strcmp(a0, "Hello & <world> \"quotes\" 'apos' 123") == 0, "async run0 fidelity");

    wubumodel_doc_destroy(g_loaded);
    doc_cache_destroy(c);
    load_async_destroy(la);

    printf("model_io_roundtrip: structural write->load->cache->async PASSED "
           "(ws05#0885/#0884/#082, ws07#1342/#1334)\n");
    return 0;
}
