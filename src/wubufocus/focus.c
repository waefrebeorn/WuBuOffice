/* focus.c -- visible focus indicator config. See focus.h. */
#include "focus.h"
#include <stdlib.h>

struct Focus {
    unsigned char r,g,b,a;
    int width;
    int enabled;
};

Focus *focus_create(void){
    Focus *f = calloc(1, sizeof *f);
    if (f){ f->r=59; f->g=130; f->b=246; f->a=255; f->width=2; f->enabled=1; }
    return f;
}
void focus_destroy(Focus *f){ free(f); }
void focus_set_color(Focus *f, unsigned char r, unsigned char g, unsigned char b, unsigned char a){ if(f){f->r=r;f->g=g;f->b=b;f->a=a;} }
void focus_set_width(Focus *f, int px){ if(f) f->width = px<0?0:px; }
void focus_set_enabled(Focus *f, int on){ if(f) f->enabled = on?1:0; }
unsigned focus_color(const Focus *f){ return f? ((unsigned)f->r<<24)|((unsigned)f->g<<16)|((unsigned)f->b<<8)|f->a : 0; }
int focus_width(const Focus *f){ return f? f->width : 0; }
int focus_enabled(const Focus *f){ return f? f->enabled : 0; }
