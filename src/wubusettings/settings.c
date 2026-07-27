/* settings.c -- see settings.h. Opaque config with JSON persistence. */
#include "settings.h"
#include "json.h"   /* wubujson: clean-room JSON (no third-party dep) */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_PATH "~/.wubuos/settings.json"

struct WubuSettings {
    double zoom;
    int    dark;
    int    autosave_ms;
    char   language[8];
    int    font_size;
    int    high_contrast;
};

WubuSettings *wubusettings_create(void){
    WubuSettings *s = calloc(1, sizeof(WubuSettings));
    if (!s) return NULL;
    s->zoom        = 1.0;
    s->dark        = 1;
    s->autosave_ms = 5000;
    s->language[0] = 'e'; s->language[1] = 'n'; s->language[2] = '\0';
    s->font_size   = 16;
    s->high_contrast = 0;
    return s;
}

/* process-wide singleton (declared below; forward reference via this static) */
static WubuSettings *g_shared = NULL;

void wubusettings_destroy(WubuSettings *s){
    if (!s) return;
    /* if this is the shared singleton, clear the global so a later
     * wubusettings_shared() re-creates it (don't leave a dangling ptr). */
    if (s == g_shared) g_shared = NULL;
    free(s);
}

static char *expand_path(const char *p){
    /* support a leading ~/ by swapping in $HOME. Caller frees. */
    if (p && p[0]=='~' && (p[1]=='/' || p[1]=='\0')){
        const char *home = getenv("HOME");
        if (!home) home = "/tmp";
        size_t n = strlen(home) + strlen(p+1) + 1;
        char *out = malloc(n);
        if (!out) return NULL;
        snprintf(out, n, "%s%s", home, p+1);
        return out;
    }
    return p ? strdup(p) : NULL;
}

int wubusettings_load(WubuSettings *s, const char *path){
    if (!s) return -1;
    char *ep = expand_path(path ? path : DEFAULT_PATH);
    if (!ep) return -1;
    FILE *f = fopen(ep, "rb");
    int rc = 0;
    if (!f){
        if (ep != (path?path:DEFAULT_PATH)) free(ep);
        return 0;   /* absent -> keep factory defaults */
    }
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf){ fclose(f); if (ep!=(path?path:DEFAULT_PATH)) free(ep); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd]='\0';
    fclose(f);

    JVal *root = j_parse(buf, NULL);
    free(buf);
    if (root && j_type(root)==J_OBJ){
        JVal *v;
        if ((v = j_obj_get(root,"zoom")) && j_type(v)==J_NUM) s->zoom = j_as_num(v);
        if ((v = j_obj_get(root,"dark")) && j_type(v)==J_NUM) s->dark = j_as_num(v)!=0;
        if ((v = j_obj_get(root,"autosave_ms")) && j_type(v)==J_NUM) s->autosave_ms = (int)j_as_num(v);
        if ((v = j_obj_get(root,"language")) && j_type(v)==J_STR){
            const char *l = j_as_str(v); strncpy(s->language, l, sizeof s->language-1); s->language[sizeof s->language-1]='\0';
        }
        if ((v = j_obj_get(root,"font_size")) && j_type(v)==J_NUM) s->font_size = (int)j_as_num(v);
        if ((v = j_obj_get(root,"high_contrast")) && j_type(v)==J_NUM) s->high_contrast = j_as_num(v)!=0;
        rc = 0;
    }
    j_free(root);
    if (ep != (path?path:DEFAULT_PATH)) free(ep);
    return rc;
}

int wubusettings_save(const WubuSettings *s, const char *path){
    if (!s) return -1;
    JVal *root = j_obj();
    if (!root) return -1;
    j_obj_put(root, "zoom", j_num(s->zoom));
    j_obj_put(root, "dark", j_num((double)s->dark));
    j_obj_put(root, "autosave_ms", j_num((double)s->autosave_ms));
    j_obj_put(root, "language", j_str(s->language));
    j_obj_put(root, "font_size", j_num((double)s->font_size));
    j_obj_put(root, "high_contrast", j_num((double)s->high_contrast));
    char *txt = j_emit(root);
    j_free(root);
    if (!txt) return -1;

    char *ep = expand_path(path ? path : DEFAULT_PATH);
    if (!ep){ free(txt); return -1; }
    /* ensure parent dir exists */
    char *slash = strrchr(ep, '/');
    if (slash && slash != ep){ *slash='\0'; mkdir(ep, 0755); *slash='/'; }
    FILE *f = fopen(ep, "wb");
    int rc = -1;
    if (f){ fwrite(txt, 1, strlen(txt), f); fclose(f); rc = 0; }
    free(txt);
    if (ep != (path?path:DEFAULT_PATH)) free(ep);
    return rc;
}

double wubusettings_zoom(const WubuSettings *s){ return s ? s->zoom : 1.0; }
void wubusettings_set_zoom(WubuSettings *s, double z){
    if (!s) return;
    if (z < 0.5) z = 0.5; if (z > 3.0) z = 3.0;
    s->zoom = z;
}
int  wubusettings_dark(const WubuSettings *s){ return s ? s->dark : 1; }
void wubusettings_set_dark(WubuSettings *s, int dark){ if (s) s->dark = dark?1:0; }
int  wubusettings_autosave_ms(const WubuSettings *s){ return s ? s->autosave_ms : 5000; }
void wubusettings_set_autosave_ms(WubuSettings *s, int ms){ if (s) s->autosave_ms = ms<0?0:ms; }
const char *wubusettings_language(const WubuSettings *s){ return s ? s->language : "en"; }
void wubusettings_set_language(WubuSettings *s, const char *lang){
    if (!s || !lang) return;
    strncpy(s->language, lang, sizeof s->language-1); s->language[sizeof s->language-1]='\0';
}
int  wubusettings_font_size(const WubuSettings *s){ return s ? s->font_size : 16; }
void wubusettings_set_font_size(WubuSettings *s, int px){ if (s){ if(px<8)px=8; if(px>48)px=48; s->font_size=px; } }

int  wubusettings_high_contrast(const WubuSettings *s){ return s ? s->high_contrast : 0; }
void wubusettings_set_high_contrast(WubuSettings *s, int on){ if (s) s->high_contrast = on?1:0; }

/* process-wide singleton (defined near top of this file) */
WubuSettings *wubusettings_shared(void){
    if (!g_shared){
        g_shared = wubusettings_create();
        if (g_shared) wubusettings_load(g_shared, NULL);
    }
    return g_shared;
}
