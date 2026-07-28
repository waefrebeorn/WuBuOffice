/* test_heading.c -- needs wubumodel: nested sections get sequential levels. */
#include "heading.h"
#include "model.h"
#include <stdio.h>
#include <string.h>

static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)

int main(void){
    wubumodel_doc *d = wubumodel_doc_create();
    wubumodel_node *s1 = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *s2 = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node *s3 = wubumodel_node_create(d, WUBUMODEL_SECTION);
    wubumodel_node_append(d, NULL, s1);
    wubumodel_node_append(d, s1, s2);
    wubumodel_node_append(d, s1, s3);

    Heading *h = heading_create();
    int n = heading_enforce(h, d);
    CK(n==3, "3 headings");
    CK(heading_level(h, wubumodel_node_id(s1))==1, "s1 level 1");
    CK(heading_level(h, wubumodel_node_id(s2))==2, "s2 level 2");
    CK(heading_level(h, wubumodel_node_id(s3))==3, "s3 level 3 (no skip)");
    CK(heading_count(h)==3, "count");
    heading_destroy(h);
    wubumodel_doc_destroy(d);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: heading (sequential level enforcement, no skip)\n");
    return 0;
}
