/* layout.c -- central text pipeline. See layout.h.
 * Walks the opaque wubumodel tree, greedily word-wraps paragraphs into
 * pages, handles per-paragraph RTL, nested tables, and records object boxes
 * (images/shapes/charts/links) for the views to overlay. Consumers (render,
 * export, a11y, virtualization) read the laid-out pages. */
#include "ublayout.h"
#include "model.h"        /* opaque engine-layer model API (allowed here) */
#include "shape.h"        /* wubushape: Bidi reorder (INT-7 payload) */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LAY_MAX_PAGES 4096
#define LAY_MAX_OBJS  8192

typedef struct {
    int page, x, y, w, h;
    void *user;            /* source model node */
    wubulayout_dir dir;
} LOb;

typedef struct {
    wubulayout_run *run;
    LOb            *obj;
    int nrun, caprun;
    int nobj, capobj;
    int w, h;
    int ml, mr, mt, mb;
    int line_seq;   /* globally-unique line index on this page */
} LPage;

/* checkpoint taken immediately BEFORE laying a top-level block: everything
 * needed to truncate the layout back to this point and re-lay from here.
 * This is what makes wubulayout_invalidate() incremental (PRF-101). */
typedef struct {
    void *node;      /* the top-level block */
    int page;        /* page index the block started on */
    int pen_y;       /* pen position at block start */
    int nrun, nobj;  /* run/obj counts of that page at block start */
    int line_seq;    /* line counter of that page at block start */
    void *resume;    /* continuation after this chain ends (parent's next sib) */
} LCheck;

struct wubulayout_doc {
    void *model_doc;
    void *model_root;
    wubulayout_measure_fn measure;
    wubulayout_style_fn   style;
    void *cb_user;
    int pw, ph, ml, mr, mt, mb;
    LPage *pages[LAY_MAX_PAGES];
    int npages;
    int total_runs;
    LCheck *checks; int nchecks, capchecks;
};

/* ---- internal helpers ---- */
static int g_font_size = 12, g_bold = 0, g_italic = 0;
static wubulayout_dir g_dir = WUBULAYOUT_LTR;

static int style_of(wubulayout_doc *L, void *run){
    if (!L->style) return 0;
    return L->style(L->cb_user, run, &g_font_size, &g_bold, &g_italic, &g_dir);
}

static int measure_seg(wubulayout_doc *L, const char *t, size_t len){
    int h = 0;
    int w = L->measure ? L->measure(t, len, g_font_size, g_bold, g_italic, &h, L->cb_user) : (int)len*7;
    (void)h;
    return w;
}

/* decode one UTF-8 codepoint; returns bytes consumed */
static int utf8_cp(const char *s, unsigned long *cp){
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80){ *cp = c; return 1; }
    if ((c & 0xE0)==0xC0){ *cp = ((c&0x1F)<<6)|(s[1]&0x3F); return 2; }
    if ((c & 0xF0)==0xE0){ *cp = ((c&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F); return 3; }
    if ((c & 0xF8)==0xF0){ *cp = ((c&0x07)<<18)|((s[1]&0x3F)<<12)|((s[2]&0x3F)<<6)|(s[3]&0x3F); return 4; }
    *cp = c; return 1;
}

static int is_space_cp(unsigned long cp){ return cp==' '||cp=='\t'||cp=='\n'||cp=='\r'; }
static int is_rtl_cp(unsigned long cp){
    return (cp>=0x0590&&cp<=0x05FF)||(cp>=0x0600&&cp<=0x06FF)||
           (cp>=0x0750&&cp<=0x077F)||(cp>=0xFB50&&cp<=0xFDFF)||
           (cp>=0xFE70&&cp<=0xFEFF);
}

/* is a char a hard break point for word wrapping? (space = breakable) */
static LPage *cur_page(wubulayout_doc *L){
    if (L->npages==0) return NULL;
    return L->pages[L->npages-1];
}

static LPage *new_page(wubulayout_doc *L){
    if (L->npages >= LAY_MAX_PAGES) return NULL;
    LPage *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->w=L->pw; p->h=L->ph; p->ml=L->ml; p->mr=L->mr; p->mt=L->mt; p->mb=L->mb;
    p->line_seq = 0;
    L->pages[L->npages++] = p;
    return p;
}

