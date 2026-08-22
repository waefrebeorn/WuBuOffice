#include "doccmd.h"
#include "wubusort.h"
#include "wubufilter.h"
#include "wubusubtotal.h"
#include "wubugoalseek.h"
#include "wubusolver.h"
#include "wubupivot.h"
#include "wubuscenario.h"
#include "wubufreeze.h"
#include "wubuhyperlink.h"
#include "wubuthesaurus.h"
#include "wubugrammar.h"
#include "wubuindex.h"
#include "wubumailmerge.h"
#include "wubudiff.h"
#include "wubumasterdoc.h"
#include "wubudropcap.h"
#include "wuburuler.h"
#include "wubugridline.h"
#include "wubuicon.h"
#include "wubugallery.h"
#include "wubusidebar.h"
#include "wubutransition.h"
#include "wubuanimation.h"
#include "wubumasterslide.h"
#include "wubuconnector.h"
#include "wubuencrypt.h"
#include "wubumailexport.h"
#include "wubunotebookbar.h"
#include "wubuqr.h"
#include "wubusmartart.h"
#include "wububasic.h"
#include "wubu3d.h"

/* shared cell accessor for string-array rows (sort/filter) */
static const char *doccmd_scell(void *row, int col, void *ud){ (void)col;(void)ud; return *(const char**)row; }

/* doccmd_sort_demo -- sort rows by a column. src/wubusort */
char *doccmd_sort_demo(void){
    char *a = strdup("Zoe"), *b = strdup("ann"), *c = strdup("Bob"), *d = strdup("Ann");
    void *rows[4] = { &a, &b, &c, &d };
    wubusort_col col = {0,0,0};
    int rc = wubusort_rows(rows, 4, &col, 1, doccmd_scell, NULL);
    char bf[160];
    snprintf(bf, sizeof bf, "sort ok: rc=%d first=%s", rc, *(const char**)rows[0]);
    free(a); free(b); free(c); free(d);
    return strdup(bf);
}

/* doccmd_filter_demo -- autofilter a small sheet. src/wubufilter */
char *doccmd_filter_demo(void){
    char *r0=strdup("10"),*r1=strdup("5"),*r2=strdup("30"),*r3=strdup("7"),*r4=strdup("15");
    void *rows[5] = {&r0,&r1,&r2,&r3,&r4};
    wubufilter_crit crit = {0, WUBUFILTER_GT, "6"};
    size_t out[5], n = 0;
    int rc = wubufilter_apply(rows, 5, &crit, 1, doccmd_scell, NULL, out, &n);
    char bf[128];
    snprintf(bf, sizeof bf, "filter ok: rc=%d matches=%zu", rc, n);
    free(r0);free(r1);free(r2);free(r3);free(r4);
    return strdup(bf);
}

/* subtotal row + accessors */
typedef struct { const char *k; const char *v; } SubRow;
static const char *doccmd_sub_key(void *row, void *ud){ (void)ud; return ((SubRow*)row)->k; }
static const char *doccmd_sub_val(void *row, void *ud){ (void)ud; return ((SubRow*)row)->v; }
char *doccmd_subtotal_demo(void){
    SubRow s0={"A","10"},s1={"B","5"},s2={"A","30"},s3={"B","7"};
    void *rows[4] = {&s0,&s1,&s2,&s3};
    wubusub_group *g=NULL; size_t n=0;
    int rc = wubusub_aggregate(rows, 4, doccmd_sub_key, doccmd_sub_val, WUBUSUB_SUM, NULL, &g, &n);
    char bf[128];
    snprintf(bf, sizeof bf, "subtotal ok: rc=%d groups=%zu", rc, n);
    wubusub_free(g, n);
    return strdup(bf);
}

