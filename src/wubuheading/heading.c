/* heading.c -- semantic heading-level enforcement. See heading.h. */
#include "heading.h"
#include "model.h"

#include <stdlib.h>

#define HD_MAX 1024
typedef struct { uint64_t id; int level; } H;

struct Heading { H e[HD_MAX]; int n; };

Heading *heading_create(void){ return calloc(1, sizeof(Heading)); }
void heading_destroy(Heading *h){ free(h); }

static void scan(Heading *h, const wubumodel_node *n, int *lvl){
    for (; n; n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) == WUBUMODEL_SECTION){
            if (h->n < HD_MAX){
                h->e[h->n].id = wubumodel_node_id(n);
                h->e[h->n].level = ++(*lvl);
                h->n++;
            }
        }
        scan(h, wubumodel_node_first_child(n), lvl);
    }
}

int heading_enforce(Heading *h, const void *doc){
    if (!h || !doc) return 0;
    h->n = 0;
    int lvl = 0;
    scan(h, wubumodel_doc_root((const wubumodel_doc*)doc), &lvl);
    return h->n;
}

int heading_level(Heading *h, uint64_t id){
    if (!h) return 0;
    for (int i=0;i<h->n;i++) if (h->e[i].id==id) return h->e[i].level;
    return 0;
}
int heading_count(Heading *h){ return h? h->n : 0; }
