/* test_doc_cache.c — ws07#1342: re-open is instant + never stale.
 * Creates a REAL temp file so the (path, mtime, size) key the cache
 * computes on get() matches what we put(). Explicit CHECK (not assert)
 * so failures surface under -DNDEBUG too. */

#include "doc_cache.h"
#include "model.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#define PATH "/tmp/wubu_cache_test.docx"

#define CHECK(c, msg) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, msg); return 1; } } while (0)

static wubumodel_doc *make_doc(const char *txt) {
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *run = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, txt);
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node_append(d, sec, run);
    return d;
}

static void file_stat(long *mt, long long *sz) {
    struct stat st;
    CHECK(stat(PATH, &st) == 0, "stat temp file");
    *mt = (long)st.st_mtime;
    *sz = (long long)st.st_size;
}

int main(void) {
    /* create the temp file with known content */
    FILE *fp = fopen(PATH, "wb");
    CHECK(fp, "create temp file");
    fputs("hello world docx", fp);
    fclose(fp);

    doc_cache *c = doc_cache_create();
    CHECK(c, "cache create");

    long mt; long long sz;
    file_stat(&mt, &sz);

    wubumodel_doc *d1 = make_doc("hello");
    CHECK(doc_cache_put(c, PATH, mt, sz, d1) == 0, "put d1 with real key");

    /* get() re-stats the file -> key matches -> returns SAME pointer (instant) */
    wubumodel_doc *g = doc_cache_get(c, PATH);
    CHECK(g == d1, "cache hit returns stored doc");

    CHECK(doc_cache_get(c, "/tmp/does_not_exist_xyz.docx") == NULL, "other path misses");

    /* simulate file change: rewrite with different size -> stale entry dropped */
    fp = fopen(PATH, "wb");
    CHECK(fp, "rewrite temp file");
    fputs("hello world docx with extra bytes now", fp);
    fclose(fp);

    CHECK(doc_cache_get(c, PATH) == NULL, "changed file -> stale entry dropped");

    /* re-put with the new key */
    file_stat(&mt, &sz);
    wubumodel_doc *d2 = make_doc("world");
    CHECK(doc_cache_put(c, PATH, mt, sz, d2) == 0, "re-put d2 with new key");
    CHECK(doc_cache_get(c, PATH) == d2, "re-put hit");

    doc_cache_destroy(c);   /* owns d2; d1 was destroyed on stale-drop */
    remove(PATH);
    printf("doc_cache: ALL CHECKS PASSED (ws07#1342)\n");
    return 0;
}