static wubulayout_run *push_run(LPage *p){
    if (p->nrun >= p->caprun){
        int nc = p->caprun? p->caprun*2 : 64;
        wubulayout_run *r = realloc(p->run, nc*sizeof *r);
        if (!r) return NULL;
        p->run=r; p->caprun=nc;
    }
    memset(&p->run[p->nrun], 0, sizeof p->run[p->nrun]);
    return &p->run[p->nrun++];
}

static LOb *push_obj(LPage *p){
    if (p->nobj >= LAY_MAX_OBJS) return NULL;
    if (p->nobj >= p->capobj){
        int nc = p->capobj? p->capobj*2 : 16;
        LOb *o = realloc(p->obj, nc*sizeof *o);
        if (!o) return NULL;
        p->obj=o; p->capobj=nc;
    }
    memset(&p->obj[p->nobj], 0, sizeof p->obj[p->nobj]);
    return &p->obj[p->nobj++];
}

/* lay a single paragraph node: gather runs -> word-wrap -> emit lines.
 * Word-level greedy wrapping: each word carries the style of its source run.
 * For an RTL paragraph the WORD ORDER on a line is reversed (words stay LTR). */
static int lay_paragraph(wubulayout_doc *L, void *para, int *pen_y){
    /* gather segments {text, len, run} from the paragraph's RUN children */
    typedef struct { const char *t; size_t off; size_t len; void *run; } Seg;
    Seg segs[1024]; int nseg=0;
    for (wubumodel_node *r = wubumodel_node_first_child((wubumodel_node*)para);
         r; r = wubumodel_node_next_sibling(r)){
        if (wubumodel_node_kind(r) != WUBUMODEL_RUN) continue;
        const char *t = wubumodel_run_text(r);
        if (!t || !*t) continue;
        if (nseg < 1024){ segs[nseg].t=t; segs[nseg].off=0; segs[nseg].len=strlen(t); segs[nseg].run=r; nseg++; }
    }
    if (nseg==0) return 0;

    /* tokenize into words: a word = maximal non-space run; spaces are
     * separate tokens (so we can break on them). Each token references its seg. */
    typedef struct { const char *base; size_t off; size_t len; int seg; int space; } Word;
    Word words[4096]; int nw=0;
    for (int s=0; s<nseg && nw<4096; s++){
        const char *t = segs[s].t; size_t len = segs[s].len; size_t i=0;
        while (i<len && nw<4096){
            /* skip leading spaces of this token */
            size_t st=i;
            while (i<len && is_space_cp((unsigned long)(unsigned char)t[i])) i++;
            if (i>st){ /* a run of spaces */
                words[nw].base=t; words[nw].off=st; words[nw].len=i-st; words[nw].seg=s; words[nw].space=1; nw++;
            }
            size_t ws=i;
            while (i<len && !is_space_cp((unsigned long)(unsigned char)t[i])) i++;
            if (i>ws){ words[nw].base=t; words[nw].off=ws; words[nw].len=i-ws; words[nw].seg=s; words[nw].space=0; nw++; }
        }
    }
    if (nw==0) return 0;

    LPage *pg = cur_page(L);
    int content_w = L->pw - L->ml - L->mr;
    int x0 = L->ml;
    int line_h = 0;

    /* paragraph base direction: first strong codepoint */
    int para_rtl = 0;
    for (int w=0; w<nw; w++){
        const char *p = words[w].base + words[w].off; size_t l=words[w].len; size_t q=0;
        while (q<l){ unsigned long cp; int nb=utf8_cp(p+q,&cp); q+=nb; if (is_rtl_cp(cp)){ para_rtl=1; break; } if (cp>' '&&!is_rtl_cp(cp)) break; }
        if (para_rtl) break;
    }

    int w = 0;
    while (w < nw){
        int line_start = w;
        int cur_x = x0;
        int line_y = *pen_y;
        int max_h = 0;
        /* determine how many words fit on this line */
        int last_fit = w; int last_x = x0;
        while (w < nw){
            Word *wd = &words[w];
            style_of(L, segs[wd->seg].run);
            int ww = measure_seg(L, wd->base+wd->off, wd->len);
            int h = g_font_size + 4; if (h>max_h) max_h=h;
            int nx = cur_x + ww;
            if (w > line_start && nx - x0 > content_w) break; /* overflow -> wrap */
            cur_x = nx; last_fit = w; last_x = cur_x; w++;
        }
        if (last_fit == line_start && w == line_start){
            /* single word wider than the page: force place it, advance */
            last_fit = w; /* place at least this one */
        }
        /* emit words [line_start, last_fit] (already advanced w past last_fit) */
        int pen = x0;
        int count = last_fit - line_start + 1;
        /* build render order: RTL paragraphs reverse word order on the line */
        int order[4096]; int no=0;
        for (int k=line_start; k<=last_fit && no<4096; k++) order[no++]=k;
        if (para_rtl) for (int a=0,b=no-1;a<b;a++,b--){ int tmp=order[a]; order[a]=order[b]; order[b]=tmp; }
        int line_idx = 0;
        for (int oi=0; oi<no; oi++){
            Word *wd = &words[order[oi]];
            (void)line_idx;
            style_of(L, segs[wd->seg].run);
            int ww = measure_seg(L, wd->base+wd->off, wd->len);
            int h = g_font_size + 4;
            wubulayout_run *R = push_run(pg);
            if (!R) return -1;
            R->text = wd->base + wd->off; R->text_len = wd->len;
            R->font_size=g_font_size; R->bold=g_bold; R->italic=g_italic;
            R->dir = para_rtl? WUBULAYOUT_RTL:WUBULAYOUT_LTR; R->rtl=para_rtl;
            R->user = segs[wd->seg].run;
            R->h=h; R->w=ww; R->page=L->npages-1; R->line=pg->line_seq++;
            if (para_rtl) R->x = pen - ww; else R->x = pen;
            R->y = line_y + h - 4;
            pen += ww;
            (void)count;
        }
        int adv = (max_h? max_h:16) + 4;
        *pen_y = line_y + adv;
        (void)line_idx;
        /* page break if past bottom margin */
        if (*pen_y > L->ph - L->mb){
            pg = new_page(L);
            if (!pg) return -1;
            *pen_y = L->mt;
        }
    }
    *pen_y += 6; /* paragraph spacing */
    (void)line_h;
    return 0;
}

