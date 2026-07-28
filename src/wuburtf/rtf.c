/* rtf.c -- RTF writer. See rtf.h. */
#include "rtf.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *rtf_write(const RtfRun *runs, int n){
    size_t cap=128, len=0; char *out=malloc(cap);
    if (!out) return NULL;
    out[0]=0;
    const char *head = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fmodern\\fcharset0 Courier;}}{\\f1\\froman\\fcharset0 Times;}}";
    size_t need=strlen(head);
    while (len+need+1>cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
    memcpy(out, head, need); len+=need;

    for (int i=0;i<n;i++){
        const RtfRun *r = &runs[i];
        int font = r->mono ? 0 : 1;
        char pre[64];
        int pl = sprintf(pre, "\\f%d%s%s ", font, r->bold?"\\b":"", r->italic?"\\i":"");
        while (len+pl+1>cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
        memcpy(out+len, pre, pl); len+=pl;
        /* escape RTF specials */
        const char *t = r->text? r->text : "";
        while (*t){
            char esc[8]; int el=0;
            if (*t=='\\'||*t=='{'||*t=='}'){ esc[el++]='\\'; esc[el++]=*t; }
            else if (*t=='\n'){ esc[el++]='\\'; esc[el++]='p'; esc[el++]='a'; esc[el++]='r'; }
            else esc[el++]=*t;
            while (len+el+1>cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
            memcpy(out+len, esc, el); len+=el;
            t++;
        }
    }
    const char *tail = "}";
    while (len+2>cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
    memcpy(out+len, tail, 1); len+=1; out[len]=0;
    return out;
}
