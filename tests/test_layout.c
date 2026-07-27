/* test_layout.c -- central pipeline check (model -> pages -> runs). */
#include "ublayout.h"
#include "model.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ printf("FAIL: %s\n", m); fails++; } }while(0)

/* metric: 7px per char, height = font_size+4 */
static int meas(const char *t, size_t len, int fs, int b, int i, int *h, void *u){
    (void)b;(void)i;(void)u;
    int w=0; for(size_t k=0;k<len;k++) w += (t[k]==' '? fs*0.3 : fs*0.55);
    *h = fs+4; return w;
}
static int sty(void *u, void *run, int *fs, int *b, int *it, wubulayout_dir *d){
    (void)u;(void)run;(void)b;(void)it;(void)d;
    *fs=12; *b=0; *it=0; *d=WUBULAYOUT_LTR; return 1;
}

static wubumodel_node *add_para(wubumodel_doc *doc, wubumodel_node *parent, const char *txt){
    wubumodel_node *p = wubumodel_node_create(doc, WUBUMODEL_PARAGRAPH);
    wubumodel_node *r = wubumodel_node_create(doc, WUBUMODEL_RUN);
    wubumodel_run_set_text(r, txt);
    wubumodel_node_append(doc, p, r);
    wubumodel_node_append(doc, parent, p);
    return p;
}

int main(void){
    wubumodel_doc *doc = wubumodel_doc_create();
    wubumodel_node *sec = wubumodel_node_create(doc, WUBUMODEL_SECTION);
    /* ~40 words of lorem to force multiple lines + hopefully a 2nd page */
    const char *lorem =
      "lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod "
      "tempor incididunt ut labore et dolore magna aliqua ut enim ad minim "
      "veniam quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
      "commodo consequat duis aute irure dolor in reprehenderit voluptate velit "
      "esse cillum dolore eu fugiat nulla pariatur excepteur sint occaecat "
      "cupidatat non proident sunt in culpa qui officia deserunt mollit anim id ";
    for (int i=0;i<30;i++) add_para(doc, sec, lorem);

    wubulayout_doc *L = wubulayout_create(doc, sec,
        meas, sty, NULL, 794, 1123, 72,72,72,72);
    CK(L != NULL, "layout created");
    int pages = wubulayout_page_count(L);
    CK(pages >= 2, "multiple pages produced");
    int tot = wubulayout_total_runs(L);
    CK(tot > 0, "runs produced");
    /* runs on page 0 have ascending x within a line and positive baseline */
    int pr0 = wubulayout_run_count(L, 0);
    CK(pr0 > 0, "page 0 has runs");
    int prev_line=-1, prev_x=-1, monotonic=1;
    for (int i=0;i<pr0;i++){
        const wubulayout_run *r=wubulayout_run_at(L,0,i);
        if (r->line != prev_line){ prev_line=r->line; prev_x=-1; }
        else if (r->rtl==0 && r->x < prev_x) monotonic=0;
        prev_x=r->x;
        CK(r->y > 0, "baseline positive");
    }
    CK(monotonic, "LTR runs left-to-right within a line");
    /* reading-order text survives a round trip */
    char *dt = wubulayout_doc_text(L);
    CK(dt && strlen(dt) > 200, "doc text produced");
    free(dt);
    /* hit test: find a run and hit it */
    const wubulayout_run *r0 = wubulayout_run_at(L,0,0);
    int line=-1; int hit = wubulayout_hit_test(L,0, r0->x+2, r0->y-4, &line);
    CK(hit >= 0, "hit test returns a run");

    wubulayout_destroy(L);
    wubumodel_doc_destroy(doc);
    if (fails==0) printf("LAYOUT TESTS PASSED\n");
    else printf("LAYOUT TESTS FAILED (%d)\n", fails);
    return fails? 1 : 0;
}
