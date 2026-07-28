/* redact.c -- redaction set + redacted-text export. See redact.h. */
#include "redact.h"

#include <stdlib.h>
#include <string.h>

#define RED_MAX 256
typedef struct { size_t a, b; } R;

struct Redact { R e[RED_MAX]; int n; };

Redact *redact_create(void){ return calloc(1, sizeof(Redact)); }
void redact_destroy(Redact *r){ free(r); }

int redact_mark(Redact *r, size_t a, size_t b){
    if (!r || b<=a) return 0;
    if (r->n>=RED_MAX) return 0;
    r->e[r->n].a=a; r->e[r->n].b=b; r->n++;
    return 1;
}
int redact_count(const Redact *r){ return r? r->n : 0; }

char *redact_apply(const Redact *r, const char *text){
    if (!r || !text) return NULL;
    size_t L = strlen(text);
    for (int i=0;i<r->n;i++) if (r->e[i].b > L) return NULL; /* out of bounds */
    /* '█' is 3-byte UTF-8; worst case every byte redacted */
    char *out = malloc(L*3+1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i=0;i<L;i++){
        int hidden = 0;
        for (int j=0;j<r->n;j++) if (i>=r->e[j].a && i<r->e[j].b){ hidden=1; break; }
        if (hidden){ out[o++]=(char)0xE2; out[o++]=(char)0x96; out[o++]=(char)0x88; }
        else out[o++] = text[i];
    }
    out[o]=0;
    return out;
}
