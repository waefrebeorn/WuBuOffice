/* eqnum.c -- sequential equation numbering. See eqnum.h. */
#include "eqnum.h"
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EQN_MAX 1024
typedef struct { uint64_t id; char label[16]; } E;

struct EqNum { E e[EQN_MAX]; int n; };

EqNum *eqnum_create(void){ return calloc(1, sizeof(EqNum)); }
void eqnum_destroy(EqNum *e){ free(e); }

static void scan_node(EqNum *e, const wubumodel_node *n, int *counter){
    for (; n; n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n) == WUBUMODEL_FIELD){
            if (e->n < EQN_MAX){
                e->e[e->n].id = wubumodel_node_id(n);
                sprintf(e->e[e->n].label, "(%d)", ++(*counter));
                e->n++;
            }
        }
        scan_node(e, wubumodel_node_first_child(n), counter);
    }
}

int eqnum_scan(EqNum *e, const void *doc){
    if (!e || !doc) return 0;
    e->n = 0;
    int counter = 0;
    scan_node(e, wubumodel_doc_root((const wubumodel_doc*)doc), &counter);
    return e->n;
}

const char *eqnum_label(EqNum *e, uint64_t id){
    if (!e) return NULL;
    for (int i=0;i<e->n;i++) if (e->e[i].id==id) return e->e[i].label;
    return NULL;
}
int eqnum_count(EqNum *e){ return e? e->n : 0; }