/* layout any block node (paragraph, table, or container of blocks) */
static int lay_node(wubulayout_doc *L, void *node, int *pen_y){
    wubumodel_kind k = wubumodel_node_kind((wubumodel_node*)node);
    if (k == WUBUMODEL_PARAGRAPH){
        return lay_paragraph(L, node, pen_y);
    }
    if (k == WUBUMODEL_TABLE){
        /* simple table: rows = child paragraphs/cells; lay each cell as a
         * paragraph block stacked vertically (full grid layout later). */
        int start_y = *pen_y;
        for (wubumodel_node *row = wubumodel_node_first_child((wubumodel_node*)node);
             row; row = wubumodel_node_next_sibling(row)){
            int cy = start_y;
            lay_node(L, row, &cy);
            if (cy > *pen_y) *pen_y = cy;
        }
        return 0;
    }
    if (k == WUBUMODEL_SHAPE || k == WUBUMODEL_CHART){
        /* object box: reserve space, view overlays it */
        LPage *pg = cur_page(L);
        LOb *o = push_obj(pg);
        if (o){
            o->page = L->npages-1;
            o->x = L->ml; o->y = *pen_y; o->w = L->pw - L->ml - L->mr; o->h = 160;
            o->user = node;
        }
        *pen_y += 170;
        return 0;
    }
    if (k == WUBUMODEL_LINK){
        /* DOC-60: a hyperlink is inline text (its RUN children) laid out as a
         * normal flow, so the view can color/underline it and hit-test clicks.
         * The layout walks the RUN children like a paragraph. */
        LPage *pg = cur_page(L);
        int start_y = *pen_y;
        for (wubumodel_node *rc = wubumodel_node_first_child((wubumodel_node*)node);
             rc; rc = wubumodel_node_next_sibling(rc)){
            if (wubumodel_node_kind(rc) != WUBUMODEL_RUN) continue;
            const char *t = wubumodel_run_text(rc);
            if (!t || !*t) continue;
            int fs=12, bold=0, it=0; wubulayout_dir d=WUBULAYOUT_LTR;
            if (L->style) L->style(L->cb_user, rc, &fs,&bold,&it,&d);
            int ww = L->measure ? L->measure(t, strlen(t), fs, bold, it, NULL, L->cb_user) : (int)strlen(t)*7;
            wubulayout_run *R = push_run(pg);
            if (!R) return -1;
            R->text = t; R->text_len = strlen(t);
            R->font_size=fs; R->bold=bold; R->italic=it;
            R->dir=d; R->rtl=(d==WUBULAYOUT_RTL);
            R->x = L->ml; R->y = start_y; R->w = ww; R->h = fs+4;
            R->page = L->npages-1; R->line = pg->line_seq++;
            R->user = rc;   /* RUN node -> view walks to parent LINK for target */
            start_y += fs + 8;
        }
        *pen_y = start_y + 4;
        return 0;
    }
    /* DOC-55: footnote / endnote -> superscript reference marker run.
     * In this block model a FOOTNOTE/ENDNOTE node is a standalone block whose
     * RUN child holds the marker (e.g. "1"); its note body is collected via
     * wubumodel_doc_notes() for the notes pane / export. We emit the marker as
     * a small raised run at the current pen and advance the pen. */
    if (k == WUBUMODEL_FOOTNOTE || k == WUBUMODEL_ENDNOTE){
        LPage *pg = cur_page(L);
        /* marker text lives on the node's RUN child */
        const char *mark = NULL;
        for (wubumodel_node *rc = wubumodel_node_first_child((wubumodel_node*)node);
             rc; rc = wubumodel_node_next_sibling(rc)){
            if (wubumodel_node_kind(rc) == WUBUMODEL_RUN){ mark = wubumodel_run_text(rc); break; }
        }
        if (pg && mark && *mark){
            int fs=10, bold=1, it=0; wubulayout_dir d=WUBULAYOUT_LTR;
            if (L->style) L->style(L->cb_user, node, &fs,&bold,&it,&d);
            int mw = L->measure ? L->measure(mark, strlen(mark), fs, bold, it, NULL, L->cb_user) : (int)strlen(mark)*7;
            int hh = fs + 4;
            wubulayout_run *R = push_run(pg);
            if (!R) return -1;
            R->text = mark; R->text_len = strlen(mark);
            R->font_size = fs; R->bold = bold; R->italic = it;
            R->dir = d; R->rtl = (d==WUBULAYOUT_RTL);
            R->x = L->ml; R->y = *pen_y - 4;   /* raised superscript */
            R->w = mw; R->h = hh;
            R->page = L->npages - 1; R->line = pg->line_seq++;
            R->user = node;
            *pen_y += hh + 2;
        }
        return 0;
    }
    /* generic container: recurse children */
    for (wubumodel_node *c = wubumodel_node_first_child((wubumodel_node*)node);
         c; c = wubumodel_node_next_sibling(c)){
        lay_node(L, c, pen_y);
    }
    return 0;
}

