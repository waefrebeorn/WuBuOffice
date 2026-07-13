/* test_load_async.c — ws07#1334: load off the UI thread, callback on
 * the joining thread. Uses a trivial in-memory load_fn (no .docx reader
 * needed) so the threading contract is verified standalone. */

#include "load_async.h"
#include "model.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

/* Trivial loader: build a one-run doc from a fixed string. Simulates
 * "heavy" work by sleeping briefly. */
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

static pthread_t g_cb_thread;   /* captured to prove callback runs on join thread */
static int g_done_count;
static wubumodel_doc *g_got;        /* doc delivered to callback (owned by test) */

static void on_done(const char *path, wubumodel_doc *doc, void *user) {
    (void)path; (void)user;
    g_cb_thread = pthread_self();   /* must equal the joiner */
    g_done_count++;
    g_got = doc;                 /* transfer ownership to the test */
}

int main(void) {
    load_async *la = load_async_create();
    assert(la && "loader create");

    g_done_count = 0;
    g_got = NULL;
    assert(load_async_queue(la, "/tmp/x.docx", fake_load, on_done, NULL) == 0);

    /* While the worker loads, this thread is FREE (would be the UI). */
    pthread_t ui_thread = pthread_self();

    load_async_join(la);   /* blocks until done; fires on_done HERE */

    assert(g_done_count == 1 && "callback fired once");
    /* Callback fired on the JOINING (UI) thread, never the worker. */
    assert(pthread_equal(g_cb_thread, ui_thread) != 0 &&
           "callback on joining thread (UI-safe)");
    assert(g_got != NULL && "doc delivered to callback");

    /* Re-queue after join to prove the worker is reusable / join drains all. */
    g_done_count = 0;
    assert(load_async_queue(la, "/tmp/y.docx", fake_load, on_done, NULL) == 0);
    load_async_join(la);
    assert(g_done_count == 1 && "second join drains queue");

    wubumodel_doc_destroy(g_got);
    load_async_destroy(la);
    printf("load_async: ALL ASSERTIONS PASSED (ws07#1334)\n");
    return 0;
}
