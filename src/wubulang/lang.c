/* lang.c -- per-node language attribute side-table. See lang.h. */
#include "lang.h"
#include <stdlib.h>
#include <string.h>

#define LANG_MAX 1024

typedef struct { uint64_t id; char tag[32]; } L;

struct LangMap { L e[LANG_MAX]; int n; };

LangMap *lang_create(void){ return calloc(1, sizeof(LangMap)); }
void lang_destroy(LangMap *m){ free(m); }

int lang_set(LangMap *m, uint64_t id, const char *tag){
    if (!m || !tag) return 0;
    for (int i=0;i<m->n;i++) if (m->e[i].id==id){ strncpy(m->e[i].tag,tag,31); m->e[i].tag[31]=0; return 1; }
    if (m->n>=LANG_MAX) return 0;
    m->e[m->n].id=id; strncpy(m->e[m->n].tag,tag,31); m->e[m->n].tag[31]=0; m->n++; return 1;
}
const char *lang_get(const LangMap *m, uint64_t id){
    if (!m) return NULL;
    for (int i=0;i<m->n;i++) if (m->e[i].id==id) return m->e[i].tag;
    return NULL;
}
int lang_count(const LangMap *m){ return m? m->n : 0; }
uint64_t lang_id_at(const LangMap *m, int i){ return (m&&i>=0&&i<m->n)? m->e[i].id : 0; }
const char *lang_tag_at(const LangMap *m, int i){ return (m&&i>=0&&i<m->n)? m->e[i].tag : NULL; }
