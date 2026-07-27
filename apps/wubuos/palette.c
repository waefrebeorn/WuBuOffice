/* palette.c -- UI-29 command palette logic. Case-insensitive subsequence
 * match ("fzf-lite"): every query char must appear in order in the label.
 * Matches are ranked: prefix match < word-start match < scattered. */
#include "palette.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PAL_MAX 128
#define PAL_QMAX 64

struct Palette {
    char *label[PAL_MAX];
    int   id[PAL_MAX];
    int   n;
    int   open;
    char  q[PAL_QMAX];
    int   qlen;
    int   res[PAL_MAX];   /* indices of filtered results, ranked */
    int   nres;
    int   sel;
};

Palette *palette_create(void){ return calloc(1, sizeof(Palette)); }

void palette_destroy(Palette *p){
    if (!p) return;
    for (int i=0;i<p->n;i++) free(p->label[i]);
    free(p);
}

int palette_add(Palette *p, const char *label, int cmd_id){
    if (!p || !label || p->n >= PAL_MAX) return -1;
    p->label[p->n] = strdup(label);
    p->id[p->n] = cmd_id;
    p->n++;
    return 0;
}

/* score: 0 = no match; 3 = prefix; 2 = word-start; 1 = scattered subsequence */
static int match_score(const char *label, const char *q){
    if (!*q) return 1;
    size_t ll = strlen(label), ql = strlen(q);
    if (ql > ll) return 0;
    /* prefix? */
    int prefix = 1;
    for (size_t i=0;i<ql;i++)
        if (tolower((unsigned char)label[i]) != tolower((unsigned char)q[i])){ prefix=0; break; }
    if (prefix) return 3;
    /* word-start: query matches at the start of any word */
    for (size_t s=1;s<ll;s++){
        if (!isalnum((unsigned char)label[s-1]) && ll-s >= ql){
            int ok=1;
            for (size_t i=0;i<ql;i++)
                if (tolower((unsigned char)label[s+i]) != tolower((unsigned char)q[i])){ ok=0; break; }
            if (ok) return 2;
        }
    }
    /* scattered subsequence */
    size_t qi=0;
    for (size_t i=0;i<ll && qi<ql;i++)
        if (tolower((unsigned char)label[i]) == tolower((unsigned char)q[qi])) qi++;
    return qi==ql ? 1 : 0;
}

static void refilter(Palette *p){
    p->nres = 0;
    /* three passes by score keeps ranking without a sort */
    for (int want=3; want>=1; want--)
        for (int i=0;i<p->n;i++)
            if (match_score(p->label[i], p->q) == want && p->nres < PAL_MAX)
                p->res[p->nres++] = i;
    if (p->sel >= p->nres) p->sel = p->nres ? p->nres-1 : 0;
}

void palette_open(Palette *p){
    if (!p) return;
    p->open = 1; p->qlen = 0; p->q[0] = 0; p->sel = 0;
    refilter(p);
}
void palette_close(Palette *p){ if (p) p->open = 0; }
int  palette_is_open(const Palette *p){ return p ? p->open : 0; }

void palette_input(Palette *p, char c){
    if (!p || !p->open || p->qlen >= PAL_QMAX-1) return;
    if ((unsigned char)c < 32) return;
    p->q[p->qlen++] = c; p->q[p->qlen] = 0;
    p->sel = 0;
    refilter(p);
}
void palette_backspace(Palette *p){
    if (!p || !p->open || p->qlen == 0) return;
    p->q[--p->qlen] = 0;
    p->sel = 0;
    refilter(p);
}
const char *palette_query(const Palette *p){ return p ? p->q : ""; }

void palette_next(Palette *p){ if (p && p->nres) p->sel = (p->sel+1) % p->nres; }
void palette_prev(Palette *p){ if (p && p->nres) p->sel = (p->sel+p->nres-1) % p->nres; }

int palette_result_count(const Palette *p){ return p ? p->nres : 0; }
const char *palette_result_label(const Palette *p, int i){
    if (!p || i<0 || i>=p->nres) return NULL;
    return p->label[p->res[i]];
}
int palette_result_id(const Palette *p, int i){
    if (!p || i<0 || i>=p->nres) return -1;
    return p->id[p->res[i]];
}
int palette_selected(const Palette *p){ return p ? p->sel : 0; }

int palette_confirm(Palette *p){
    if (!p || !p->open || p->nres == 0){ if (p) p->open = 0; return -1; }
    int id = p->id[p->res[p->sel]];
    p->open = 0;
    return id;
}