/* goal-seek target f(x)=x^2-2 */
static double doccmd_seek_fn(double x, void *ud){ (void)ud; return x*x - 2.0; }
char *doccmd_goalseek_demo(void){
    wubugoalseek_result r;
    int rc = wubugoalseek(doccmd_seek_fn, 0.0, 0.0, 2.0, 1e-9, 1e-9, 32, NULL, &r);
    char bf[128];
    snprintf(bf, sizeof bf, "goalseek ok: rc=%d x=%.6f conv=%d", rc, r.x, r.converged);
    return strdup(bf);
}

/* solver: solve 2x+3y=8, 4x-y=2 -> x=1,y=2 */
char *doccmd_solver_demo(void){
    double A[4]={2,3,4,-1}, b[2]={8,2}, x[2];
    int rc = wubusolver_solve(A,2,b,x);
    char bf[128];
    snprintf(bf, sizeof bf, "solver ok: rc=%d x=%.2f y=%.2f", rc, x[0], x[1]);
    return strdup(bf);
}

/* pivot row + accessors */
typedef struct { const char *r; const char *c; const char *v; } PivRow;
static const char *doccmd_piv_r(void *row, void *ud){ (void)ud; return ((PivRow*)row)->r; }
static const char *doccmd_piv_c(void *row, void *ud){ (void)ud; return ((PivRow*)row)->c; }
static const char *doccmd_piv_v(void *row, void *ud){ (void)ud; return ((PivRow*)row)->v; }
char *doccmd_pivot_demo(void){
    PivRow p0={"East","Apples","10"},p1={"East","Pears","20"},p2={"West","Apples","5"};
    void *rows[3] = {&p0,&p1,&p2};
    wubupiv *t = wubupiv_build(rows,3,doccmd_piv_r,doccmd_piv_c,doccmd_piv_v,WUBUPIV_SUM,NULL);
    double v=0; int rc = t ? wubupiv_get(t,"East","Apples",&v) : -1;
    char bf[128];
    snprintf(bf, sizeof bf, "pivot ok: rows=%zu cols=%zu East/Apples=%.0f", t?t->nrows:0, t?t->ncols:0, v);
    wubupiv_free(t);
    return strdup(bf);
}

/* scenario */
char *doccmd_scenario_demo(void){
    wubuscenario *s = wubuscenario_create();
    wubuscen_cell cells[2] = { {0,0,"10"}, {1,1,"20"} };
    int rc = wubuscenario_set(s, "Pessimistic", cells, 2);
    size_t n = wubuscenario_count(s);
    wubuscenario_destroy(s);
    char bf[128];
    snprintf(bf, sizeof bf, "scenario ok: rc=%d scenarios=%zu", rc, n);
    return strdup(bf);
}

/* freeze panes */
char *doccmd_freeze_demo(void){
    wubufreeze f;
    int rc = wubufreeze_init(&f, 1, 1);
    int vr = wubufreeze_visible_row(&f, 5);
    char bf[128];
    snprintf(bf, sizeof bf, "freeze ok: rc=%d frozen_rows=%d vis_row5=%d", rc, wubufreeze_frozen_rows(&f), vr);
    return strdup(bf);
}

/* hyperlink */
char *doccmd_hyperlink_demo(void){
    wubuhyperlink *h = wubuhyperlink_create();
    int rc = wubuhyperlink_set(h, 1001, "https://example.com", "Example", "sec2");
    const wubuhyperlink_entry *e = wubuhyperlink_get(h, 1001);
    size_t n = wubuhyperlink_count(h);
    char target[96] = "(null)";
    if (e && e->target) { strncpy(target, e->target, sizeof target - 1); target[sizeof target - 1] = 0; }
    wubuhyperlink_destroy(h);
    char bf[160];
    snprintf(bf, sizeof bf, "hyperlink ok: rc=%d links=%zu target=%s", rc, n, target);
    return strdup(bf);
}

