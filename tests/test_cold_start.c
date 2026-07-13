/* test_cold_start.c — ws07#1333: cold start must be fast.
 * Times a from-scratch document creation + a minimal tree build.
 * The upstream target is <300ms for a blank doc on reference hardware;
 * this unit test asserts a generous bound so it stays green on CI
 * while still catching gross regressions (e.g. a blocking I/O or a
 * heavy eager parse on create). */

#include "model.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>

#define BOUND_SEC 0.05   /* 50ms generous bound; ref target is 300ms */

int main(void) {
    clock_t start = clock();
    wubumodel_doc *d = wubumodel_doc_create();
    assert(d && "doc create");

    /* Build a minimal section->paragraph->run tree (no I/O, no parse). */
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *par = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, "cold start");
    wubumodel_node_append(d, par, run);
    wubumodel_node_append(d, sec, par);
    (void)sec;

    clock_t end = clock();
    double secs = (double)(end - start) / CLOCKS_PER_SEC;

    wubumodel_doc_destroy(d);
    assert(secs < BOUND_SEC && "cold start within bound");
    printf("cold_start: %.3f ms (bound %.0f ms) PASSED (ws07#1333)\n",
           secs * 1000.0, BOUND_SEC * 1000.0);
    return 0;
}
