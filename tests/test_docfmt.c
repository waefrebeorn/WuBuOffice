/* test_docfmt.c -- unit tests for the docmodel -> alternate-format serializers. */
#include "docfmt.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* a tiny docmodel: one paragraph + one 2x2 table */
static const char *SAMPLE =
"{\"blocks\":["
"{\"kind\":\"paragraph\",\"text\":\"Hello world\",\"conf\":92},"
"{\"kind\":\"table\",\"rows\":2,\"cols\":2,\"cells\":[[\"a\",\"b\"],[\"c\",\"d\"]],"
"\"conf\":[[88,90],[85,87]]}"
"]}";

int main(void){
    int fail=0;
    char *t=docfmt_to_text(SAMPLE);
    if(!t || !strstr(t,"Hello world") || !strstr(t,"a\tb") || !strstr(t,"c\td")){
        printf("FAIL docfmt_to_text\n"); fail=1; }
    free(t);

    char *ts=docfmt_to_tsv(SAMPLE);
    if(!ts || !strstr(ts,"a\tb") || !strstr(ts,"c\td")){ printf("FAIL docfmt_to_tsv\n"); fail=1; }
    free(ts);

    char *cs=docfmt_to_csv(SAMPLE);
    if(!cs || !strstr(cs,"a,b") || !strstr(cs,"c,d")){ printf("FAIL docfmt_to_csv\n"); fail=1; }
    free(cs);

    char *jl=docfmt_to_jsonl(SAMPLE);
    if(!jl){ printf("FAIL docfmt_to_jsonl null\n"); fail=1; }
    else {
        /* two blocks -> two newline-terminated lines, each a JSON object */
        int nl=0, objs=0; for(char *p=jl;*p;p++){ if(*p=='\n') nl++; if(*p=='{') objs++; }
        if(nl!=2 || objs!=2){ printf("FAIL jsonl format (nl=%d objs=%d)\n",nl,objs); fail=1; }
        if(!strstr(jl,"\"kind\":\"paragraph\"") || !strstr(jl,"\"kind\":\"table\"")){ printf("FAIL jsonl kinds\n"); fail=1; }
    }
    free(jl);

    char *lx=docfmt_to_latex(SAMPLE);
    if(!lx || !strstr(lx,"tabular") || !strstr(lx,"Hello world")){ printf("FAIL docfmt_to_latex\n"); fail=1; }
    free(lx);

    char *rt=docfmt_to_rtf(SAMPLE);
    if(!rt || !strstr(rt,"Hello world") || !strstr(rt,"\\rtf1")){ printf("FAIL docfmt_to_rtf\n"); fail=1; }
    free(rt);

    char *ho=docfmt_to_hocr(SAMPLE);
    if(!ho || !strstr(ho,"ocr_page") || !strstr(ho,"x_wconf")){ printf("FAIL docfmt_to_hocr\n"); fail=1; }
    free(ho);

    char *al=docfmt_to_alto(SAMPLE);
    if(!al || !strstr(al,"<alto") || !strstr(al,"WC=")){ printf("FAIL docfmt_to_alto\n"); fail=1; }
    free(al);

    if(fail){ printf("FAIL test_docfmt\n"); return 1; }
    printf("PASS test_docfmt\n");
    return 0;
}
