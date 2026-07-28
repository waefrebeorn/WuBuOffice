/* a11yannounce.c -- screen-reader announcement queue. See a11yannounce.h. */
#include "a11yannounce.h"

#include <stdlib.h>
#include <string.h>

#define AN_MAX 256
typedef struct { char *m; } A;

struct A11yAnnounce { A e[AN_MAX]; int n, head; };

A11yAnnounce *a11y_announce_create(void){ return calloc(1, sizeof(A11yAnnounce)); }
void a11y_announce_destroy(A11yAnnounce *a){
    if (!a) return;
    for (int i=0;i<a->n;i++) free(a->e[(a->head+i)%AN_MAX].m);
    free(a);
}

void a11y_announce_push(A11yAnnounce *a, const char *msg){
    if (!a || !msg || a->n>=AN_MAX) return;
    a->e[(a->head+a->n)%AN_MAX].m = strdup(msg);
    a->n++;
}
int a11y_announce_pending(const A11yAnnounce *a){ return a? a->n : 0; }
char *a11y_announce_pop(A11yAnnounce *a){
    if (!a || a->n==0) return NULL;
    char *m = a->e[a->head].m;
    a->head = (a->head+1)%AN_MAX; a->n--;
    return m;
}
