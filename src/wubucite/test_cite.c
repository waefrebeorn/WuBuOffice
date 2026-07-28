/* test_cite.c */
#include "cite.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Cite *c = cite_create();
    CK(cite_add(c, "smith2020", "article", "On Things", "Alice Smith", 2020)==1, "add");
    CK(cite_add(c, "doe2021", "book", "More Things", "Bob Doe", 2021)==1, "add2");
    char *in = cite_inline(c, "smith2020");
    CK(in && strcmp(in, "(Smith, 2020)")==0, "inline");
    free(in);
    CK(cite_inline(c, "missing")==NULL, "unknown inline null");
    char *bib = cite_bibliography(c);
    CK(bib && strstr(bib, "Alice Smith") && strstr(bib, "On Things"), "bibliography");
    free(bib);
    CK(cite_count(c)==2, "count");
    cite_destroy(c);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: cite (add/inline/bibliography)\n");
    return 0;
}