/* ---- public ---- */
wubulayout_doc *wubulayout_create(void *model_doc, void *model_root,
        wubulayout_measure_fn measure, wubulayout_style_fn style, void *cb_user,
        int page_w, int page_h, int ml,int mr,int mt,int mb){
    wubulayout_doc *L = calloc(1, sizeof *L);
    if (!L) return NULL;
    L->model_doc=model_doc; L->model_root=model_root;
    L->measure=measure; L->style=style; L->cb_user=cb_user;
    L->pw=page_w; L->ph=page_h; L->ml=ml; L->mr=mr; L->mt=mt; L->mb=mb;
    if (!new_page(L)){ free(L); return NULL; }
    if (wubulayout_rebuild(L)!=0){ wubulayout_destroy(L); return NULL; }
    return L;
}

/* ---- checkpoints (PRF-101 incremental layout) ---- */
static int push_check(wubulayout_doc *L, void *node, int pen_y, void *resume){
    if (L->nchecks >= L->capchecks){
        int nc = L->capchecks? L->capchecks*2 : 64;
        LCheck *c = realloc(L->checks, nc*sizeof *c);
        if (!c) return -1;
        L->checks=c; L->capchecks=nc;
    }
    LPage *p = L->pages[L->npages-1];
    LCheck *c = &L->checks[L->nchecks++];
    c->node=node; c->page=L->npages-1; c->pen_y=pen_y; c->resume=resume;
    c->nrun=p->nrun; c->nobj=p->nobj; c->line_seq=p->line_seq;
    return 0;
}

