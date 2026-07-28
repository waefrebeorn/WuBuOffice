/* test_aislot.c */
#include "aislot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)

static int myprov(void *user, const char *task, const char *prompt, char *out, size_t cap){
    (void)user; (void)prompt;
    if (strcmp(task,"summarize")==0){ snprintf(out,cap,"CUSTOM"); return 0; }
    return 1;
}

int main(void){
    AiSlot *s = aislot_create();
    CK(aislot_has_custom_provider(s)==0,"no provider default");
    /* built-in fallback: summarize takes first sentences */
    char *r = aislot_run(s, "summarize", "First sentence. More text here.\n\nSecond para starts. tail.");
    CK(r && strstr(r,"First sentence.")!=NULL,"fallback summarize");
    free(r);
    char *c = aislot_run(s, "complete", "line one\npartial thought");
    CK(c && strstr(c,"partial thought")!=NULL,"fallback complete");
    free(c);
    CK(aislot_run(s, "unknown-task", "x")==NULL,"unknown task NULL");
    /* custom provider replaces fallback */
    aislot_set_provider(s, myprov, NULL);
    CK(aislot_has_custom_provider(s)==1,"provider set");
    char *m = aislot_run(s, "summarize", "whatever");
    CK(m && strcmp(m,"CUSTOM")==0,"custom provider used");
    free(m);
    CK(aislot_run(s, "other", "x")==NULL,"provider failure NULL");
    /* restore fallback */
    aislot_set_provider(s, NULL, NULL);
    CK(aislot_has_custom_provider(s)==0,"fallback restored");
    aislot_destroy(s);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: aislot (fallback + provider swap)\n"); return 0;
}
