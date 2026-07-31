/* pasteplain.c -- paste-plain stripper. See pasteplain.h. */
#include "pasteplain.h"

#include <stdlib.h>
#include <string.h>

char *pasteplain_strip(const char *in){
    if (!in) return NULL;
    size_t cap=strlen(in)+1, len=0; char *out=malloc(cap);
    if (!out) return NULL;
    const char *p = in;
    int intag = 0;       /* inside <...> */
    while (*p){
        if (intag){
            if (*p=='>') intag=0;
            p++; continue;
        }
        if (*p=='<'){ intag=1; p++; continue; }
        if (*p=='\\'){
            /* skip the control word: \xxx or \'xx */
            p++;
            while (*p && ((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z'))) p++;
            if (*p==' ') p++;       /* consume one space delimiter */
            continue;
        }
        while (len+2>cap){ cap*=2; char *no=realloc(out,cap); if(!no){free(out);return NULL;} out=no; }
        out[len++]=*p++;
    }
    out[len]=0;
    return out;
}