/* thesaurus */
char *doccmd_thesaurus_demo(void){
    wubuthesaurus *t = wubuthesaurus_create();
    const char *happy[] = {"glad","joyful","cheerful",NULL};
    int rc = wubuthesaurus_add(t,"happy",happy);
    const char **s = wubuthesaurus_lookup(t,"happy");
    size_t n = wubuthesaurus_count(t);
    char first[48] = "(null)";
    if (s && s[0]) { strncpy(first, s[0], sizeof first - 1); first[sizeof first - 1] = 0; }
    wubuthesaurus_destroy(t);
    char bf[128];
    snprintf(bf, sizeof bf, "thesaurus ok: rc=%d entries=%zu first=%s", rc, n, first);
    return strdup(bf);
}

/* grammar check */
char *doccmd_grammar_demo(void){
    wubugrammar_finding f[16];
    int n = wubugrammar_check("this is is alot of fun", f, 16);
    char bf[128];
    snprintf(bf, sizeof bf, "grammar ok: findings=%d first_id=%d", n, n?f[0].issue_id:-1);
    return strdup(bf);
}

/* index */
char *doccmd_index_demo(void){
    wubuindex *ix = wubuindex_create();
    wubuindex_add_term(ix,"Apple");
    wubuindex_add_term(ix,"Banana");
    int rc = wubuindex_feed_page(ix,"The Apple falls",1);
    const wubuindex_entry *e = wubuindex_get(ix,0);
    size_t n = wubuindex_count(ix);
    char term[48] = "(null)"; size_t pages = 0;
    if (e) { strncpy(term, e->term, sizeof term - 1); term[sizeof term - 1] = 0; pages = e->npages; }
    wubuindex_destroy(ix);
    char bf[128];
    snprintf(bf, sizeof bf, "index ok: rc=%d entries=%zu first=%s pages=%zu", rc, n, term, pages);
    return strdup(bf);
}

/* mail merge */
char *doccmd_mailmerge_demo(void){
    wubumailmerge *m = wubumailmerge_create();
    const wubumailmerge_field rec[] = {{"Name","Alice"},{"City","Boston"},{"Amount","100"},{NULL,NULL}};
    int rc = wubumailmerge_add_record(m, rec);
    char *out = wubumailmerge_merge(m, 0, "Dear ${Name} of {City}.");
    size_t n = wubumailmerge_record_count(m);
    wubumailmerge_destroy(m);
    char bf[160];
    snprintf(bf, sizeof bf, "mailmerge ok: rc=%d records=%zu merged=%s", rc, n, out?out:"(null)");
    free(out);
    return strdup(bf);
}

/* diff */
char *doccmd_diff_demo(void){
    int n = wubudiff_count("a\nb\nc\n","a\nc\n");
    char bf[96];
    snprintf(bf, sizeof bf, "diff ok: hunks=%d", n);
    return strdup(bf);
}

/* master doc */
char *doccmd_masterdoc_demo(void){
    wubumasterdoc *m = wubumasterdoc_create();
    int rc = wubumasterdoc_add(m,"ch1.docx");
    wubumasterdoc_add(m,"ch2.docx");
    const char *first = wubumasterdoc_get(m,0);
    size_t n = wubumasterdoc_count(m);
    char firstbuf[48] = "(null)";
    if (first) { strncpy(firstbuf, first, sizeof firstbuf - 1); firstbuf[sizeof firstbuf - 1] = 0; }
    wubumasterdoc_destroy(m);
    char bf[128];
    snprintf(bf, sizeof bf, "masterdoc ok: rc=%d subs=%zu first=%s", rc, n, firstbuf);
    return strdup(bf);
}

/* doccmd_dropcap_demo -- first-letter drop cap. src/wubudropcap */
char *doccmd_dropcap_demo(void){
    wubudropcap d;
    int rc = wubudropcap_init(&d, 3);
    int lines = wubudropcap_lines(&d);
    char bf[96];
    snprintf(bf, sizeof bf, "dropcap ok: rc=%d lines=%d", rc, lines);
    return strdup(bf);
}

