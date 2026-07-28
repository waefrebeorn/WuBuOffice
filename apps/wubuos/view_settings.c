/* view_settings.c -- Settings view (UI-25): a real, interactive preferences
 * surface (not a stub). It edits the shared wubusettings singleton and persists
 * to disk on change. Keyboard: +/- zoom, t theme, a autosave toggle, l language
 * cycle, f font-size, s save, Esc close (returns to previous tab via shell). */
#include "wuos.h"
#include "wuos_font.h"
#include "settings.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { WubuSettings *s; int saved_flash; } SetV;

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)scroll;
    SetV *e = v->priv;
    unsigned char *fb = calloc((size_t)w*h, 4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=30;fb[k+1]=33;fb[k+2]=40;fb[k+3]=255; }
    int y = 40;
    wuos_font_draw("WuBuOffice Settings", 20, y, 1, 235,237,240, fb, w, h); y += 36;
    char line[256];
    snprintf(line,sizeof line,"Zoom:            %.0f%%   (use + / -)", e->s?wubusettings_zoom(e->s)*100:100);
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 26;
    snprintf(line,sizeof line,"Theme:           %s   (t)", wubusettings_dark(e->s)?"Dark":"Light");
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 26;
    snprintf(line,sizeof line,"Autosave:        %s   (a)", wubusettings_autosave_ms(e->s)?"ON (5s)":"OFF");
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 26;
    snprintf(line,sizeof line,"UI language:     %s   (l)", wubusettings_language(e->s));
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 26;
    snprintf(line,sizeof line,"Base font size:  %d px   (f)", wubusettings_font_size(e->s));
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 26;
    /* UI-26: live word-wrap + tab-width */
    snprintf(line,sizeof line,"Word wrap:       %s   (w)", wubusettings_word_wrap(e->s)?"ON":"OFF");
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 26;
    snprintf(line,sizeof line,"Tab width:       %d sp   (i / Shift+i)", wubusettings_tab_width(e->s));
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 36;
    /* UXA-41: high-contrast toggle */
    snprintf(line,sizeof line,"High contrast:   %s   (c)", wubusettings_high_contrast(e->s)?"ON":"OFF");
    wuos_font_draw(line, 20, y, 0, 200,203,210, fb, w, h); y += 36;
    wuos_font_draw("Keys:  + zoom in   - zoom out   0 reset zoom", 20, y, 0, 150,153,160, fb, w, h); y += 22;
    wuos_font_draw("       t theme     a autosave  l language  f font-size", 20, y, 0, 150,153,160, fb, w, h); y += 22;
    snprintf(line,sizeof line,"       c high-contrast   s save to disk   (auto-saves on every change)");
    wuos_font_draw(line, 20, y, 0, 150,153,160, fb, w, h); y += 22;
    if (e->saved_flash){
        wuos_font_draw("saved.", 20, y, 0, 120,220,140, fb, w, h);
    }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static char *status(WuView *v){
    SetV *e = v->priv; char *s = malloc(64);
    if (s) snprintf(s,64,"Settings — %s, zoom %.0f%%",
                    wubusettings_dark(e->s)?"dark":"light",
                    wubusettings_zoom(e->s)*100);
    return s;
}

static void on_key(WuView *v, int key, int down){
    SetV *e = v->priv;
    if (!down || !e->s) return;
    WubuSettings *s = e->s;
    if (key=='+' || key=='='){ wubusettings_set_zoom(s, wubusettings_zoom(s)+0.1); }
    else if (key=='-'){ wubusettings_set_zoom(s, wubusettings_zoom(s)-0.1); }
    else if (key=='0'){ wubusettings_set_zoom(s, 1.0); }
    else if (key=='t' || key=='T'){ wubusettings_set_dark(s, !wubusettings_dark(s)); }
    else if (key=='a' || key=='A'){ wubusettings_set_autosave_ms(s, wubusettings_autosave_ms(s)?0:5000); }
    else if (key=='l' || key=='L'){
        const char *cur = wubusettings_language(s);
        const char *next = (!strcmp(cur,"en"))?"ar":(!strcmp(cur,"ar"))?"fr":"en";
        wubusettings_set_language(s, next);
    }
    else if (key=='f' || key=='F'){ wubusettings_set_font_size(s, wubusettings_font_size(s)+2>48?10:wubusettings_font_size(s)+2); }
    else if (key=='w' || key=='W'){ wubusettings_set_word_wrap(s, !wubusettings_word_wrap(s)); }
    else if (key=='i' || key=='I'){ int tw=wubusettings_tab_width(s)+1; wubusettings_set_tab_width(s, tw>16?1:tw); }
    else if (key=='c' || key=='C'){ wubusettings_set_high_contrast(s, !wubusettings_high_contrast(s)); }
    else if (key=='s' || key=='S'){ wubusettings_save(s, NULL); e->saved_flash = 12; return; }
    else return;
    /* every change persists immediately */
    wubusettings_save(s, NULL);
    if (e->saved_flash) e->saved_flash = 12;
}

static void destroy(WuView *v){ SetV *e = v->priv; free(e); free(v); }

WuView *wuos_settings_create(void){
    SetV *e = calloc(1, sizeof *e);
    e->s = wubusettings_shared();
    e->saved_flash = 0;
    WuView *v = calloc(1, sizeof *v);
    v->name = "Settings";
    v->priv = e;
    v->destroy = destroy;
    v->render = render;
    v->status = status;
    v->on_key = on_key;
    return v;
}
