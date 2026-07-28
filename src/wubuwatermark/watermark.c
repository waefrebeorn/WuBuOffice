/* watermark.c -- page watermark config. See watermark.h. */
#include "watermark.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct Watermark {
    char text[128];
    int  angle;
    float opacity;
    int  enabled;
};

Watermark *watermark_create(void){
    Watermark *w = calloc(1, sizeof *w);
    if (w){ w->opacity = 0.18f; w->enabled = 0; strncpy(w->text,"DRAFT",127); }
    return w;
}
void watermark_destroy(Watermark *w){ free(w); }
void watermark_set_text(Watermark *w, const char *text){ if (w&&text){ strncpy(w->text,text,127); w->text[127]=0; } }
void watermark_set_angle(Watermark *w, int d){ if (w) w->angle = d % 360; }
void watermark_set_opacity(Watermark *w, float o){ if (w){ if (o<0)o=0; if (o>1)o=1; w->opacity=o; } }
void watermark_set_enabled(Watermark *w, int on){ if (w) w->enabled = on?1:0; }
const char *watermark_text(const Watermark *w){ return w? w->text : ""; }
int watermark_angle(const Watermark *w){ return w? w->angle : 0; }
float watermark_opacity(const Watermark *w){ return w? w->opacity : 0; }
int watermark_enabled(const Watermark *w){ return w? w->enabled : 0; }
