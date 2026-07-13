/* test_load_async.c — ws07#1334: load off the UI thread, callback on
 * the joining thread. Explicit CHECK (not assert) so failures surface
 * under -DNDEBUG too. */

#include "load_async.h"
#include "model.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define CHECK(c, msg) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, msg); return 1; } } while (0)

static int fake_load(const char *path, wubumodel_doc **out) {
    (void)path;
    wubumodel_doc *d = wubumodel_doc_create();
    if (!d) return -1;
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *run = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, "loaded async");
    wubumodel_node_append(d, sec, run);
    *out = d;
    return 0;
}

static pthread_t g_cb_thread;
static int g_done_count;
static wubumodel_doc *g_got;

static void on_done(const char *path, wubumodel_doc *doc, void *user) {
    (void)path; (void)user;
    g_cb_thread = pthread_self();
    g_done_count++;
    g_got = doc;
}

int main(void) {
    load_async *la = load_async_create();
    CHECK(la, "loader create");

    g_done_count = 0;
    g_got = NULL;
    CHECK(load_async_queue(la, "/tmp/x.docx", fake_load, on_done, NULL) == 0, "queue");

    pthread_t ui_thread = pthread_self();

    load_async_join(la);
    CHECK(g_done_count == 1, "callback fired once");
    CHECK(pthread_equal(g_cb_thread, ui_thread) != 0, "callback on joining thread (UI-safe)");
    CHECK(g_got != NULL, "doc delivered to callback");
    wubumodel_doc_destroy(g_got);   /* ownership transferred to callback */
    g_got = NULL;

    g_done_count = 0;
    CHECK(load_async_queue(la, "/tmp/y.docx", fake_load, on_done, NULL) == 0, "queue 2");
    load_async_join(la);
    CHECK(g_done_count == 1, "second join drains queue");
    wubumodel_doc_destroy(g_got);

    load_async_destroy(la);
    printf("load_async: ALL CHECKS PASSED (ws07#1334)\n");
    return 0;
}
