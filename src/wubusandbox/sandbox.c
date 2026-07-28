/* sandbox.c -- plugin capability sandbox. See sandbox.h. */
#include "sandbox.h"

#include <stdlib.h>
#include <string.h>

#define SBX_MAX 64

typedef struct {
    char name[64];
    unsigned requested, granted;
    int denials;
    int used;
} P;

struct Sandbox { P p[SBX_MAX]; int n; };

Sandbox *sandbox_create(void){ return calloc(1, sizeof(Sandbox)); }
void sandbox_destroy(Sandbox *s){ free(s); }

int sandbox_register(Sandbox *s, const char *name, unsigned requested){
    if (!s || !name || s->n>=SBX_MAX) return -1;
    P *p = &s->p[s->n];
    strncpy(p->name, name, 63); p->name[63]=0;
    p->requested = requested;
    p->granted = 0;              /* deny-by-default */
    p->denials = 0;
    p->used = 1;
    return s->n++;
}

static P *get(const Sandbox *s, int id){
    if (!s || id<0 || id>=s->n || !s->p[id].used) return NULL;
    return (P*)&s->p[id];
}

int sandbox_grant(Sandbox *s, int id, unsigned granted){
    P *p = get(s, id); if (!p) return 0;
    p->granted = granted;
    return 1;
}

int sandbox_check(Sandbox *s, int id, unsigned cap){
    P *p = get(s, id); if (!p) return 0;
    unsigned eff = p->requested & p->granted;
    if ((eff & cap) == cap) return 1;
    p->denials++;
    return 0;
}

unsigned sandbox_effective(const Sandbox *s, int id){
    P *p = get(s, id); return p ? (p->requested & p->granted) : 0;
}
int sandbox_denials(const Sandbox *s, int id){
    P *p = get(s, id); return p ? p->denials : 0;
}
const char *sandbox_name(const Sandbox *s, int id){
    P *p = get(s, id); return p ? p->name : NULL;
}
