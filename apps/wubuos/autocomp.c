/* autocomp.c -- opaque auto-completion engine (see autocomp.h).
 * Ported out of view_editor.c so the editor no longer owns the candidate list.
 */
#include "autocomp.h"

#include "doc.h"   /* WuBuPad piece-table Doc */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *AC_BUILTIN[] = {
    "int","char","float","double","void","long","short","unsigned","signed",
    "if","else","for","while","do","switch","case","break","continue","return",
    "struct","union","enum","typedef","static","const","sizeof","printf","malloc",
    "NULL","true","false","include","define","strlen","strcpy","strcat",NULL };

struct AutoComp {
    int   mode;              /* popup open */
    char  list[64][32];      /* candidate words */
    int   n;                 /* candidate count */
    int   sel;               /* selected index */
    char  prefix[32];        /* word being completed */
};

AutoComp *autocomp_create(void){ return calloc(1, sizeof(AutoComp)); }
void autocomp_destroy(AutoComp *ac){ free(ac); }

int  autocomp_opened(const AutoComp *ac){ return ac ? ac->mode : 0; }
void autocomp_close(AutoComp *ac){ if (ac) ac->mode = 0; }
int  autocomp_count(const AutoComp *ac){ return ac ? ac->n : 0; }
int  autocomp_selected(const AutoComp *ac){ return ac ? ac->sel : 0; }
const char *autocomp_candidate(const AutoComp *ac, int i){
    if (!ac || i < 0 || i >= ac->n) return "";
    return ac->list[i];
}
void autocomp_move(AutoComp *ac, int dir){
    if (!ac || !ac->n) return;
    ac->sel += dir;
    if (ac->sel < 0) ac->sel = 0;
    if (ac->sel >= ac->n) ac->sel = ac->n - 1;
}

/* membership test + insert if it matches the prefix and is new */
static void ac_try_add(AutoComp *ac, const char *buf, size_t len){
    if (!ac || len==0 || len>=32) return;
    size_t pl = strlen(ac->prefix);
    if (pl && strncmp(buf, ac->prefix, pl)!=0) return;   /* must start with prefix */
    if (strcmp(buf, ac->prefix)==0) return;              /* skip exact prefix */
    for (int i=0;i<ac->n;i++) if (!strcmp(ac->list[i], buf)) return;
    if (ac->n < 64){ size_t c = len<31? len:31; memcpy(ac->list[ac->n], buf, c); ac->list[ac->n][c]=0; ac->n++; }
}

int autocomp_open(AutoComp *ac, const void *doc){
    if (!ac || !doc) return 0;
    char *t = doc_text((const Doc*)doc);
    size_t cur = doc_cursor((const Doc*)doc);
    int s = (int)cur; while (s>0 && (isalnum((unsigned char)t[s-1])||t[s-1]=='_')) s--;
    int plen = (int)cur - s;
    if (plen >= (int)sizeof ac->prefix) plen = (int)sizeof ac->prefix - 1;
    memcpy(ac->prefix, t+s, plen); ac->prefix[plen]=0;
    free(t);
    ac->n = 0; ac->sel = 0;
    for (int i=0; AC_BUILTIN[i]; i++){
        const char *b = AC_BUILTIN[i]; size_t bl=strlen(b);
        size_t c=bl<31?bl:31; char buf[32]; memcpy(buf, b, c); buf[c]=0;
        ac_try_add(ac, buf, c);
    }
    char *d = doc_text((const Doc*)doc);
    size_t n = doc_length((const Doc*)doc), q=0;
    while (q<n){
        if (isalpha((unsigned char)d[q]) || d[q]=='_'){
            size_t e2=q; while (e2<n && (isalnum((unsigned char)d[e2])||d[e2]=='_')) e2++;
            char buf[32]; size_t L=e2-q; if (L>31) L=31;
            memcpy(buf, d+q, L); buf[L]=0; ac_try_add(ac, buf, L); q=e2;
        } else q++;
    }
    free(d);
    ac->mode = (ac->n>0);
    return ac->mode;
}

int autocomp_accept(AutoComp *ac, void *doc){
    if (!ac || !doc || !ac->mode || ac->sel<0 || ac->sel>=ac->n) { if (ac) ac->mode=0; return 0; }
    char *t = doc_text((const Doc*)doc);
    size_t cur = doc_cursor((const Doc*)doc);
    int s = (int)cur; while (s>0 && (isalnum((unsigned char)t[s-1])||t[s-1]=='_')) s--;
    free(t);
    int plen = (int)cur - s;
    if (plen>0) doc_delete((Doc*)doc, s, plen);
    doc_type((Doc*)doc, ac->list[ac->sel], strlen(ac->list[ac->sel]));
    ac->mode = 0;
    return 1;
}
