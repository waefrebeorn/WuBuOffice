#include "doccmd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "a11y.h"
#include "col.h"
#include "cite.h"
#include "caption.h"
#include "heading.h"
#include "eqnum.h"
#include "vars.h"
#include "hash.h"
#include "sig.h"
#include "crdt.h"
#include "csv.h"
#include "focus.h"
#include "watermark.h"
#include "dyslexia.h"
#include "fmtpaint.h"
#include "sandbox.h"
#include "form.h"
#include "history.h"
#include "lang.h"
#include "nesttab.h"
#include "pdfextract.h"
#include "pdfform.h"
#include "scope.h"
#include "sync.h"
#include "xps.h"
#include "aislot.h"
#include "script.h"
#include "model.h"


/* doccmd_col_demo -- append/resolve a thread on the comment store. Used to
 * exercise the wubucol engine from the document shell. Returns status. */
char *doccmd_col_demo(void){
    Col *c = col_create();
    if (!c) return strdup("col_create failed");
    int tid = col_add(c, "doc:root", "user", "first comment on root");
    if (tid < 0){ col_destroy(c); return strdup("col_add failed"); }
    col_reply(c, tid, "reviewer", "agreed");
    col_resolve(c, tid, 1);
    int n = col_thread_count(c);
    char b[128];
    snprintf(b, sizeof b, "col ok: threads=%d resolved=%d",
             n, col_resolved(c, tid));
    col_destroy(c);
    return strdup(b);
}

/* doccmd_cite_demo -- add a citation entry and render inline + bibliography
 * text. Exercises src/wubucite (DOC-68). Writes bibliography to /tmp/wubuos_bib.txt. */
char *doccmd_cite_demo(void){
    Cite *c = cite_create();
    if (!c) return strdup("cite_create failed");
    cite_add(c, "smith2020", "article", "On CRDTs", "Alice Smith; Bob Jones", 2020);
    cite_add(c, "doe2019", "book", "Distributed Systems", "Jane Doe", 2019);
    char *inline1 = cite_inline(c, "smith2020");
    char *bib = cite_bibliography(c);
    int n = cite_count(c);
    char b[256];
    snprintf(b, sizeof b, "cite ok: count=%d inline1=%s bib_len=%zu",
             n, inline1 ? inline1 : "(null)", bib ? strlen(bib) : 0);
    if (bib){
        FILE *f = fopen("/tmp/wubuos_bib.txt", "wb");
        if (f){ fputs(bib, f); fclose(f); }
    }
    free(inline1); free(bib); cite_destroy(c);
    return strdup(b);
}

/* doccmd_caption_demo -- bind captions to (synthetic) node ids. Exercises
 * src/wubucaption (DOC-70). */
char *doccmd_caption_demo(void){
    CaptionMap *m = caption_create();
    if (!m) return strdup("caption_create failed");
    caption_set(m, 1001, "Figure 1: System architecture");
    caption_set(m, 1002, "Figure 2: CRDT merge diagram");
    const char *c2 = caption_get(m, 1002);
    char b[256];
    snprintf(b, sizeof b, "caption ok: count=%d node1002=%s",
             caption_count(m), c2 ? c2 : "(null)");
    caption_destroy(m);
    return strdup(b);
}

/* doccmd_heading_enforce -- walk the doc's SECTION nodes and assign sequential
 * levels. Exercises src/wubuheading (UXA-49/50). */
char *doccmd_heading_enforce(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    Heading *h = heading_create();
    if (!h) return strdup("heading_create failed");
    int n = heading_enforce(h, doc);
    char b[128];
    snprintf(b, sizeof b, "heading enforce ok: counted=%d", n);
    heading_destroy(h);
    return strdup(b);
}

/* doccmd_eqnum_scan -- number every equation-bearing FIELD node. Exercises
 * src/wubueqnum (DOC-69). */
char *doccmd_eqnum_scan(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    EqNum *e = eqnum_create();
    if (!e) return strdup("eqnum_create failed");
    int n = eqnum_scan(e, doc);
    char b[128];
    snprintf(b, sizeof b, "eqnum scan ok: numbered=%d", n);
    eqnum_destroy(e);
    return strdup(b);
}

/* doccmd_vars_expand -- set 2 variables and expand a template. Exercises
 * src/wubuvars (DOC-73). */
char *doccmd_vars_expand(const char *tpl){
    Vars *v = vars_create();
    if (!v) return strdup("vars_create failed");
    vars_set(v, "name", "Hermes");
    vars_set(v, "year", "2026");
    const char *text = tpl ? tpl : "Hello, ${name}! Year ${year}.";
    char *out = vars_expand(v, text);
    char b[256];
    snprintf(b, sizeof b, "vars expand: count=%d out=%s",
             vars_count(v), out ? out : "(null)");
    free(out); vars_destroy(v);
    return strdup(b);
}

