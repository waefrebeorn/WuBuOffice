/* test_autosave.c -- wubuautosave acceptance test (atomic snapshots + recovery) */
#include "autosave.h"
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c,m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

/* build a tiny doc: section -> paragraph -> run("hello world"). The SECTION
 * is left parent-less so it is the document's top-level node (wubumodel
 * serializes all parent-less nodes; only the section qualifies here). */
static wubumodel_doc *make_doc(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *sec  = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *para = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run  = wubumodel_node_create(d, WUBUMODEL_RUN);
    wubumodel_run_set_text(run, "hello world");
    wubumodel_node_append(d, para, run);
    wubumodel_node_append(d, sec, para);
    return d;
}

int main(void){
    const char *doc = "/tmp/wubu_as_test.docx";
    unlink("/tmp/wubu_as_test.docx.asd");
    unlink("/tmp/wubu_as_test.docx.lock");

    /* --- autosave + flush --- */
    Autosave *a = wubuautosave_create(doc, 0);
    CHECK(a != NULL, "autosave session created");
    wubumodel_doc *d = make_doc();
    wubuautosave_mark_dirty(a);
    CHECK(wubuautosave_tick(a, d) == 0, "tick with interval=0 skips (no write)");
    CHECK(wubuautosave_flush(a, d) == 0, "flush writes snapshot");
    CHECK(access("/tmp/wubu_as_test.docx.asd", F_OK) == 0, "asd file exists after flush");

    /* normal close: clear removes asd + lock */
    CHECK(wubuautosave_clear(a) == 0, "clear ok");
    CHECK(access("/tmp/wubu_as_test.docx.asd", F_OK) != 0, "asd removed after clear");
    CHECK(wubuautosave_has_recovery(doc) == 0, "no recovery offered after clear");

    wubumodel_doc_destroy(d);
    wubuautosave_destroy(a);

    /* --- crash simulation: write snapshot, then DROP the lock (simulate dead PID) --- */
    a = wubuautosave_create(doc, 0);
    d = make_doc();
    wubuautosave_flush(a, d);
    wubumodel_doc_destroy(d);
    /* simulate crash: release lock, leave asd behind */
    unlink("/tmp/wubu_as_test.docx.lock");
    CHECK(wubuautosave_has_recovery(doc) == 1, "recovery offered when asd present + no live lock");

    /* recover */
    wubumodel_doc *rec = NULL;
    int r = wubuautosave_recover(doc, &rec);
    CHECK(r == 1, "recover returns 1");
    CHECK(rec != NULL, "recovered doc non-NULL");
    if (rec){
        /* walk to first run and check its text survived the round-trip */
        wubumodel_node *sec = wubumodel_doc_root(rec);
        CHECK(sec != NULL, "recovered doc has root section");
        wubumodel_node *para = sec ? wubumodel_node_first_child(sec) : NULL;
        wubumodel_node *run  = para ? wubumodel_node_first_child(para) : NULL;
        const char *txt = run ? wubumodel_run_text(run) : NULL;
        CHECK(txt && strcmp(txt, "hello world") == 0, "recovered run text == 'hello world'");
        wubumodel_doc_destroy(rec);
    }
    /* discard recovery sidecar */
    CHECK(wubuautosave_discard_recovery(doc) == 0, "discard recovery ok");
    CHECK(access("/tmp/wubu_as_test.docx.asd", F_OK) != 0, "asd gone after discard");
    CHECK(wubuautosave_has_recovery(doc) == 0, "no recovery after discard");

    wubuautosave_destroy(a);

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuautosave (atomic snapshot, dirty-skip, crash recovery round-trip)\n");
    return 0;
}