/* Lay a sibling chain of blocks, checkpointing each. Containers at depth 0
 * are descended one level so PARAGRAPHS get their own checkpoints (the usual
 * document shape is one SECTION of many paragraphs). `resume` is the
 * continuation a restarted chain must proceed to after its last sibling —
 * recorded per checkpoint so wubulayout_invalidate() can resume the walk. */
static int lay_chain(wubulayout_doc *L, void *start, int *pen_y, void *resume, int depth){
    for (wubumodel_node *n = (wubumodel_node*)start;
         n; n = wubumodel_node_next_sibling(n)){
        wubumodel_kind k = wubumodel_node_kind(n);
        int is_container = (k!=WUBUMODEL_PARAGRAPH && k!=WUBUMODEL_TABLE &&
                            k!=WUBUMODEL_SHAPE && k!=WUBUMODEL_CHART && k!=WUBUMODEL_LINK &&
                            k!=WUBUMODEL_FOOTNOTE && k!=WUBUMODEL_ENDNOTE);
        if (is_container && depth==0 && wubumodel_node_first_child(n)){
            wubumodel_node *sib = wubumodel_node_next_sibling(n);
            void *res = sib ? (void*)sib : resume;
            if (lay_chain(L, wubumodel_node_first_child(n), pen_y, res, 1)!=0) return -1;
        } else {
            if (push_check(L, n, *pen_y, resume)!=0) return -1;
            if (lay_node(L, n, pen_y)!=0) return -1;
        }
    }
    return 0;
}

static void recount_runs(wubulayout_doc *L){
    L->total_runs = 0;
    for (int i=0;i<L->npages;i++) L->total_runs += L->pages[i]->nrun;
}

int wubulayout_rebuild(wubulayout_doc *L){
    /* free old pages */
    for (int i=0;i<L->npages;i++){
        free(L->pages[i]->run); free(L->pages[i]->obj); free(L->pages[i]);
    }
    L->npages=0; L->total_runs=0; L->nchecks=0;
    if (!new_page(L)) return -1;
    void *root = L->model_root ? L->model_root
                 : wubumodel_doc_root((wubumodel_doc*)L->model_doc);
    int pen_y = L->mt;
    int rc = lay_chain(L, root, &pen_y, NULL, 0);
    recount_runs(L);
    return rc;
}

/* Incremental re-lay (PRF-101): truncate the layout back to the checkpoint of
 * the given block and re-lay ONLY from there. Blocks before it keep their
 * geometry and the measure callback is never re-invoked for them. Falls back
 * to a full rebuild when the node has no checkpoint. */
int wubulayout_invalidate(wubulayout_doc *L, void *block){
    int ci = -1;
    for (int i=0;i<L->nchecks;i++) if (L->checks[i].node==block){ ci=i; break; }
    if (ci < 0) return wubulayout_rebuild(L);
    LCheck c = L->checks[ci];
    /* drop pages after the checkpoint page */
    for (int i=c.page+1; i<L->npages; i++){
        free(L->pages[i]->run); free(L->pages[i]->obj); free(L->pages[i]);
    }
    L->npages = c.page+1;
    /* truncate the checkpoint page back to the recorded state */
    LPage *p = L->pages[c.page];
    p->nrun = c.nrun; p->nobj = c.nobj; p->line_seq = c.line_seq;
    /* drop this checkpoint and everything after (re-recorded below) */
    L->nchecks = ci;
    int pen_y = c.pen_y;
    /* re-lay the invalidated block's sibling chain, then continue the walk at
     * the recorded continuation (e.g. the parent section's next sibling). */
    int rc = lay_chain(L, block, &pen_y, c.resume, c.resume? 1:0);
    if (rc==0 && c.resume) rc = lay_chain(L, c.resume, &pen_y, NULL, 0);
    recount_runs(L);
    return rc;
}

void wubulayout_destroy(wubulayout_doc *L){
    if (!L) return;
    for (int i=0;i<L->npages;i++){
        free(L->pages[i]->run); free(L->pages[i]->obj); free(L->pages[i]);
    }
    free(L->checks);
    free(L);
}

