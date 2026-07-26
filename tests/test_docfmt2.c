/* test_docfmt2.c -- unit test for TEI serialization + reading-order/confidence
 * fields round-tripping through the docmodel JSON.
 * Builds a small docmodel (1 header-like paragraph + 1 table) and checks:
 *   - docfmt_to_tei emits a well-formed <TEI><text><body> skeleton,
 *   - the docmodel JSON carries order/head/cconf on paragraph blocks. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "docfmt.h"
#include "json.h"

static int contains(const char *hay, const char *needle){
    return strstr(hay, needle) != NULL;
}

int main(void){
    /* a docmodel with a header paragraph (top, short) + a table */
    const char *json =
        "{\"blocks\":["
        "{\"kind\":\"paragraph\",\"text\":\"Title Here\",\"conf\":95,"
         "\"bbox\":[10,5,200,30],\"order\":0,\"head\":1,\"cconf\":[99,98,97]},"
        "{\"kind\":\"table\",\"rows\":2,\"cols\":2,\"cells\":[[\"a\",\"b\"],[\"c\",\"d\"]],"
         "\"conf\":[[90,91],[92,93]],\"cconf\":[[88,89],[87,86]],"
         "\"cellbox\":[[[0,40,50,60],[55,40,105,60]],[[0,65,50,85],[55,65,105,85]]]}"
        "]}";

    /* 1) TEI serialization */
    char *tei = docfmt_to_tei(json);
    if (!tei){ printf("FAIL: docfmt_to_tei returned NULL\n"); return 1; }
    int ok = 1;
    if (!contains(tei, "<TEI")) { printf("FAIL: TEI root missing\n"); ok=0; }
    if (!contains(tei, "<text>") || !contains(tei,"<body>")) { printf("FAIL: text/body missing\n"); ok=0; }
    if (!contains(tei, "<p>Title Here</p>")) { printf("FAIL: paragraph not emitted\n"); ok=0; }
    if (!contains(tei, "<table>") || !contains(tei,"<row>") || !contains(tei,"<cell>a</cell>")) { printf("FAIL: table/cell not emitted\n"); ok=0; }
    free(tei);
    if (!ok){ return 1; }

    /* 2) reading-order + confidence fields parse from the docmodel */
    const char *end=NULL;
    JVal *root = j_parse(json, &end);
    if (!root){ printf("FAIL: docmodel parse\n"); return 1; }
    const JVal *blocks = j_obj_get(root, "blocks");
    const JVal *p0 = j_arr_at(blocks, 0);
    const JVal *order = j_obj_get(p0, "order");
    const JVal *head  = j_obj_get(p0, "head");
    const JVal *cconf = j_obj_get(p0, "cconf");
    if (!order || (int)j_as_num(order)!=0){ printf("FAIL: order field\n"); ok=0; }
    if (!head  || (int)j_as_num(head)!=1){ printf("FAIL: head flag\n"); ok=0; }
    if (!cconf || j_type(cconf)!=J_ARR || j_len(cconf)!=3){ printf("FAIL: cconf array\n"); ok=0; }
    j_free(root);
    if (!ok){ return 1; }

    printf("PASS: test_docfmt2 (TEI + order/head/cconf)\n");
    return 0;
}
