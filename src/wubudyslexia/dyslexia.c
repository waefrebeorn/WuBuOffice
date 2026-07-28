/* dyslexia.c -- dyslexia-friendly mode config. See dyslexia.h. */
#include "dyslexia.h"
#include <stdlib.h>
#include <string.h>

struct Dyslexia {
    int enabled;
    char face[64];
    float spacing;
};

Dyslexia *dyslexia_create(void){
    Dyslexia *d = calloc(1, sizeof *d);
    if (d){ d->enabled=0; d->face[0]=0; d->spacing=1.5f; }
    return d;
}
void dyslexia_destroy(Dyslexia *d){ free(d); }
void dyslexia_set_enabled(Dyslexia *d, int on){ if(d) d->enabled = on?1:0; }
int dyslexia_enabled(const Dyslexia *d){ return d? d->enabled : 0; }
void dyslexia_set_face(Dyslexia *d, const char *face){ if(d&&face){ strncpy(d->face,face,63); d->face[63]=0; } }
const char *dyslexia_face(const Dyslexia *d){ return d? d->face : ""; }
void dyslexia_set_spacing(Dyslexia *d, float s){ if(d){ if(s<1.0f)s=1.0f; if(s>3.0f)s=3.0f; d->spacing=s; } }
float dyslexia_spacing(const Dyslexia *d){ return d? d->spacing : 1.0f; }
