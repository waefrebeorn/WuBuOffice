/* test_eqnum.c -- needs wubumodel to build a doc with FIELD (equation) nodes. */
#include "eqnum.h"
#include "model.h"
#include <stdio.h>
#include <string.h>

static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)

int main(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *sec = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *f1  = wubumodel_node_create(d, WUBUMODEL_FIELD);
    wubumodel_node *f2  = wubumodel_node_create(d, WUBUMODEL_FIELD);
    wubumodel_node_append(d, sec, f1);
    wubumodel_node_append(d, sec, f2);
    wubumodel_node_append(d, NULL, sec);

    EqNum *e = eqnum_create();
    int n = eqnum_scan(e, d);
    CK(n==2, "scanned 2 equations");
    CK(eqnum_count(e)==2, "count");
    const char *l1 = eqnum_label(e, wubumodel_node_id(f1));
    const char *l2 = eqnum_label(e, wubumodel_node_id(f2));
    CK(l1 && strcmp(l1,"(1)")==0, "first label");
    CK(l2 && strcmp(l2,"(2)")==0, "second label");
    CK(eqnum_label(e, 999999)==NULL, "unknown id null");
    eqnum_destroy(e);
    wubumodel_doc_destroy(d);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: eqnum (sequential equation numbering)\n");
    return 0;
}
