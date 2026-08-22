/* view_doc.c -- Document view: renders a wubumodel page via wurender AND is
 * interactive. Given a path it ingests the real document through the wubudoc
 * facade (docx/odt/pdf/md/txt/html/...), and when the format isn't a renderable
 * page it shows the recognized text projection instead. Ctrl+F searches the
 * loaded text and jumps to the first match. The SAME render path the offscreen
 * PNG writer uses, so the in-shell Document tab is the real office surface. */
#include "wuos.h"
#include "wuos_file.h"
#include "wuos_font.h"    /* wuos_font_draw: text raster callback for svg_rasterize_cb */
#include "wuos_theme.h"
#include "wuburender.h"
#include "model.h"
#include "wubudoc.h"     /* doc_session_*, doc_open, doc_text, doc_drop_text */
#include "chart.h"       /* wubuchart: insert chart (INT-1) */
#include "draw.h"        /* wubudraw: insert shape (INT-3) */
#include "math.h"        /* wubumath: insert equation (INT-3) */
#include "a11y.h"        /* wubua11y: check (INT-5) */
#include "rast.h"        /* wubusvg rasterizer: SVG -> RGBA (gap #13) */
#include "ublayout.h"      /* central text pipeline: model -> laid-out pages */
#include "shape.h"        /* wubushape: Bidi per-run reorder for RTL */
#include "toc.h"          /* DOC-54: table-of-contents generator */
#include "settings.h"      /* UXA-41: high-contrast colors */
#include "script.h"        /* DOC-97: wubuscript computed fields */
#include "doccmd.h"
#include "view_doc_internal.h"

/* defined in view_doc_render.c */
int doc_render(WuView *v, int w, int h, int scroll, unsigned char **rgba, int *rw, int *rh);

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* forward declarations (defined below) */
static int wuos_doc_footnote_count(WuView *v);
int epub_write(wubumodel_doc *doc, const char *out, const char *title, const char *lang);

/* metric callback for the layout: uses the app font (wubulayout only sees this
 * function pointer, so the engine layer never includes an app header). */




/* All document-editing commands now live in the opaque doccmd module
 * (doccmd.h/doccmd.c); this view delegates to them below in on_key. */

/* DOC-58: return the Nth top-level paragraph in the document body. */
static char *sidebar(WuView *v){
    DocV *e = v->priv;
    if (!e->doc) return NULL;
    if (!e->toc) e->toc = toc_build(e->doc, NULL, NULL);
    int n = toc_count(e->toc);
    if (n <= 0){
        /* Guided empty state (research: empty panes must be useful, not blank).
         * Tells the user exactly what populates this outline. */
        const char *guide =
            "Navigator — Document outline\n"
            "\n"
            "No headings yet.\n"
            "Apply Heading 1/2/3 (toolbar or\n"
            "Ctrl+1/2/3) and they appear\n"
            "here as a clickable outline.";
        return strdup(guide);
    }
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    for (int i=0;i<n;i++){
        const char *t = toc_title(e->toc, i);
        int lvl = toc_level(e->toc, i);
        if (!t) continue;
        char line[256];
        snprintf(line, sizeof line, "%*s%s\n", (lvl-1)*2, "", t);
        size_t add = strlen(line);
        if (len + add + 1 > cap){ cap = (len+add+1)*2; char *nb = realloc(out, cap); if(!nb){ free(out); return NULL; } out=nb; }
        memcpy(out+len, line, add); len += add; out[len]=0;
    }
    return out;
}

/* Walk the doc model collecting run text into a malloc'd string (for the
 * status word/char count). Caller frees. Recurses through sections. */
static void doc_walk_text(wubumodel_node *n, char **out, size_t *len, size_t *cap){
    for (; n; n = wubumodel_node_next_sibling(n)){
        if (wubumodel_node_kind(n)==WUBUMODEL_PARAGRAPH){
            for (wubumodel_node *r = wubumodel_node_first_child(n);
                 r; r = wubumodel_node_next_sibling(r)){
                const char *t = wubumodel_node_text(r);
                if (t && *t){
                    size_t add = strlen(t);
                    if (*len+add+1 > *cap){ *cap=(*len+add+1)*2; char *nb=realloc(*out,*cap); if(!nb) return; *out=nb; }
                    memcpy(*out+*len, t, add); *len+=add; (*out)[*len]=0;
                }
            }
            if (*len+1 > *cap){ *cap*=2; char *nb=realloc(*out,*cap); if(!nb) return; *out=nb; }
            (*out)[(*len)++]='\n'; (*out)[*len]=0;
        } else {
            wubumodel_node *c = wubumodel_node_first_child(n);
            if (c) doc_walk_text(c, out, len, cap);
        }
    }
}
static char *doc_model_text(wubumodel_doc *d){
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    wubumodel_node *root = wubumodel_doc_root(d);
    if (root) doc_walk_text(wubumodel_node_first_child(root), &out, &len, &cap);
    return out;
}

