/* scope.c -- table header-row scope side-table. See scope.h. */
#include "scope.h"
#include <stdlib.h>
#include <string.h>

#define SC_MAX 1024
typedef struct { uint64_t id; char scope[8]; } S;

struct ScopeMap { S e[SC_MAX]; int n; };

ScopeMap *scope_create(void){ return calloc(1, sizeof(ScopeMap)); }
void scope_destroy(ScopeMap *m){ free(m); }
int scope_set(ScopeMap *m, uint64_t id, const char *scope){
    if (!m || !scope) return 0;
    const char *ok = (!strcmp(scope,"col")||!strcmp(scope,"row")||!strcmp(scope,"none"))? scope : "col";
    for (int i=0;i<m->n;i++) if (m->e[i].id==id){ strncpy(m->e[i].scope,ok,7); return 1; }
    if (m->n>=SC_MAX) return 0;
    m->e[m->n].id=id; strncpy(m->e[m->n].scope,ok,7); m->n++; return 1;
}
const char *scope_get(const ScopeMap *m, uint64_t id){
    if (!m) return NULL;
    for (int i=0;i<m->n;i++) if (m->e[i].id==id) return m->e[i].scope;
    return NULL;
}
int scope_count(const ScopeMap *m){ return m? m->n : 0; }
uint64_t scope_id_at(const ScopeMap *m, int i){ return (m&&i>=0&&i<m->n)? m->e[i].id : 0; }