/* doccmd_hash_sha256 -- one-shot SHA-256 of a string, hex output. Exercises
 * src/wubuhash (FIPS 180-4). */
char *doccmd_hash_sha256(const char *text){
    if (!text) return strdup("no text");
    uint8_t dig[WUBUHASH_SHA256_SIZE];
    char hex[WUBUHASH_SHA256_SIZE * 2 + 1];
    sha256(text, strlen(text), dig);
    hash_hex(dig, hex);
    char b[160];
    snprintf(b, sizeof b, "sha256: %s", hex);
    return strdup(b);
}

/* doccmd_doc_sign -- HMAC-SHA256 sign a fixed message with a fixed key.
 * Exercises src/wubusig (EXP-90) which links against wubuhash. */
char *doccmd_doc_sign(void){
    const char *msg = "WuBuOffice document body";
    const char *key = "wubu-test-key-2026";
    uint8_t sig[32];
    char hex[65];
    sig_sign(msg, strlen(msg), key, strlen(key), sig);
    sig_hex(sig, hex);
    int ok = sig_verify(msg, strlen(msg), key, strlen(key), sig);
    char b[128];
    snprintf(b, sizeof b, "doc sig: ok=%d sig=%s...", ok, hex);
    return strdup(b);
}

/* doccmd_crdt_demo -- exercise a CRDT replica: insert 3 items, delete 1,
 * merge a peer replica with a concurrent insert. Exercises src/wubucrdt. */
char *doccmd_crdt_demo(void){
    Crdt *a = crdt_create("site-a");
    Crdt *b = crdt_create("site-b");
    if (!a || !b){ crdt_destroy(a); crdt_destroy(b); return strdup("crdt_create failed"); }
    crdt_insert(a, 0, "alpha");
    crdt_insert(a, 1, "beta");
    crdt_insert(a, 2, "gamma");
    crdt_insert(b, 0, "delta");     /* concurrent */
    crdt_merge(a, b);
    crdt_delete(a, 0);               /* delete "alpha" */
    char b2[160];
    snprintf(b2, sizeof b2, "crdt ok: count=%d first=%s",
             crdt_count(a), crdt_count(a) > 0 ? crdt_get(a, 0) : "(empty)");
    crdt_destroy(a); crdt_destroy(b);
    return strdup(b2);
}

/* doccmd_csv_parse -- parse a small RFC-4180 CSV with quoted fields. Exercises
 * src/wubucsv (EXP-86). */
char *doccmd_csv_parse(void){
    Csv *c = csv_create();
    if (!c) return strdup("csv_create failed");
    const char *txt = "name,age,city\n\"Alice, A.\",30,\"New York\"\nBob,25,LA\n";
    int ok = csv_parse(c, txt);
    char b[160];
    snprintf(b, sizeof b, "csv parse: ok=%d rows=%d cols=%d cell00=%s",
             ok, csv_rows(c), csv_cols(c),
             csv_rows(c) > 0 ? csv_cell(c, 0, 0) : "(none)");
    csv_destroy(c);
    return strdup(b);
}

/* doccmd_focus_demo -- configure a focus indicator. Exercises src/wubufocus
 * (UXA-51). */
char *doccmd_focus_demo(void){
    Focus *f = focus_create();
    if (!f) return strdup("focus_create failed");
    focus_set_enabled(f, 1);
    focus_set_width(f, 3);
    focus_set_color(f, 0x33, 0x99, 0xFF, 0xFF);
    char b[128];
    snprintf(b, sizeof b, "focus ok: en=%d w=%d color=0x%08x",
             focus_enabled(f), focus_width(f), focus_color(f));
    focus_destroy(f);
    return strdup(b);
}

/* doccmd_watermark_demo -- configure a watermark. Exercises src/wubuwatermark
 * (DOC-71). */
char *doccmd_watermark_demo(void){
    Watermark *w = watermark_create();
    if (!w) return strdup("watermark_create failed");
    watermark_set_text(w, "DRAFT");
    watermark_set_angle(w, -30);
    watermark_set_opacity(w, 0.15f);
    watermark_set_enabled(w, 1);
    char b[160];
    snprintf(b, sizeof b, "watermark ok: en=%d text=%s angle=%d op=%.2f",
             watermark_enabled(w), watermark_text(w),
             watermark_angle(w), watermark_opacity(w));
    watermark_destroy(w);
    return strdup(b);
}

/* doccmd_dyslexia_demo -- configure dyslexia-friendly mode. Exercises
 * src/wubudyslexia (UXA-52). */