static char *status(WuView *v){
    DocV *e = v->priv;
    /* UI-35: breadcrumb / location bar over the loaded path */
    const char *src = e->path ? e->path : "sample";
    char *s = malloc(200); if(!s) return NULL;
    const char *base = strrchr(src, '/');
    base = base ? base+1 : src;
    int notes = wuos_doc_footnote_count(v);
    /* live word + character counts from the text model (LibreOffice parity) */
    size_t words=0, chars=0, inword=0;
    const char *txt = NULL;
    char *tm = NULL;
    if (e->text) txt = e->text;
    else if (e->doc){ tm = doc_model_text(e->doc); txt = tm; }
    if (txt) for (const char *p=txt; *p; p++){
        if (*p!=' ' && *p!='\n' && *p!='\t'){ chars++; if(!inword){ inword=1; words++; } }
        else inword = 0;
    }
    free(tm);
    if (e->doc){
        if (notes > 0)
            snprintf(s,200,"Document ▸ %s ▸ rendered page ▸ %d note(s) ▸ %zu words", base, notes, words);
        else
            snprintf(s,200,"Document ▸ %s ▸ rendered page ▸ %zu words", base, words);
    }
    else snprintf(s,200,"Document ▸ %s ▸ %s text", base, e->text?"recognized":"no");
    return s;
}

