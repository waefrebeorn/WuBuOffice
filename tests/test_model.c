/* WS11 model green-baseline smoke test. Covers: doc lifecycle, node create
 * with stable id, RUN text, shared style, set-text command + undo, observer. */
#include "model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_changes = 0;
static void on_change(wubumodel_doc *d, wubumodel_id n, void *u) {
    (void)d; (void)n; (void)u;
    g_changes++;
}

int main(void) {
    wubumodel_doc *doc = wubumodel_doc_create();
    assert(doc);

    wubumodel_node *run = wubumodel_node_create(doc, WUBUMODEL_RUN);
    assert(run);
    wubumodel_id id = wubumodel_node_id(run);
    assert(id == 1);
    assert(wubumodel_node_kind(run) == WUBUMODEL_RUN);
    assert(wubumodel_node_find(doc, id) == run);
    (void)wubumodel_node_find(doc, id); /* keep `id` referenced under -DNDEBUG */

    /* style attach (shared pointer) */
    wubumodel_style *s = wubumodel_style_create();
    assert(s);
    assert(wubumodel_style_set_prop(s, "font", "serif") == 0);
    assert(wubumodel_node_set_style(run, s) == 0);
    assert(strcmp(wubumodel_style_get_prop(s, "font"), "serif") == 0);
    wubumodel_style_destroy(s); /* node holds a ref; safe */

    /* observer */
    assert(wubumodel_on_change(doc, on_change, NULL) == 0);
    (void)on_change; /* referenced unconditionally so it is not flagged unused under -DNDEBUG */

    /* set-text command + undo */
    assert(wubumodel_cmd_set_text(doc, run, "hello") == 0);
    assert(strcmp(wubumodel_run_text(run), "hello") == 0);
    assert(g_changes == 1);

    assert(wubumodel_cmd_set_text(doc, run, "world") == 0);
    assert(strcmp(wubumodel_run_text(run), "world") == 0);
    assert(g_changes == 2);

    assert(wubumodel_doc_undo(doc) == 0);
    assert(strcmp(wubumodel_run_text(run), "hello") == 0);
    assert(g_changes == 2); /* undo does not re-emit */

    assert(wubumodel_doc_undo(doc) == 0);
    assert(wubumodel_run_text(run) == NULL); /* back to empty */

    assert(wubumodel_doc_undo(doc) == -1); /* nothing left */

    wubumodel_node_destroy(doc, run);
    assert(wubumodel_node_find(doc, id) == NULL);

    wubumodel_doc_destroy(doc);
    printf("model: all green-baseline checks passed\n");
    return 0;
}
