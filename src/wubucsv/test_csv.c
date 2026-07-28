/* test_csv.c */
#include "csv.h"
#include <stdio.h>
#include <string.h>
static int fails=0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[%s]\n",(m)); fails++; } }while(0)
int main(void){
    Csv *c = csv_create();
    const char *doc = "a,b,\"c,d\"\n\"line1\nline2\",e,f";
    CK(csv_parse(c, doc)==1,"parse");
    CK(csv_rows(c)==2,"rows");
    CK(csv_cols(c)==3,"cols");
    CK(strcmp(csv_cell(c,0,0),"a")==0,"c00");
    CK(strcmp(csv_cell(c,0,1),"b")==0,"c01");
    CK(strcmp(csv_cell(c,0,2),"c,d")==0,"quoted comma");
    CK(strcmp(csv_cell(c,1,0),"line1\nline2")==0,"quoted newline");
    CK(strcmp(csv_cell(c,1,1),"e")==0,"c11");
    csv_destroy(c);
    if(fails){ printf("FAILED (%d)\n",fails); return 1; }
    printf("PASS: csv (quoted comma/newline parsing)\n"); return 0;
}