char *doccmd_dyslexia_demo(void){
    Dyslexia *d = dyslexia_create();
    if (!d) return strdup("dyslexia_create failed");
    dyslexia_set_enabled(d, 1);
    dyslexia_set_face(d, "OpenDyslexic");
    dyslexia_set_spacing(d, 1.3f);
    char b[160];
    snprintf(b, sizeof b, "dyslexia ok: en=%d face=%s spacing=%.2f",
             dyslexia_enabled(d), dyslexia_face(d), dyslexia_spacing(d));
    dyslexia_destroy(d);
    return strdup(b);
}

/* doccmd_fmtpaint_demo -- pick format from a node, apply to another. Exercises
 * src/wubufmtpaint (DOC-74). */
char *doccmd_fmtpaint_demo(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    FmtPaint *f = fmtpaint_create();
    if (!f) return strdup("fmtpaint_create failed");
    /* Use the doc's root as both src and dst for the demo. */
    void *root = wubumodel_doc_root(doc);
    int picked = root ? fmtpaint_pick(f, root) : 0;
    int applied = root ? fmtpaint_apply(f, root) : -1;
    char b[128];
    snprintf(b, sizeof b, "fmtpaint ok: picked=%d applied=%d loaded=%d",
             picked, applied, fmtpaint_loaded(f));
    fmtpaint_destroy(f);
    return strdup(b);
}

/* doccmd_sandbox_demo -- register a plugin, grant subset of caps, exercise
 * allow/deny checks. Exercises src/wubusandbox (SCR-100). */
char *doccmd_sandbox_demo(void){
    Sandbox *s = sandbox_create();
    if (!s) return strdup("sandbox_create failed");
    int pid = sandbox_register(s, "test-plugin",
                               SBX_READ_DOC | SBX_WRITE_DOC | SBX_FS);
    if (pid < 0){ sandbox_destroy(s); return strdup("sandbox_register failed"); }
    /* Grant only read+fs, NOT write. */
    sandbox_grant(s, pid, SBX_READ_DOC | SBX_FS);
    int read_ok  = sandbox_check(s, pid, SBX_READ_DOC);
    int write_ok = sandbox_check(s, pid, SBX_WRITE_DOC);   /* should be denied */
    int fs_ok    = sandbox_check(s, pid, SBX_FS);
    int net_ok   = sandbox_check(s, pid, SBX_NET);         /* not requested */
    char b[200];
    snprintf(b, sizeof b, "sandbox ok: read=%d write=%d fs=%d net=%d denials=%d eff=0x%x",
             read_ok, write_ok, fs_ok, net_ok,
             sandbox_denials(s, pid), sandbox_effective(s, pid));
    sandbox_destroy(s);
    return strdup(b);
}

/* doccmd_form_demo -- add 3 form fields, set/get a value. Exercises
 * src/wubuform (DOC-72). */
char *doccmd_form_demo(void){
    Form *f = form_create();
    if (!f) return strdup("form_create failed");
    form_add(f, "name", FORM_TEXT, "Alice");
    form_add(f, "subscribe", FORM_CHECKBOX, "true");
    form_add(f, "plan", FORM_CHOICE, "pro");
    form_set_value(f, "name", "Bob");
    const char *v = form_value(f, "name");
    char b[160];
    snprintf(b, sizeof b, "form ok: count=%d name=%s",
             form_count(f), v ? v : "(null)");
    form_destroy(f);
    return strdup(b);
}

/* doccmd_history_demo -- commit 2 revisions, diff them. Exercises
 * src/wubuhistory (DOC-75). */
char *doccmd_history_demo(void){
    History *h = history_create();
    if (!h) return strdup("history_create failed");
    history_commit(h, "v1 text", 7, "user", "draft 1");
    history_commit(h, "v2 text", 7, "user", "draft 2");
    int n = history_count(h);
    char *diff = history_diff(h, 1, 2);
    char b[160];
    snprintf(b, sizeof b, "history ok: count=%d diff=%s",
             n, diff ? diff : "(null)");
    free(diff); history_destroy(h);
    return strdup(b);
}

/* doccmd_lang_demo -- set/get a language tag on a node id. Exercises
 * src/wubulang (DOC-76). */
char *doccmd_lang_demo(void){
    LangMap *m = lang_create();
    if (!m) return strdup("lang_create failed");
    lang_set(m, 5001, "en-US");
    lang_set(m, 5002, "fr-FR");
    const char *t1 = lang_get(m, 5001);
    const char *t2 = lang_get(m, 5002);
    char b[160];
    snprintf(b, sizeof b, "lang ok: count=%d node5001=%s node5002=%s",
             lang_count(m), t1 ? t1 : "(null)", t2 ? t2 : "(null)");
    lang_destroy(m);
    return strdup(b);
}

/* doccmd_nesttab_demo -- build + nest a table, check depth. Exercises
 * src/wubunesttab (DOC-77). Requires a wubumodel_doc. */
