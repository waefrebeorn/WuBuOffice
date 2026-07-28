/* bkmk.c -- opaque line-bookmark set (see bkmk.h).
 * Ported out of view_editor.c so the editor no longer owns the bookmark array.
 */
#include "bkmk.h"

#include <stdlib.h>
#include <string.h>

#define BKMK_MAX 256

struct BkMk {
    int lines[BKMK_MAX]; /* sorted ascending */
    int n;
};

BkMk *bkmk_create(void){ return calloc(1, sizeof(BkMk)); }
void  bkmk_destroy(BkMk *b){ free(b); }

int bkmk_count(const BkMk *b){ return b ? b->n : 0; }

int bkmk_has(const BkMk *b, int line){
    if (!b) return 0;
    for (int i=0;i<b->n;i++) if (b->lines[i]==line) return 1;
    return 0;
}

void bkmk_toggle(BkMk *b, int line){
    if (!b || line<0) return;
    for (int i=0;i<b->n;i++){
        if (b->lines[i]==line){
            memmove(&b->lines[i], &b->lines[i+1], (b->n-i-1)*sizeof(int));
            b->n--;
            return;
        }
    }
    if (b->n < BKMK_MAX){
        int i = b->n;
        while (i>0 && b->lines[i-1] > line){ b->lines[i]=b->lines[i-1]; i--; }
        b->lines[i]=line; b->n++;
    }
}

int bkmk_jump(BkMk *b, int from, int dir){
    if (!b || !b->n) return -1;
    if (dir > 0){
        int best = b->lines[b->n-1];
        for (int i=0;i<b->n;i++) if (b->lines[i] > from){ best=b->lines[i]; break; }
        return best;
    } else {
        int best = -1;
        for (int i=b->n-1;i>=0;i--) if (b->lines[i] < from){ best=b->lines[i]; break; }
        return best;
    }
}
