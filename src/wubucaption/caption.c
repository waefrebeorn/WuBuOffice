/* caption.c -- figure/table caption side-table. See caption.h. */
#include "caption.h"
#include <stdlib.h>
#include <string.h>

#define CAP_MAX 1024
typedef struct { uint64_t id; char *text; } C;

struct CaptionMap { C e[CAP_MAX]; int n; };

CaptionMap *caption_create(void){ return calloc(1, sizeof(CaptionMap)); }
void caption_destroy(CaptionMap *m){
    if (!m) return;
    for (int i=0;i<m->n;i++) free(m->e[i].text);
    free(m);
}
int caption_set(CaptionMap *m, uint64_t id, const char *text){
    if (!m || !text) return 0;
    for (int i=0;i<m->n;i++) if (m->e[i].id==id){ free(m->e[i].text); m->e[i].text=strdup(text); return 1; }
    if (m->n>=CAP_MAX) return 0;
    m->e[m->n].id=id; m->e[m->n].text=strdup(text); m->n++; return 1;
}
const char *caption_get(const CaptionMap *m, uint64_t id){
    if (!m) return NULL;
    for (int i=0;i<m->n;i++) if (m->e[i].id==id) return m->e[i].text;
    return NULL;
}
int caption_count(const CaptionMap *m){ return m? m->n : 0; }
uint64_t caption_id_at(const CaptionMap *m, int i){ return (m&&i>=0&&i<m->n)? m->e[i].id : 0; }
const char *caption_text_at(const CaptionMap *m, int i){ return (m&&i>=0&&i<m->n)? m->e[i].text : NULL; }