char *doccmd_nesttab_demo(wubumodel_doc *doc){
    if (!doc) return strdup("no model doc");
    void *root = wubumodel_doc_root(doc);
    if (!root) return strdup("no root");
    void *t1 = nesttab_build(doc, root, 2, 2);
    if (!t1) return strdup("nesttab_build failed");
    void *cell = nesttab_cell(t1, 0, 0, 2);
    if (!cell) return strdup("nesttab_cell failed");
    void *t2 = nesttab_nest(doc, cell, 2, 3);
    int depth = nesttab_depth(t2);
    int valid = nesttab_validate(t2);
    char b[128];
    snprintf(b, sizeof b, "nesttab ok: depth=%d valid=%d", depth, valid);
    return strdup(b);
}

/* doccmd_pdfextract_demo -- extract text from a tiny PDF. Exercises
 * src/wubupdfextract (EXP-91). */
char *doccmd_pdfextract_demo(void){
    /* A minimal valid PDF with a text "Hi" in stream. */
    const char *pdf =
        "%PDF-1.0\n1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
        "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
        "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] "
        "/Contents 4 0 R >>\nendobj\n"
        "4 0 obj\n<< /Length 20 >>\nstream\nBT /F1 12 Tf (Hi) Tj ET\nendstream\nendobj\n"
        "xref\n0 5\n0000000000 65535 f \ntrailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n0\n%%EOF\n";
    char *text = pdfextract_bytes((const uint8_t*)pdf, strlen(pdf));
    char b[160];
    snprintf(b, sizeof b, "pdfextract ok: text=%s",
             text ? text : "(null)");
    free(text);
    return strdup(b);
}

/* doccmd_pdfform_demo -- build a PDF form from a Form, write to /tmp. Exercises
 * src/wubupdfform (EXP-92). */
char *doccmd_pdfform_demo(void){
    Form *f = form_create();
    if (!f) return strdup("form_create failed");
    form_add(f, "username", FORM_TEXT, "");
    form_add(f, "agree", FORM_CHECKBOX, "true");
    int rc = pdfform_write_file(f, "/tmp/wubuos_form.pdf");
    char b[128];
    snprintf(b, sizeof b, "pdfform ok: rc=%d path=/tmp/wubuos_form.pdf", rc);
    form_destroy(f);
    return strdup(b);
}

/* doccmd_scope_demo -- set/get scope labels on table ids. Exercises
 * src/wubuscope (DOC-78). */
char *doccmd_scope_demo(void){
    ScopeMap *m = scope_create();
    if (!m) return strdup("scope_create failed");
    scope_set(m, 7001, "Sheet1.A1:C10");
    scope_set(m, 7002, "Sheet2.D1:F20");
    const char *s1 = scope_get(m, 7001);
    char b[160];
    snprintf(b, sizeof b, "scope ok: count=%d table7001=%s",
             scope_count(m), s1 ? s1 : "(null)");
    scope_destroy(m);
    return strdup(b);
}

/* doccmd_sync_demo -- open a /tmp sync store, put+get a blob. Exercises
 * src/wubusync (DOC-79). */
char *doccmd_sync_demo(void){
    Sync *s = sync_open("/tmp/wubuos_sync_test");
    if (!s) return strdup("sync_open failed");
    const char *blob = "replica-v1-data";
    sync_put(s, "doc:hash123", blob, strlen(blob), "site-alpha");
    char *got = NULL;
    size_t gotlen = 0;
    int found = sync_get(s, "doc:hash123", &got, &gotlen);
    char b[128];
    snprintf(b, sizeof b, "sync ok: found=%d len=%zu", found, gotlen);
    free(got); sync_close(s);
    return strdup(b);
}

/* doccmd_xps_demo -- write a tiny XPS file to /tmp. Exercises src/wubuxps
 * (EXP-93). */
char *doccmd_xps_demo(void){
    int rc = xps_write_file("/tmp/wubuos_export.xps", "Hello XPS", 200, 200);
    char b[128];
    snprintf(b, sizeof b, "xps ok: rc=%d path=/tmp/wubuos_export.xps", rc);
    return strdup(b);
}

/* doccmd_aislot_demo -- run the built-in (rule-based) AI summarizer. Exercises
 * src/wubuaislot (SCR-99). */
char *doccmd_aislot_demo(void){
    AiSlot *s = aislot_create();
    if (!s) return strdup("aislot_create failed");
    aislot_set_provider(s, NULL, NULL);  /* use built-in fallback */
    char *out = aislot_run(s, "summarize", "This is a long document about CRDTs and real-time collaboration.");
    char b[256];
    snprintf(b, sizeof b, "aislot ok: custom=%d out=%s",
             aislot_has_custom_provider(s), out ? out : "(null)");
    free(out); aislot_destroy(s);
    return strdup(b);
}

/* ---- Spreadsheet & document analysis modules (2026-08-11 wave) ---- */

