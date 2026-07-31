/* test_a11ytree.c -- needs wubumodel to build a doc. */
#include "a11ytree.h"
#include <stdlib.h>
#include "model.h"
#include <stdio.h>
#include <string.h>

static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)

int main(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *s1 = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *p  = wubumodel_node_create(d, WUBUMODEL_PARAGRAPH);
    wubumodel_node *im = wubumodel_node_create(d, WUBUMODEL_IMAGE);
    wubumodel_node_set_note(im, "a diagram");   /* alt text */
    wubumodel_node_append(d, s1, p);
    wubumodel_node_append(d, s1, im);
    wubumodel_node_append(d, NULL, s1);   /* NULL parent -> becomes doc root */

    char *t = a11ytree_build(d);
    CK(t != NULL, "build");
    if (t){
        CK(strstr(t, "HEADING:1")!=NULL, "heading role");
        CK(strstr(t, "PARAGRAPH:")!=NULL, "paragraph role");
        CK(strstr(t, "IMAGE: a diagram")!=NULL, "image alt");
        CK(a11ytree_count(d)==3, "count");
        free(t);
    }
    wubumodel_doc_destroy(d);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: a11ytree (section/paragraph/image roles + alt)\n");
    return 0;
}