static void on_key(WuView *v, int key, int down){
    DocV *e = v->priv;
    if (!down) return;
    if (key==WUOS_KEY_INSERT_CHART){
        if (e->nobj < DOC_MAX_OBJS){ int w=0,h=0; unsigned char *fb=doccmd_insert_chart(wuos_svg_text,&w,&h);
            if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
        return; }
    if (key==WUOS_KEY_INSERT_DRAW){
        if (e->nobj < DOC_MAX_OBJS){ int w=0,h=0; unsigned char *fb=doccmd_insert_draw(wuos_svg_text,&w,&h);
            if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
        return; }
    if (key==WUOS_KEY_INSERT_MATH){
        if (e->nobj < DOC_MAX_OBJS){ int w=0,h=0; unsigned char *fb=doccmd_insert_math(wuos_svg_text,&w,&h);
            if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
        return; }
    if (key==WUOS_KEY_EXPORT_EPUB){ free(e->epub_msg); e->epub_msg = doccmd_export_epub(e->doc, "/tmp/wubuos_export.epub"); return; }
    if (key==WUOS_KEY_EXPORT_PDF){      free(e->epub_msg); e->epub_msg = doccmd_export_pdf     (e->doc); return; }
    if (key==WUOS_KEY_EXPORT_HTML){     free(e->epub_msg); e->epub_msg = doccmd_export_html    (e->doc); return; }
    if (key==WUOS_KEY_EXPORT_MARKDOWN){ free(e->epub_msg); e->epub_msg = doccmd_export_markdown(e->doc); return; }
    if (key==WUOS_KEY_EXPORT_LATEX){    free(e->epub_msg); e->epub_msg = doccmd_export_latex   (e->doc); return; }
    if (key==WUOS_KEY_EXPORT_RTF){      free(e->epub_msg); e->epub_msg = doccmd_export_rtf     (e->doc); return; }
    if (key==WUOS_KEY_SAVE){ free(e->epub_msg); e->epub_msg = doccmd_save(e->doc, e->path); return; }
    if (key==WUOS_KEY_A11Y_CHECK){ doccmd_a11y_check(e->doc, &e->a11y); e->a11y_done = 1; return; }
    if (key==WUOS_KEY_INSERT_LINK){ if (doccmd_insert_link(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_LIST){ if (doccmd_insert_list(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_TABLE){ if (doccmd_insert_table(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_IMAGE){ if (doccmd_insert_image(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_PAGEBREAK){ if (doccmd_insert_pagebreak(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_SECTIONBREAK){ if (doccmd_insert_sectionbreak(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_HEADER){ if (doccmd_insert_header(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_FOOTER){ if (doccmd_insert_footer(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_COMMENT){ if (doccmd_insert_comment(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_TRACKCHANGE){ if (doccmd_insert_trackchange(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_FIELD){ if (doccmd_insert_field(e->doc)) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_INSERT_SCRIPT){ if (doccmd_insert_script_field(e->doc, "lines * 2")) e->toc_dirty=1; return; }
    if (key==WUOS_KEY_STYLE_H1){ doc_apply_named_style(e, "Heading1"); return; }
    if (key==WUOS_KEY_STYLE_H2){ doc_apply_named_style(e, "Heading2"); return; }
    if (key==WUOS_KEY_STYLE_H3){ doc_apply_named_style(e, "Heading3"); return; }
    if (key==WUOS_KEY_STYLE_BODY){ doc_apply_named_style(e, "Body"); return; }
    if (key==WUOS_KEY_STYLE_QUOTE){ doc_apply_named_style(e, "Quote"); return; }
    if (key==WUOS_KEY_STYLE_CODE){ doc_apply_named_style(e, "Code"); return; }
    if (key==WUOS_KEY_PARA_PREV){ doc_move_para(e, -1); return; }
    if (key==WUOS_KEY_PARA_NEXT){ doc_move_para(e, +1); return; }
    /* DOC-54: jump to a TOC entry with Ctrl+[1..6] */
    if (key>=WUOS_KEY_TOC1 && key<=WUOS_KEY_TOC6){
        int idx = key - WUOS_KEY_TOC1;
        if (e->toc && idx < toc_count(e->toc)){
            int pg = toc_page(e->toc, idx);
            if (pg>0) e->jump_page = pg-1;
        }
        return;
    }
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

static void on_click(WuView *v, int x, int y){
    DocV *e = v->priv;
    /* DOC-60: hit-test the recorded link boxes; the first hit "opens" the
     * target (we surface it in the status / a toast via the epub_msg slot). */
    for (int i=0;i<e->nlink;i++){
        int bx=e->linkbox[i].x, by=e->linkbox[i].y;
        int bw=e->linkbox[i].w, bh=e->linkbox[i].h;
        if (x>=bx && x<=bx+bw && y>=by && y<=by+bh){
            free(e->epub_msg);
            const char *t = e->linkbox[i].target;
            size_t L = t?strlen(t):0;
            char *m = malloc(L+24); if (m){ snprintf(m, L+24, "link -> %s", t?t:""); }
            e->epub_msg = m;
            return;
        }
    }
}

static void destroy(WuView *v){
    DocV *e = v->priv;
    if (e->doc)  wubumodel_doc_destroy(e->doc);
    if (e->text) free(e->text);
    if (e->path) free(e->path);
    if (e->find_q) free(e->find_q);
    if (e->epub_msg) free(e->epub_msg);
    for (int i=0;i<e->nobj;i++) free(e->obj[i]);
    if (e->a11y_done) a11y_report_free(&e->a11y);
    toc_free(e->toc);
    if (e->r) wurender_destroy(e->r);
    free(e);
    free(v);
}

/* Thin view-level save hook: delegate to the doccmd module (DOC-76). */
static void doc_save(WuView *v){
    DocV *e = v ? v->priv : NULL;
    if (!e) return;
    free(e->epub_msg); e->epub_msg = doccmd_save(e->doc, e->path);
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
            /* DOC-76: open Word files into a real, round-trippable model so
             * save-as-DOCX preserves structure (headings/paragraphs/runs). */
            int is_docx = (strstr(path,".docx")!=NULL||strstr(path,".docm")!=NULL||strstr(path,".dotx")!=NULL);
            int is_odt  = (strstr(path,".odt")!=NULL||strstr(path,".fodt")!=NULL);
            int is_rtf  = (strstr(path,".rtf")!=NULL);
            int is_epub = (strstr(path,".epub")!=NULL||strstr(path,".epub3")!=NULL);
            if (is_docx){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_docx(path, &md) == 0 && md) e->doc = md;
            } else if (is_odt){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_odt(path, &md) == 0 && md) e->doc = md;
            } else if (is_rtf){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_rtf(path, &md) == 0 && md) e->doc = md;
            } else if (is_epub){
                wubumodel_doc *md = NULL;
                if (wubumodel_load_epub(path, &md) == 0 && md) e->doc = md;
            }
        }
        if (!e->doc){
            /* ingest via the real document facade (odt/pdf/html/...) */
            DocSession *s = doc_session_create();
            long id = doc_open(s, path);
            if (id >= 0){
                const char *dt = doc_text(s, id);
                if (dt){ free(e->text); e->text = strdup(dt); }
                doc_session_free(s);
            }
        }
    }
    if (!e->doc && !e->text) e->doc = wurender_sample_doc();
    if (!e->r || (!e->doc && !e->text)){ free(e->path); free(e->text); free(e); return NULL; }
    /* INT-1/3: the Document tab now consumes the chart/draw/math engines
     * (previously "SVG that goes nowhere", gap #13). Seed one sample chart so
     * the Insert path is exercised + visible immediately. */
    e->nobj = 0; e->a11y_done = 0; e->a11y.count = 0; e->a11y.items = NULL; e->a11y.cap = 0;
    e->toc = NULL; e->toc_dirty = 1; e->jump_page = -1;
    /* INT-1/3: seed one sample chart so the Insert path is exercised + visible. */
    { int w=0,h=0; unsigned char *fb=doccmd_insert_chart(wuos_svg_text,&w,&h);
      if (fb){ int i=e->nobj++; e->obj[i]=fb; e->objw[i]=w; e->objh[i]=h; } }
    WuView *v = calloc(1, sizeof *v);
    v->name = "Document";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = doc_render;
    v->status   = status;
    v->sidebar  = sidebar;
    v->on_key   = on_key;
    v->on_click = on_click;
    v->get_path = get_path;
    v->save    = doc_save;   /* DOC-76: Ctrl+S writes DOCX round-trip */
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
wubumodel_doc *wuos_doc_model(WuView *v){ return v ? ((DocV*)v->priv)->doc : NULL; }
/* Count of inserted overlay objects (chart/draw/math) currently displayed. */
int wuos_doc_obj_count(WuView *v){ return ((DocV*)v->priv)->nobj; }
/* EPUB export status string (caller must NOT free; view owns it), or NULL. */
const char *wuos_doc_epub_msg(WuView *v){ return ((DocV*)v->priv)->epub_msg; }
/* a11y issue count from the last check (0 if not run). */
int wuos_doc_a11y_issues(WuView *v){ return ((DocV*)v->priv)->a11y_done ? ((DocV*)v->priv)->a11y.count : -1; }
const char *wuos_doc_a11y_item(WuView *v, int i){
    DocV *e = (DocV*)v->priv;
    if (!e->a11y_done || i<0 || i>=e->a11y.count) return NULL;
    return e->a11y.items[i];
}
/* TOC entry count for the current render (DOC-54), or -1 if no model. */
int wuos_doc_toc_count(WuView *v){
    DocV *e = v->priv;
    if (!e->doc) return -1;
    if (!e->toc) e->toc = toc_build(e->doc, NULL, NULL);
    return toc_count(e->toc);
}
/* High-contrast setting (UXA-41) read from shared settings. */
int wuos_doc_high_contrast(WuView *v){
    (void)v;
    WubuSettings *s = wubusettings_shared();
    return s ? wubusettings_high_contrast(s) : 0;
}
/* Arg-bearing inserts driven by the shell's modal dialogs. */
int wuos_doc_insert_link_url(WuView *v, const char *url){
    DocV *e = v ? (DocV*)v->priv : NULL;
    if (!e || !e->doc) return 0;
    int ok = doccmd_insert_link_url(e->doc, url);
    if (ok) e->toc_dirty = 1;
    return ok;
}
int wuos_doc_insert_image_alt(WuView *v, const char *alt){
    DocV *e = v ? (DocV*)v->priv : NULL;
    if (!e || !e->doc) return 0;
    int ok = doccmd_insert_image_alt(e->doc, alt);
    if (ok) e->toc_dirty = 1;
    return ok;
}
int wuos_doc_insert_qr(WuView *v, const char *text){
    DocV *e = v ? (DocV*)v->priv : NULL;
    if (!e || !e->doc) return 0;
    int ok = doccmd_insert_qr(e->doc, text);
    if (ok) e->toc_dirty = 1;
    return ok;
}
/* Footnote/endnote count for the current model (DOC-55), or -1 if no model. */
int wuos_doc_footnote_count(WuView *v){
    DocV *e = v->priv;
    if (!e->doc) return -1;
    const char **out = NULL;
    int n = wubumodel_doc_notes(e->doc, &out);
    free(out);
    return n;
}
