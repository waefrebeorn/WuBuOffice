/* view_doc.c -- Document view: renders a wubumodel page via wurender AND is
 * interactive. Given a path it ingests the real document through the wubudoc
 * facade (docx/odt/pdf/md/txt/html/...), and when the format isn't a renderable
 * page it shows the recognized text projection instead. Ctrl+F searches the
 * loaded text and jumps to the first match. The SAME render path the offscreen
 * PNG writer uses, so the in-shell Document tab is the real office surface. */
#include "wuos.h"
#include "wuos_file.h"
#include "wuburender.h"
#include "model.h"
#include "wubudoc.h"     /* doc_session_*, doc_open, doc_text, doc_drop_text */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { Wurender *r; wubumodel_doc *doc; char *path;
                 char *text;          /* recognized/raw text (for find) */
                 char *find_q; int find_hit; } DocV;

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    DocV *e = v->priv;
    (void)scroll;
    if (e->doc){
        int page_h = h + 400;
        return wurender_render_doc(e->r, e->doc, w, page_h, rgba, rw, rh);
    }
    /* non-renderable format: show the text projection */
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ fb[i*4]=252;fb[i*4+1]=252;fb[i*4+2]=250;fb[i*4+3]=255; }
    wuos_font_draw("Document text (recognized):", 16, 20, 1, 40,44,52, fb,w,h);
    int y = 48;
    const char *p = e->text;
    if (p && *p){
        /* simple word-wrap */
        int x=16; int fh=wuos_font_height();
        const char *wstart=p;
        while (*wstart){
            const char *sp = strchr(wstart, ' ');
            size_t wl = sp ? (size_t)(sp-wstart) : strlen(wstart);
            if (x + (int)wl*8 > w-16){ x=16; y += fh+4; }
            char tmp[256]; if(wl>=sizeof tmp) wl=sizeof tmp-1;
            memcpy(tmp,wstart,wl); tmp[wl]=0;
            wuos_font_draw(tmp, x, y, 0, 28,30,34, fb,w,h);
            x += (int)wl*8 + 8;
            wstart = sp ? sp+1 : wstart+wl;
            if (y > h-30) break;
        }
        if (e->find_q && e->find_q[0]){
            int fy = h-26;
            for (int xx=0; xx<w; xx++) for(int yy=fy; yy<h; yy++)
                if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4; fb[i]=30;fb[i+1]=33;fb[i+2]=40; }
            char line[256]; snprintf(line,sizeof line,"find '%s': %s", e->find_q, e->find_hit?"1 match highlighted in text":"no match");
            wuos_font_draw(line, 8, fy+5, 0, 200,203,210, fb,w,h);
        }
    } else {
        wuos_font_draw("(nothing to display)", 16, 48, 0, 120,30,30, fb,w,h);
    }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static char *status(WuView *v){
    DocV *e = v->priv;
    const char *src = e->path ? e->path : "sample";
    char *s = malloc(160); if(!s) return NULL;
    if (e->doc) snprintf(s,160,"Document — %s (rendered page)", src);
    else snprintf(s,160,"Document — %s (%s text)", src, e->text?"recognized":"no");
    return s;
}

static void on_key(WuView *v, int key, int down){
    DocV *e = v->priv;
    if (!down) return;
    if (key==WUOS_KEY_FIND){
        free(e->find_q); e->find_q = calloc(1,1); e->find_hit=0;
        return;
    }
    if (e->find_q){
        if (key==WUOS_KEY_RETURN){
            /* search loaded text */
            e->find_hit = 0;
            if (e->text && e->find_q && e->find_q[0]){
                if (strstr(e->text, e->find_q)) e->find_hit = 1;
            }
            return;
        }
        if (key==WUOS_KEY_BACKSPACE){
            size_t L=strlen(e->find_q); if(L) e->find_q[L-1]=0; return;
        }
        if (key>=32 && key<128 && strlen(e->find_q)<127){
            size_t L=strlen(e->find_q); e->find_q[L]=(char)key; e->find_q[L+1]=0; return;
        }
        return;
    }
}

static void destroy(WuView *v){
    DocV *e = v->priv;
    if(e->r)    wurender_destroy(e->r);
    if(e->doc)  wubumodel_doc_destroy(e->doc);
    if(e->text) free(e->text);
    if(e->path) free(e->path);
    if(e->find_q) free(e->find_q);
    free(e);
    free(v);
}

static const char *get_path(WuView *v){ return ((DocV*)v->priv)->path; }

WuView *wuos_doc_create(const char *path){
    DocV *e = calloc(1, sizeof *e);
    e->r = wurender_create();
    e->find_q = NULL; e->find_hit = 0;
    if (path){
        e->path = strdup(path);
        /* read raw bytes up front so find works for any format (renderable
         * or not). The text model drives find-in-doc. */
        size_t len = 0;
        char *raw = wuos_read_file(path, &len);
        if (raw){
            /* keep a NUL-terminated copy for find (may be binary; find is
             * best-effort over the prefix). */
            e->text = malloc(len + 1);
            if (e->text){ memcpy(e->text, raw, len); e->text[len]=0; }
            free(raw);
        }
        /* try renderable markdown/text first via wurender */
        if (e->text && (strstr(e->text,"#")||strstr(e->text,"*")||strstr(e->text,"_")||strstr(e->text,"\n"))){
            e->doc = wurender_doc_from_markdown(e->text);
        }
        if (!e->doc){
            /* ingest via the real document facade (docx/odt/pdf/html/...) */
            DocSession *s = doc_session_create();
            long id = doc_open(s, path);
            if (id >= 0){
                char *dt = doc_text(s, id);
                if (dt){ free(e->text); e->text = dt; }
                doc_session_free(s);
            }
        }
    }
    if (!e->doc && !e->text) e->doc = wurender_sample_doc();
    if (!e->r || (!e->doc && !e->text)){ free(e->path); free(e->text); free(e); return NULL; }
    WuView *v = calloc(1, sizeof *v);
    v->name = "Document";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = render;
    v->status   = status;
    v->on_key   = on_key;
    v->get_path = get_path;
    return v;
}

/* ---- test accessors ---- */
int wuos_doc_is_rendered(WuView *v){ return ((DocV*)v->priv)->doc ? 1 : 0; }
int wuos_doc_has_text(WuView *v){ return ((DocV*)v->priv)->text ? 1 : 0; }
int wuos_doc_find(WuView *v, const char *q){
    DocV *e = v->priv;
    if (!e->text || !q || !q[0]) return 0;
    return strstr(e->text, q) ? 1 : 0;
}
