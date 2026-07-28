/* csv.c -- CSV parser. See csv.h. */
#include "csv.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CSV_MAXR 1024
#define CSV_MAXC 64
#define CSV_CELLSZ 256

struct Csv {
    char cells[CSV_MAXR][CSV_MAXC][CSV_CELLSZ];
    int rows, cols;
};

Csv *csv_create(void){ return calloc(1, sizeof(Csv)); }
void csv_destroy(Csv *c){ free(c); }

int csv_parse(Csv *c, const char *t){
    if (!c || !t) return 0;
    int r=0, col=0; char *cur = c->cells[r][col];
    const char *p = t;
    int inq = 0;
    while (*p && r < CSV_MAXR){
        if (inq){
            if (*p=='"'){
                if (*(p+1)=='"'){ *cur++='"'; p+=2; continue; }
                inq=0; p++; continue;
            }
            if (*p=='\n'){ *cur++='\n'; p++; continue; }
            *cur++ = *p++; continue;
        } else {
            if (*p=='"'){ inq=1; p++; continue; }
            if (*p==','){ *cur=0; col++; if(col>=CSV_MAXC){col=CSV_MAXC-1;} cur=c->cells[r][col]; p++; continue; }
            if (*p=='\n'){ *cur=0; if(col+1>c->cols) c->cols=col+1; col=0; r++; cur=c->cells[r][0]; p++; continue; }
            if (*p=='\r'){ p++; continue; }
            *cur++ = *p++;
        }
    }
    *cur=0;
    if (col+1 > c->cols) c->cols = col+1;
    c->rows = r + (c->cols>0?1:0);
    if (c->rows > CSV_MAXR) c->rows = CSV_MAXR;
    return 1;
}

int csv_rows(const Csv *c){ return c? c->rows : 0; }
int csv_cols(const Csv *c){ return c? c->cols : 0; }
const char *csv_cell(const Csv *c, int row, int col){
    if (!c || row<0 || row>=c->rows || col<0 || col>=c->cols) return "";
    return c->cells[row][col];
}
