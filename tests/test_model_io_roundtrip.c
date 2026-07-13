/* test_model_io_roundtrip.c — ws05#0885 + ws07#1342/#1334.
 * Round-trip: build text (incl. XML-special chars) -> write .docx ->
 * load .docx -> assert text survives loss-lessly, using our OWN
 * from-scratch ZIP+DEFLATE (no zlib in the prod path). Also wires
 * doc_cache (instant re-open) + load_async (off-thread load).
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

static const char *first_run_text(wubumodel_doc *d) {
    wubumodel_node *root = wubumodel_doc_root(d);
    for (; root; root = wubumodel_node_next_sibling(root)) {
        for (wubumodel_node *p = wubumodel_node_first_child(root);
             p; p = wubumodel_node_next_sibling(p)) {
            for (wubumodel_node *r = wubumodel_node_first_child(p);
                 r; r = wubumodel_node_next_sibling(r)) {
                if (wubumodel_node_kind(r) == WUBUMODEL_RUN)
                    return wubumodel_run_text(r);
            }
        }
    }
    return NULL;
}

static wubumodel_doc *build(const char *txt) {
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *par = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, txt);
    wubumodel_node_append(d, par, run);
    wubumodel_node_append(d, sec, par);
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
    const char *orig = "Hello & <world> \"quotes\" 'apos' 123";

    wubumodel_doc *d = build(orig);
    CHECK(wubumodel_write_docx(d, PATH) == 0, "write_docx");
    wubumodel_doc_destroy(d);

    wubumodel_doc *d2 = NULL;
    CHECK(wubumodel_load_docx(PATH, &d2) == 0, "load_docx");
    CHECK(d2, "loaded doc");
    const char *back = first_run_text(d2);
    CHECK(back, "run text present after load");
    CHECK(strcmp(back, orig) == 0, "text round-trips loss-lessly");

    doc_cache *c = doc_cache_create();
    struct stat st;
    CHECK(stat(PATH, &st) == 0, "stat");
    CHECK(doc_cache_put(c, PATH, (long)st.st_mtime, (long long)st.st_size, d2) == 0, "cache put");
    CHECK(doc_cache_get(c, PATH) == d2, "cache re-open returns parsed doc");

    load_async *la = load_async_create();
    g_cb = 0; g_loaded = NULL;
    CHECK(load_async_queue(la, PATH, load_fn, on_done, NULL) == 0, "async queue");
    load_async_join(la);
    CHECK(g_cb == 1, "async callback fired");
    CHECK(g_loaded, "async loaded doc");
    const char *back2 = first_run_text(g_loaded);
    CHECK(back2 && strcmp(back2, orig) == 0, "async load text fidelity");

    wubumodel_doc_destroy(g_loaded);
    doc_cache_destroy(c);
    load_async_destroy(la);

    printf("model_io_roundtrip: write->load->cache->async, text loss-less PASSED "
           "(ws05#0885, ws07#1342/#1334)\n");
    return 0;
}
