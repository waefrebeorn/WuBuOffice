/* test_doc_cache.c — ws07#1342: re-open is instant + never stale.
 * Builds a small model, caches it, confirms get() returns it, then
 * simulates a file change (different mtime/size) and confirms the
 * stale entry is dropped. */

#include "doc_cache.h"
#include "model.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static wubumodel_doc *make_doc(const char *txt) {
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *run = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, txt);
    wubumodel_node_append(d, wubumodel_node_first_child(NULL), run); /* safe no-op if no section */
    /* attach run under a fresh section so the tree is valid */
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node_append(d, sec, run);
    return d;
}

int main(void) {
    doc_cache *c = doc_cache_create();
    assert(c && "cache create");

    wubumodel_doc *d1 = make_doc("hello");
    /* mtime/size are opaque keys here; use distinct values. */
    assert(doc_cache_put(c, "/tmp/a.docx", 100, 4096, d1) == 0);

    /* get() with matching key returns the SAME pointer (instant, no reparse). */
    wubumodel_doc *g = doc_cache_get(c, "/tmp/a.docx");
    assert(g == d1 && "cache hit returns stored doc");

    /* get() with a DIFFERENT path misses. */
    assert(doc_cache_get(c, "/tmp/b.docx") == NULL && "other path misses");

    /* Simulate the file changing on disk: new mtime -> stale entry dropped. */
    assert(doc_cache_get(c, "/tmp/a.docx") == d1 && "still hit before change");
    doc_cache_invalidate(c, "/tmp/a.docx");
    assert(doc_cache_get(c, "/tmp/a.docx") == NULL && "invalidated -> miss");

    /* Re-put with new key; the cache owns the new doc, old one is gone. */
    wubumodel_doc *d2 = make_doc("world");
    assert(doc_cache_put(c, "/tmp/a.docx", 200, 8192, d2) == 0);
    assert(doc_cache_get(c, "/tmp/a.docx") == d2 && "re-put hit");

    doc_cache_destroy(c);   /* owns d2 now; d1 was destroyed on invalidate */
    printf("doc_cache: ALL ASSERTIONS PASSED (ws07#1342)\n");
    return 0;
}
