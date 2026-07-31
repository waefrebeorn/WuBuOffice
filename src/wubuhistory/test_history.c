/* test_history.c -- headless version-history test. */
#include "history.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } } while(0)

int main(void){
    History *h = history_create();
    const char *v1 = "line1\nline2\nline3\n";
    const char *v2 = "line1\nlineTWO\nline3\nline4\n";
    int id1 = history_commit(h, v1, strlen(v1), "initial", "alice");
    int id2 = history_commit(h, v2, strlen(v2), "edit", "bob");
    CHECK(id1==1 && id2==2, "ids");
    CHECK(history_count(h)==2, "count");
    CHECK(strcmp(history_author(h,id2),"bob")==0, "author");
    CHECK(strcmp(history_label(h,id2),"edit")==0, "label");
    size_t len; const char *b = history_blob(h, id1, &len);
    CHECK(b && len==strlen(v1) && memcmp(b,v1,len)==0, "blob");
    char *d = history_diff(h, id1, id2);
    CHECK(d != NULL, "diff non-null");
    if (d){
        CHECK(strstr(d, "-line2")!=NULL, "diff has removed line2");
        CHECK(strstr(d, "+lineTWO")!=NULL, "diff has added lineTWO");
        CHECK(strstr(d, "+line4")!=NULL, "diff has added line4");
        free(d);
    }
    CHECK(history_diff(h, id1, id1)==NULL, "equal diff null");
    history_destroy(h);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: history (commit/count/author/label/blob/diff)\n");
    return 0;
}
