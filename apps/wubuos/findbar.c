/* findbar.c -- opaque find/replace engine (see findbar.h).
 * Ported out of view_editor.c so the editor view no longer owns search state.
 */
#include "findbar.h"

#include "doc.h"     /* WuBuPad piece-table Doc */
#include "search.h"  /* WuBuPad regex_literal engine */
#include "regex.h"   /* WuBuPad regex */

#include <stdlib.h>
#include <string.h>

struct FindBar {
    char   query[256];
    char   replace[256];
    int    icase;        /* case-insensitive */
    int    regex;        /* regex vs literal */
    Regex *re;           /* compiled regex (lazy) */
    int    re_bad;       /* last compile failed */
    size_t ms, me;       /* active match [start,end) in bytes */
    int    active;       /* a match is currently selected */
    int    total;        /* count of matches (lazy) */
    int    idx;          /* 1-based index of active match */
    char   msg[64];      /* transient status (e.g. "bad pattern") */
};

FindBar *findbar_create(void){
    return calloc(1, sizeof(FindBar));
}
void findbar_destroy(FindBar *fb){
    if (!fb) return;
    if (fb->re) regex_free(fb->re);
    free(fb);
}

void findbar_set_icase(FindBar *fb, int on){ if (fb) fb->icase = on?1:0; }
void findbar_set_regex(FindBar *fb, int on){ if (fb) fb->regex = on?1:0; }
void findbar_set_query(FindBar *fb, const char *q){
    if (!fb) return;
    if (q) { strncpy(fb->query, q, sizeof fb->query - 1); fb->query[sizeof fb->query - 1] = 0; }
    else fb->query[0] = 0;
    fb->re_bad = 0;
}
void findbar_set_replace(FindBar *fb, const char *r){
    if (!fb) return;
    if (r) { strncpy(fb->replace, r, sizeof fb->replace - 1); fb->replace[sizeof fb->replace - 1] = 0; }
    else fb->replace[0] = 0;
}

int findbar_active(const FindBar *fb){ return fb ? fb->active : 0; }
void findbar_match(const FindBar *fb, size_t *start, size_t *end){
    if (start) *start = fb ? fb->ms : 0;
    if (end)   *end   = fb ? fb->me : 0;
}
void findbar_counts(const FindBar *fb, int *idx, int *total){
    if (idx)   *idx   = fb ? fb->idx   : 0;
    if (total) *total = fb ? fb->total : 0;
}
const char *findbar_msg(const FindBar *fb){ return fb ? fb->msg : ""; }
const char *findbar_query(const FindBar *fb){ return fb ? fb->query : ""; }
const char *findbar_replace(const FindBar *fb){ return fb ? fb->replace : ""; }
int findbar_icase(const FindBar *fb){ return fb ? fb->icase : 0; }
int findbar_regex(const FindBar *fb){ return fb ? fb->regex : 0; }

/* Recompile the regex if needed; returns 1 if a usable pattern is set. */
static int findbar_ensure_re(FindBar *fb){
    if (!fb->regex) return 1;                  /* literal mode: no regex needed */
    if (fb->re && !fb->re_bad) return 1;
    if (fb->re) { regex_free(fb->re); fb->re = NULL; }
    fb->re_bad = 0;
    if (fb->query[0] == '\0') return 0;
    fb->re = regex_compile(fb->query, fb->icase);
    if (!fb->re){ fb->re_bad = 1; return 0; }
    return 1;
}

static int findbar_one(FindBar *fb, const char *t, size_t n, size_t from,
                       size_t *s, size_t *e){
    if (fb->regex){
        if (!findbar_ensure_re(fb)) return 0;
        if (fb->re) return regex_find_from(fb->re, t, n, from, s, e);
        return 0;
    }
    size_t r = search_literal(t, n, fb->query, strlen(fb->query), from);
    if (r == (size_t)-1) return 0;
    *s = r; *e = r + strlen(fb->query);
    return 1;
}

int findbar_next(FindBar *fb, const void *doc, size_t from){
    if (!fb || !doc) return 0;
    char *t = doc_text((const Doc*)doc);
    size_t n = doc_length((const Doc*)doc);
    int got = 0;
    size_t ms=0, me=0;
    if (findbar_one(fb, t, n, from, &ms, &me)) got = 1;
    free(t);
    if (!got){ fb->active = 0; return 0; }
    fb->ms = ms; fb->me = me; fb->active = 1;
    doc_set_selection((Doc*)doc, ms, me);
    doc_set_cursor((Doc*)doc, me);
    /* count total + index (small doc; linear scan) */
    fb->total = 0; fb->idx = 0;
    size_t pos = 0;
    while (pos <= n){
        size_t s=0, en=0; int ok=0;
        char *tt = doc_text((const Doc*)doc);
        if (findbar_one(fb, tt, n, pos, &s, &en)) ok = 1;
        free(tt);
        if (!ok) break;
        fb->total++;
        if (s == ms && en == me) fb->idx = fb->total;
        pos = en;
        if (pos == 0) break;
    }
    if (fb->idx == 0) fb->idx = fb->total; /* match moved */
    return 1;
}

int findbar_prev(FindBar *fb, const void *doc){
    if (!fb || !doc) return 0;
    char *t = doc_text((const Doc*)doc);
    size_t n = doc_length((const Doc*)doc);
    size_t prev_s=(size_t)-1, prev_e=(size_t)-1;
    size_t pos = 0;
    size_t start = fb->active ? fb->ms : 0;
    while (pos <= start){
        size_t s=0, en=0; int ok=0;
        if (findbar_one(fb, t, n, pos, &s, &en)) ok = 1;
        if (!ok) break;
        prev_s = s; prev_e = en;
        pos = en;
        if (pos == 0) break;
    }
    free(t);
    if (prev_s == (size_t)-1) return findbar_next(fb, doc, 0); /* wrap: first match */
    fb->ms = prev_s; fb->me = prev_e; fb->active = 1;
    doc_set_selection((Doc*)doc, prev_s, prev_e);
    doc_set_cursor((Doc*)doc, prev_e);
    return 1;
}

void findbar_replace_one(FindBar *fb, void *doc){
    if (!fb || !doc || !fb->active) return;
    size_t ms = fb->ms, me = fb->me;
    doc_replace((Doc*)doc, ms, me, fb->replace);
    doc_set_cursor((Doc*)doc, ms + strlen(fb->replace));
    doc_set_selection((Doc*)doc, doc_cursor((Doc*)doc), doc_cursor((Doc*)doc));
    fb->active = 0;
    findbar_next(fb, doc, ms);
}

void findbar_replace_all(FindBar *fb, void *doc){
    if (!fb || !doc || !fb->query[0]) return;
    int guard = 0;
    if (!findbar_next(fb, doc, 0)) return;
    while (fb->active && guard++ < 100000){
        size_t ms = fb->ms, me = fb->me;
        doc_replace((Doc*)doc, ms, me, fb->replace);
        fb->active = 0;
        if (!findbar_next(fb, doc, ms + strlen(fb->replace))) break;
        if (fb->ms == ms && fb->me == me) break; /* no progress */
    }
    doc_set_selection((Doc*)doc, doc_cursor((Doc*)doc), doc_cursor((Doc*)doc));
}

void findbar_clear_active(FindBar *fb){
    if (!fb) return;
    fb->active = 0;
    if (fb->re){ regex_free(fb->re); fb->re = NULL; }
    fb->re_bad = 0;
}