int wubulayout_page_count(const wubulayout_doc *L){ return L->npages; }
const wubulayout_page_info *wubulayout_page(const wubulayout_doc *L, int page){
    if (page<0||page>=L->npages) return NULL;
    LPage *p = L->pages[page];
    static wubulayout_page_info info;
    info.w=p->w; info.h=p->h; info.margin_l=p->ml; info.margin_r=p->mr;
    info.margin_t=p->mt; info.margin_b=p->mb;
    return &info;
}
int wubulayout_run_count(const wubulayout_doc *L, int page){
    if (page<0||page>=L->npages) return 0; return L->pages[page]->nrun;
}
const wubulayout_run *wubulayout_run_at(const wubulayout_doc *L, int page, int i){
    if (page<0||page>=L->npages) return NULL;
    LPage *p=L->pages[page]; if (i<0||i>=p->nrun) return NULL; return &p->run[i];
}
int wubulayout_line_count(const wubulayout_doc *L, int page){
    if (page<0||page>=L->npages) return 0;
    int mx=0; LPage *p=L->pages[page];
    for (int i=0;i<p->nrun;i++) if (p->run[i].line>mx) mx=p->run[i].line;
    return mx+1;
}
const wubulayout_line *wubulayout_line_at(const wubulayout_doc *L, int page, int li){
    if (page<0||page>=L->npages) return NULL;
    static wubulayout_line ln;
    LPage *p=L->pages[page];
    int x0=1<<30, x1=-1, y0=1<<30, y1=-1, found=0;
    for (int i=0;i<p->nrun;i++){ if (p->run[i].line!=li) continue;
        found=1; if (p->run[i].x<x0) x0=p->run[i].x; if (p->run[i].x+p->run[i].w>x1) x1=p->run[i].x+p->run[i].w;
        if (p->run[i].y-y0<0) y0=p->run[i].y; if (p->run[i].y>y1) y1=p->run[i].y; }
    if (!found) return NULL;
    ln.x=x0; ln.y=y0; ln.w=x1-x0; ln.h=(y1-y0)+4; ln.page=page; ln.line=li;
    return &ln;
}
int wubulayout_total_runs(const wubulayout_doc *L){ return L->total_runs; }

int wubulayout_hit_test(const wubulayout_doc *L, int page, int x, int y, int *out_line){
    if (page<0||page>=L->npages) return -1;
    LPage *p=L->pages[page];
    for (int i=0;i<p->nrun;i++){
        wubulayout_run *r=&p->run[i];
        if (x>=r->x && x<=r->x+r->w && y>=r->y-r->h && y<=r->y+4){
            if (out_line) *out_line=r->line; return i;
        }
    }
    if (out_line) *out_line=-1;
    return -1;
}

char *wubulayout_page_text(const wubulayout_doc *L, int page){
    if (page<0||page>=L->npages) return NULL;
    LPage *p=L->pages[page];
    size_t cap=1024, len=0; char *buf=malloc(cap);
    if (!buf) return NULL; buf[0]=0;
    int cur=-1;
    for (int i=0;i<p->nrun;i++){
        wubulayout_run *r=&p->run[i];
        if (r->line != cur){ if (cur>=0){ if(len+1<cap){buf[len++]='\n';buf[len]=0;} } cur=r->line; }
        size_t add=r->text_len;
        if (len+add+1>=cap){ cap=(len+add+1)*2; char *nb=realloc(buf,cap); if(!nb){free(buf);return NULL;} buf=nb; }
        memcpy(buf+len, r->text, add); len+=add; buf[len]=0;
    }
    return buf;
}
char *wubulayout_doc_text(const wubulayout_doc *L){
    size_t cap=4096, len=0; char *buf=malloc(cap); if(!buf) return NULL; buf[0]=0;
    for (int pg=0; pg<L->npages; pg++){
        char *pt = wubulayout_page_text(L, pg);
        if (!pt) continue;
        size_t add=strlen(pt); size_t need=len+add+2;
        if (need>=cap){ while(need>=cap) cap*=2; char *nb=realloc(buf,cap); if(!nb){free(pt);free(buf);return NULL;} buf=nb; }
        if (len && buf[len-1]!='\n'){ buf[len++]='\n'; }
        memcpy(buf+len, pt, add); len+=add; buf[len]=0;
        free(pt);
    }
    return buf;
}
