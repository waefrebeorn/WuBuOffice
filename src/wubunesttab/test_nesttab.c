/* test_nesttab.c -- nested tables over real wubumodel. */
#include "nesttab.h"
#include "model.h"
#include <stdio.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node_append(d, NULL, sec);

    /* 2x2 outer table */
    void *t = nesttab_build(d, sec, 2, 2);
    CK(t != NULL, "build outer");
    CK(nesttab_validate(t)==1, "outer valid");
    CK(nesttab_depth(t)==1, "depth 1");

    /* nest a 1x2 table inside cell (0,1) */
    void *c01 = nesttab_cell(t, 0, 1, 2);
    CK(c01 != NULL, "cell 0,1");
    void *inner = nesttab_nest(d, c01, 1, 2);
    CK(inner != NULL, "nest inner");
    CK(nesttab_depth(t)==2, "depth 2 after nest");
    CK(nesttab_validate(t)==1, "still valid");

    /* nest one more level: depth 3 */
    void *ic0 = nesttab_cell(inner, 0, 0, 2);
    CK(nesttab_nest(d, ic0, 1, 1) != NULL, "nest level 3");
    CK(nesttab_depth(t)==3, "depth 3");

    /* nesting into a non-cell is rejected */
    CK(nesttab_nest(d, sec, 1, 1)==NULL, "non-cell rejected");
    /* out-of-range cell */
    CK(nesttab_cell(t, 5, 0, 2)==NULL, "row OOB");
    CK(nesttab_cell(t, 0, 2, 2)==NULL, "col OOB");
    wubumodel_doc_destroy(d);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: nesttab (build/nest depth 3/validate)\n"); return 0;
}
