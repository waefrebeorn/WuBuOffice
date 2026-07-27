/* view_doc.c -- Document view: renders a wubumodel page via wurender.
 * The SAME render path the offscreen PNG writer and the live wubuwordwin use,
 * so the in-shell Document tab is the real office document surface. Loads a
 * Markdown/text file when given a path, else shows the bundled sample. */
#include "wuos.h"
#include "wuos_file.h"
#include "wuburender.h"
#include "model.h"

#include <stdlib.h>
#include <string.h>

typedef struct { Wurender *r; wubumodel_doc *doc; char *path; } DocV;

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    (void)scroll;
    DocV *e = v->priv;
    int page_h = h + 400;   /* tall page we scroll through */
    return wurender_render_doc(e->r, e->doc, w, page_h, rgba, rw, rh);
}

static void destroy(WuView *v){
    DocV *e = v->priv;
    wubumodel_doc_destroy(e->doc);
    wurender_destroy(e->r);
    free(e->path);
    free(e);
}

static const char *get_path(WuView *v){ return ((DocV*)v->priv)->path; }

WuView *wuos_doc_create(const char *path){
    DocV *e = calloc(1, sizeof *e);
    e->r = wurender_create();
    if (path){
        e->path = strdup(path);
        size_t len = 0;
        char *txt = wuos_read_file(path, &len);
        if (txt){ e->doc = wurender_doc_from_markdown(txt); free(txt); }
    }
    if (!e->doc) e->doc = wurender_sample_doc();
    if (!e->r || !e->doc){ free(e->path); free(e); return NULL; }
    WuView *v = calloc(1, sizeof *v);
    v->name = "Document";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = render;
    v->get_path = get_path;
    return v;
}