/* doccmd_ruler_demo -- page ruler margins. src/wuburuler */
char *doccmd_ruler_demo(void){
    wuburuler r;
    int rc = wuburuler_init(&r, 612, 792);
    double w=0, h=0;
    wuburuler_content(&r, &w, &h);
    char bf[128];
    snprintf(bf, sizeof bf, "ruler ok: rc=%d content=%.0fx%.0f", rc, w, h);
    return strdup(bf);
}

/* doccmd_gridline_demo -- gridline toggle. src/wubugridline */
char *doccmd_gridline_demo(void){
    wubugridline g;
    int rc = wubugridline_init(&g);
    wubugridline_toggle(&g);
    char bf[96];
    snprintf(bf, sizeof bf, "gridline ok: rc=%d show=%d", rc, g.show);
    return strdup(bf);
}

/* doccmd_icon_demo -- icon registry. src/wubuicon */
char *doccmd_icon_demo(void){
    wubuicon *i = wubuicon_create();
    int rc = wubuicon_add(i,"save","M5 5h14v14H5z");
    size_t n = wubuicon_count(i);
    wubuicon_destroy(i);
    char bf[96];
    snprintf(bf, sizeof bf, "icon ok: rc=%d icons=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_gallery_demo -- gallery collections. src/wubugallery */
char *doccmd_gallery_demo(void){
    wubugallery *g = wubugallery_create();
    int rc = wubugallery_add_item(g,"Clipart","star.png");
    wubugallery_add_item(g,"Clipart","heart.png");
    size_t n = wubugallery_count(g);
    wubugallery_destroy(g);
    char bf[96];
    snprintf(bf, sizeof bf, "gallery ok: rc=%d galleries=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_sidebar_demo -- sidebar panels. src/wubusidebar */
char *doccmd_sidebar_demo(void){
    wubusidebar *s = wubusidebar_create();
    int rc = wubusidebar_add_panel(s,"Styles");
    wubusidebar_add_panel(s,"Navigator");
    size_t n = wubusidebar_count(s);
    wubusidebar_destroy(s);
    char bf[96];
    snprintf(bf, sizeof bf, "sidebar ok: rc=%d panels=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_transition_demo -- slide transition. src/wubutransition */
char *doccmd_transition_demo(void){
    wubutransition t;
    int rc = wubutransition_set(&t, WUBU_TR_FADE, 0.5, 1, 2.0);
    char bf[96];
    snprintf(bf, sizeof bf, "transition ok: rc=%d type=%d", rc, (int)t.type);
    return strdup(bf);
}

/* doccmd_animation_demo -- keyframe animation. src/wubuanimation */
char *doccmd_animation_demo(void){
    wubuanimation *a = wubuanimation_create();
    int rc = wubuanimation_add(a,"title",WUBU_AN_FADE,1.0,0.0,0);
    size_t n = wubuanimation_count(a);
    wubuanimation_destroy(a);
    char bf[96];
    snprintf(bf, sizeof bf, "animation ok: rc=%d keys=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_masterslide_demo -- master slide theme. src/wubumasterslide */
char *doccmd_masterslide_demo(void){
    wubumasterslide *m = wubumasterslide_create();
    int rc = wubumasterslide_set_theme(m,"1F3B8C",24.0);
    double fs = wubumasterslide_fontsize(m);
    wubumasterslide_destroy(m);
    char bf[96];
    snprintf(bf, sizeof bf, "masterslide ok: rc=%d font=%.0f", rc, fs);
    return strdup(bf);
}

/* doccmd_connector_demo -- diagram connector. src/wubuconnector */
char *doccmd_connector_demo(void){
    wubuconnector *c = wubuconnector_create();
    int rc = wubuconnector_add(c,"A","out","B","in");
    size_t n = wubuconnector_count(c);
    wubuconnector_destroy(c);
    char bf[96];
    snprintf(bf, sizeof bf, "connector ok: rc=%d edges=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_encrypt_demo -- password document encryption. src/wubuencrypt */
char *doccmd_encrypt_demo(void){
    const char *msg = "Classified WuBuOffice doc";
    size_t elen=0, dlen=0;
    unsigned char *enc = wubuencrypt_document("secret", (const unsigned char*)msg, strlen(msg), &elen);
    unsigned char *dec = enc ? wubuencrypt_open("secret", enc, elen, &dlen) : NULL;
    int ok = (dec && dlen == strlen(msg) && memcmp(dec, msg, dlen)==0) ? 1 : 0;
    free(enc); free(dec);
    char bf[96];
    snprintf(bf, sizeof bf, "encrypt ok: rc=%d roundtrip=%d", ok, ok);
    return strdup(bf);
}

/* doccmd_mailexport_demo -- RFC-5322 mail render. src/wubumailexport */
char *doccmd_mailexport_demo(void){
    wubumailexport m = {0};
    int rc = wubumailexport_build(&m,"b@x.com","a@y.com","Re","body");
    char *rendered = wubumailexport_render(&m);
    size_t rlen = rendered ? strlen(rendered) : 0;
    free(rendered);
    wubumailexport_free(&m);
    char bf[96];
    snprintf(bf, sizeof bf, "mailexport ok: rc=%d len=%zu", rc, rlen);
    return strdup(bf);
}

/* doccmd_notebookbar_demo -- sheet tab strip. src/wubunotebookbar */
char *doccmd_notebookbar_demo(void){
    wubunotebookbar *n = wubunotebookbar_create();
    int rc = wubunotebookbar_add(n,"Sheet1");
    wubunotebookbar_add(n,"Sheet2");
    size_t c = wubunotebookbar_count(n);
    wubunotebookbar_destroy(n);
    char bf[96];
    snprintf(bf, sizeof bf, "notebookbar ok: rc=%d tabs=%zu", rc, c);
    return strdup(bf);
}

/* doccmd_qr_demo -- QR render. src/wubuqr */
char *doccmd_qr_demo(void){
    int size = 0;
    char *qr = wubuqr_render_ascii("WUBU", &size);
    int ok = qr ? 1 : 0;
    free(qr);
    char bf[96];
    snprintf(bf, sizeof bf, "qr ok: ok=%d size=%d", ok, size);
    return strdup(bf);
}

/* doccmd_smartart_demo -- diagram layout. src/wubusmartart */
char *doccmd_smartart_demo(void){
    wubusmartart *s = wubusmartart_create();
    int rc = wubusmartart_set_layout(s, WUBU_SA_CYCLE);
    wubusmartart_add_node(s,"Plan");
    size_t n = wubusmartart_count(s);
    wubusmartart_destroy(s);
    char bf[96];
    snprintf(bf, sizeof bf, "smartart ok: rc=%d nodes=%zu", rc, n);
    return strdup(bf);
}

/* doccmd_basic_demo -- minimal BASIC macro engine. src/wububasic */
char *doccmd_basic_demo(void){
    wububasic *b = wububasic_create();
    static char out[256]; out[0]=0;
    wububasic_set_output(b, NULL, NULL); /* stdout default; use a capture fn below */
    (void)out;
    int rc = wububasic_load(b, "s = 0\nFOR i = 1 TO 4\n s = s + i\nNEXT\nPRINT s\nEND\n");
    if (rc == 0) rc = wububasic_run(b);
    const char *val = wububasic_get_var(b, "s");
    char bf[96];
    snprintf(bf, sizeof bf, "basic ok: rc=%d s=%s", rc, val ? val : "?");
    wububasic_destroy(b);
    return strdup(bf);
}

/* doccmd_3d_demo -- 3D mesh model. src/wubu3d */
char *doccmd_3d_demo(void){
    wubu3d *m = wubu3d_create();
    int rc = wubu3d_make_cube(m);
    size_t v = wubu3d_vertex_count(m), f = wubu3d_face_count(m);
    wubu3d_destroy(m);
    char bf[96];
    snprintf(bf, sizeof bf, "3d ok: rc=%d verts=%zu faces=%zu", rc, v, f);
    return strdup(bf);
}


