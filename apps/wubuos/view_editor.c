/* view_editor.c -- Notepad++-parity editor view (WuBuPad core + lexer).
 *
 * Hosts the REAL WuBuPad document model (piece-table buffer, undo/redo,
 * column selection) and a real lexer for syntax highlighting. Renders the
 * text with a blinking caret, line numbers, and per-token coloring. Typing,
 * arrows, backspace/return, Home/End, PageUp/PageDn all edit the live Doc.
 *
 * This is genuine Notepad++-class editing, not a mockup -- it is the same
 * engine WuBuPad ships, embedded into the unified office shell.
 */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_file.h"
#include "findbar.h" /* opaque find/replace engine (extracted from this file) */
#include "autocomp.h" /* opaque auto-completion engine (extracted from this file) */
#include "bkmk.h"     /* opaque line-bookmark set (extracted from this file) */
#include "codefold.h"  /* opaque code-folding + function-list (extracted) */
#include "macro.h"     /* opaque macro record/playback (extracted) */
#include "gotoline.h"  /* opaque go-to-line prompt (extracted) */

#include "doc.h"    /* cross-repo: ~/WuBuPad/src */
#include "lex.h"
#include "encode.h" /* WuBuPad encoding detect */
#include "docs.h"   /* WuBuPad multi-document session (DONE engine) */
#include "autosave.h" /* wubuautosave: crash-recovery (INT-2 P0) */
#include "spell.h"    /* wubuspell: live red-squiggle (INT-8 P0) */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>

/* forward decls: wubuautosave bridge helpers (defined below wuos_editor_create) */
static wubumodel_doc *editor_doc_to_model(const void *d);
static char *editor_model_text(const wubumodel_doc *m);

typedef struct {
    Doc  *doc;
    Lex  *lex;
    char *path;       /* loaded file, or NULL */
    int   top;        /* first visible line */
    int   caret_line, caret_col;
    int   blink;      /* caret phase */
    int   frames;     /* for blink timing */

    /* ---- find / replace ----
     * Search engine state (query/replace/matches/regex) lives in the opaque
     * FindBar (findbar.h); the editor keeps only its own panel UI state:
     * which panel is open (find_mode), which field is focused (find_focus),
     * and the transient message timer (find_msg_t). */
    FindBar *fb;       /* opaque search engine (see findbar.h) */
    int   find_mode;  /* 0 none, 1 find, 2 replace */
    int   find_focus; /* in replace mode: 0=find field, 1=replace field */
    int   find_msg_t; /* frames remaining to show msg */

    /* ---- go to line (Ctrl+G): delegated to opaque GotoLine engine ---- */
    GotoLine *gto;

    /* ---- EOL + encoding (Notepad++ parity) ---- */
    int   eol_crlf;        /* 0 = LF, 1 = CRLF */
    const char *enc_label; /* detected encoding label, or NULL */
    int   dark;            /* 0 = light, 1 = dark theme */

    /* multi-document session (DONE engine: src/docs) */
    Docs *docs;

    /* bookmarks (Notepad++ line ops): delegated to opaque BkMk engine */
    BkMk *bk;

    /* column / block selection mode */
    int   col_mode;          /* 0 = stream, 1 = column */
    int   sel_l0, sel_c0;    /* anchor line/col */
    int   sel_l1, sel_c1;    /* active line/col */

    /* macro record/play (Notepad++-style): delegated to opaque Macro engine */
    Macro *macro;

    /* auto-completion popup: delegated to the opaque AutoComp engine */
    AutoComp *ac;           /* owns candidate list + selection (see autocomp.h) */

    /* code folding + function-list panel: delegated to opaque CodeFold engine */
    CodeFold *cf;

    /* crash-recovery autosave (INT-2 P0: was never attached to any app) */
    Autosave *asv;
    int   asv_tick;          /* frame counter for periodic snapshot */
    size_t asv_len;          /* last seen doc length (detect edits) */
    /* live spell-check (INT-8 P0: was never linked by any app) */
    SpellDict *spd;
} Editor;
static unsigned char g_def_r=36, g_def_g=41, g_def_b=47;  /* theme-aware default */

/* macro_play callback: re-dispatch a recorded op through on_key so playback
 * exercises the exact same editing path as a live key press. */
static void on_key(WuView *v, int key, int down);  /* forward: defined below */
static void editor_macro_replay_op(int opcode, unsigned char ch, void *ctx){
    WuView *v = ctx;
    if (!v) return;
    if (opcode == MACRO_OP_CHAR) on_key(v, (int)ch, 1);
    else if (opcode == MACRO_OP_RETURN) on_key(v, WUOS_KEY_RETURN, 1);
    else if (opcode == MACRO_OP_BACKSPACE) on_key(v, WUOS_KEY_BACKSPACE, 1);
}
static void tok_color(LexTok k, unsigned char *r, unsigned char *g, unsigned char *b){
    switch (k){
        case TK_KEYWORD:  *r=86;  *g=156; *b=214; break;   /* blue */
        case TK_TYPE:     *r=78;  *g=201; *b=176; break;   /* teal */
        case TK_STRING:   *r=152; *g=195; *b=121; break;   /* green */
        case TK_CHAR:     *r=209; *g=154; *b=102; break;   /* orange */
        case TK_NUMBER:   *r=181; *g=206; *b=168; break;   /* light green */
        case TK_COMMENT:  *r=128; *g=128; *b=128; break;   /* grey */
        case TK_PREPROC:  *r=215; *g=186; *b=125; break;   /* tan */
        case TK_OPERATOR:
        case TK_PUNCT:    *r=120; *g=120; *b=130; break;   /* slate */
        default:          *r=g_def_r; *g=g_def_g; *b=g_def_b; break;   /* theme default */
    }
}

static void save(WuView *v);          /* forward decl (used by on_key) */
static void find_close(WuView *v);    /* forward decl */

/* ---- find / replace now delegate to the opaque FindBar engine (findbar.c) ----
 * The editor keeps only the UI-mode booleans; all search state lives in e->fb. */

static void find_close(WuView *v){
    Editor *e = v->priv;
    e->find_mode = 0;
    if (e->fb) findbar_clear_active(e->fb);
    doc_set_selection(e->doc, doc_cursor(e->doc), doc_cursor(e->doc)); /* clear */
}

/* Open find (mode=1) or replace (mode=2). */
static void find_open(WuView *v, int mode){
    Editor *e = v->priv;
    if (e->find_mode != mode){ e->find_mode = mode; e->find_focus = 0; }
    e->frames = 0;
}

/* Byte offset of the start of 1-based line `lineN` (clamped to last line). */
static size_t doc_offset_of_line(Doc *d, int lineN){
    char *t = doc_text(d);
    size_t n = doc_length(d);
    int cur = 1;
    if (lineN <= 1){ free(t); return 0; }
    for (size_t q=0; q<n; q++){
        if (t[q]=='\n'){ cur++; if (cur == lineN){ free(t); return q+1; } }
    }
    free(t);
    /* lineN beyond end: return end of doc */
    return n;
}

/* Detect current EOL style from text (1 if any CRLF present, else 0=LF). */
static int detect_eol(const char *t){
    if (!t) return 0;
    for (size_t q=0; t[q]; q++) if (t[q]=='\r' && t[q+1]=='\n') return 1;
    return 0;
}

/* Convert the whole document between LF and CRLF in place. */
static void convert_eol(Editor *e, int to_crlf){
    if (e->eol_crlf == to_crlf) return;
    char *t = doc_text(e->doc);
    size_t n = doc_length(e->doc);
    /* worst case: every char is '\n' -> doubles (+1 for NUL) */
    char *out = malloc(n*2 + 1);
    if (!out){ free(t); return; }
    size_t o=0;
    for (size_t q=0; q<n; q++){
        if (to_crlf && t[q]=='\n' && (q==0 || t[q-1]!='\r')) out[o++]='\r';
        else if (!to_crlf && t[q]=='\r' && t[q+1]=='\n') continue; /* drop \r */
        out[o++] = t[q];
    }
    out[o]=0;
    doc_replace(e->doc, 0, n, out);
    free(out); free(t);
    e->eol_crlf = to_crlf;
}

/* Point e->doc at the active document in the session and refresh EOL/encoding
 * from its current text. Called after any doc switch. */
static void editor_sync_active(Editor *e){
    if (!e->docs) return;
    size_t a = docs_active(e->docs);
    e->doc = docs_doc(e->docs, a);
    if (e->doc){
        char *t = doc_text(e->doc);
        e->eol_crlf = detect_eol(t);
        free(t);
    }
    const char *p = docs_path(e->docs, a);
    e->path = (p && *p)? (char*)p : NULL;   /* docs owns the string; we only read */
    e->enc_label = "UTF-8";
}

/* ---- session save/restore (Notepad++ session model) ---- */
static const char *session_path(void){
    const char *e = getenv("WUBUOS_SESSION");
    return e && *e ? e : "~/.wubuos_session";
}
/* write the open document list (one path per line, "(untitled)" for none) */
static void session_save(Editor *e){
    if (!e->docs) return;
    char *fn = strdup(session_path());
    /* expand leading ~ */
    if (fn[0]=='~'){ const char *home=getenv("HOME"); size_t hl=home?strlen(home):0;
        char *exp=malloc(hl+strlen(fn)+2); if(home) strcpy(exp,home); strcat(exp, fn+1);
        free(fn); fn=exp; }
    FILE *f = fopen(fn, "w");
    if (f){
        size_t n = docs_count(e->docs);
        for (size_t i=0;i<n;i++){
            const char *p = docs_path(e->docs, i);
            fprintf(f, "%s\n", (p && *p)? p : "(untitled)");
        }
        fclose(f);
    }
    free(fn);
}
/* reopen the saved document list (called when launching with no file arg) */
static void session_restore(Editor *e){
    char *fn = strdup(session_path());
    if (fn[0]=='~'){ const char *home=getenv("HOME"); size_t hl=home?strlen(home):0;
        char *exp=malloc(hl+strlen(fn)+2); if(home) strcpy(exp,home); strcat(exp, fn+1);
        free(fn); fn=exp; }
    FILE *f = fopen(fn, "r");
    free(fn);
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof line, f)){
        size_t L = strlen(line); while (L>0 && (line[L-1]=='\n'||line[L-1]=='\r')) line[--L]=0;
        if (!*line) continue;
        if (!strcmp(line, "(untitled)")) docs_open(e->docs, NULL, "", "c");
        else docs_open(e->docs, line, "", "c");
    }
    fclose(f);
}

/* current caret line (0-based) */
static int editor_line_of(Editor *e){
    char *t = doc_text(e->doc);
    size_t cur = doc_cursor(e->doc), ln = 0;
    for (size_t q=0; q<cur; q++) if (t[q]=='\n') ln++;
    free(t);
    return (int)ln;
}
/* current caret column (0-based, in chars) */
static int editor_col_of(Editor *e){
    char *t = doc_text(e->doc);
    size_t cur = doc_cursor(e->doc), c = 0;
    while (cur>0 && t[cur-1]!='\n'){ cur--; c++; }
    free(t);
    return (int)c;
}
/* move caret up/down `dl` lines, keeping the same char column */
static void editor_caret_vert(Editor *e, int dl){
    char *t = doc_text(e->doc);
    size_t n = doc_length(e->doc);
    int cl = editor_line_of(e), cc = editor_col_of(e);
    cl += dl; if (cl<0) cl=0;
    /* find start of line cl */
    size_t p=0; int ln=0;
    while (ln<cl && p<n){ if (t[p]=='\n') ln++; p++; }
    /* advance cc chars (or to EOL) */
    size_t q=p; int c=0;
    while (q<n && t[q]!='\n' && c<cc){ q++; c++; }
    doc_set_cursor(e->doc, q);
    free(t);
}

/* ---- auto-completion (collect identifiers from doc + builtins) ---- */
/* ---- code-folding + function-list now delegate to opaque CodeFold (codefold.c) ---- */
static void fold_toggle_block(Editor *e){
    if (e->cf) codefold_toggle_block(e->cf, e->doc, editor_line_of(e));
}
static void sym_toggle(Editor *e){ if (e->cf) codefold_sym_toggle(e->cf); }

static void bk_jump(Editor *e, int dir){   /* dir +1 next, -1 prev */
    if (!e->bk) return;
    int cl = editor_line_of(e);
    int best = bkmk_jump(e->bk, cl, dir);
    if (best < 0) return;
    size_t off = doc_offset_of_line(e->doc, best+1);
    doc_set_cursor(e->doc, off);
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    Editor *e = v->priv;
    int fh = wuos_font_height();
    int lh = fh + 6;
    (void)scroll;
    int H = h;
    unsigned char *fb = malloc((size_t)w*H*4);
    if (!fb) return -1;
    /* theme: dark mode flips background/text/gutter */
    unsigned char bg_r=255,bg_g=255,bg_b=255;
    unsigned char gut_r=238,gut_g=240,gut_b=244;
    unsigned char sepr=210,sepg=214,sepb=220;
    unsigned char def_r=36,def_g=41,def_b=47;        /* default token text */
    unsigned char num_r=120,num_g=124,num_b=130;     /* line numbers */
    if (e->dark){
        bg_r=30;bg_g=33;bg_b=40;
        gut_r=40;gut_g=43;gut_b=50;
        sepr=64;sepg=68;sepb=76;
        def_r=200;def_g=203;def_b=210;
        num_r=120;num_g=124;num_b=132;
    }
    g_def_r = def_r; g_def_g = def_g; g_def_b = def_b;
    for (int i=0;i<w*H;i++){ fb[i*4]=bg_r; fb[i*4+1]=bg_g; fb[i*4+2]=bg_b; fb[i*4+3]=255; }
    e->top = 0;

    /* gutter */
    int gutter = 52;
    for (int y=0;y<H;y++) for (int x=0;x<gutter;x++){ size_t i=((size_t)y*w+x)*4; fb[i]=gut_r;fb[i+1]=gut_g;fb[i+2]=gut_b; }
    for (int y=0;y<H;y++){ size_t i=((size_t)y*(w)+gutter)*4; fb[i]=sepr;fb[i+1]=sepg;fb[i+2]=sepb; }

    /* ---- empty-state hint (UI-31): new/blank doc shows a friendly prompt ---- */
    if (e->doc && doc_length(e->doc) == 0 && !e->find_mode && !gotoline_active(e->gto)){
        const char *hint = "New document - start typing, or press Ctrl+K for commands";
        int hw = (int)wuos_font_draw(hint, 0,0, 0, 0,0,0, NULL,0,0);
        int hx = (w - hw)/2 > gutter+10 ? (w - hw)/2 : gutter+10;
        int hy = H/2 - fh/2;
        wuos_font_draw(hint, hx, hy, 0, num_r, num_g, num_b, fb, w, H);
    }

    /* ---- document tab strip (multi-doc) ---- */
    int dofst = 0;
    if (e->docs && docs_count(e->docs) > 1){
        dofst = 22;
        int dx = 0; size_t n = docs_count(e->docs), act = docs_active(e->docs);
        for (size_t di=0; di<n; di++){
            const char *dp = docs_path(e->docs, di);
            const char *nm = (dp && *dp)? dp : "untitled";
            const char *bn = strrchr(nm, '/'); if (bn) nm = bn+1;
            char lab[64]; snprintf(lab,sizeof lab," %s ", nm);
            int tw = (int)strlen(lab)*9 + 12;
            int on = (di==act);
            for (int yy=0; yy<dofst; yy++) for (int xx=dx; xx<dx+tw && xx<w; xx++){
                size_t i=((size_t)yy*w+xx)*4;
                if (on){ fb[i]=230;fb[i+1]=235;fb[i+2]=245; } else { fb[i]=gut_r;fb[i+1]=gut_g;fb[i+2]=gut_b; }
            }
            wuos_font_draw(lab, dx+6, dofst-6, 0, on?20:80, on?24:90, on?30:90, fb,w,H);
            dx += tw;
        }
    }

    char *text = doc_text(e->doc);
    size_t tlen = text? strlen(text):0;

    int y = 6 + dofst;
    size_t pos = 0, line = 0;
    size_t line_start = 0;
    size_t caret = doc_cursor(e->doc);
    e->caret_line = 0; e->caret_col = 0;
    { size_t cl=0, cc=0; for (size_t p=0;p<caret;p++){ if (text && text[p]=='\n'){cl++;cc=0;} else cc++; } e->caret_line=cl; e->caret_col=cc; }

    LexSpan spans[256];
    while (y < H - lh){
        /* skip hidden (folded) body lines entirely */
        if (e->cf && codefold_hidden(e->cf, (int)line)){
            while (pos < tlen && text[pos] != '\n') pos++;
            line++;
            if (pos < tlen){ pos++; line_start = pos; }
            else break;
            continue;
        }
        char num[16]; snprintf(num,sizeof num,"%zu",line+1);
        wuos_font_draw(num, 6, y+fh, 0, num_r,num_g,num_b, fb, w, H);
        /* fold marker: ▾ on a header line whose body is hidden */
        if (e->cf && codefold_hidden(e->cf, (int)(line+1))){
            wuos_font_draw("v", 30, y+fh, 0, 120,200,140, fb, w, H);  /* 'v' glyph as ▾ */
        }
        /* bookmark marker (cyan disc) in the gutter */
        if (e->bk && bkmk_has(e->bk, (int)line)){
            int cx=30, cy=y+fh, cr=4;
            for (int dy=-cr; dy<=cr; dy++) for (int dx=-cr; dx<=cr; dx++)
                if (dx*dx+dy*dy <= cr*cr){ int px=cx+dx, py=cy+dy; if(px>=0&&px<gutter&&py>=0&&py<H){ size_t ii=((size_t)py*w+px)*4; fb[ii]=80;fb[ii+1]=200;fb[ii+2]=220; } }
            break;
        }
        size_t le = pos;
        while (pos < tlen && text[pos] != '\n'){ pos++; }
        le = pos;

        /* ---- find match highlight (behind tokens) ---- */
        if (e->fb && findbar_active(e->fb)){
            size_t m0 = 0, m1 = 0; findbar_match(e->fb, &m0, &m1);
            if (m1 > line_start && m0 < le){
                size_t h0 = m0 < line_start ? line_start : m0;
                size_t h1 = m1 > le ? le : m1;
                (void)h1;
                char pre[512]; size_t pl=0;
                for (size_t q=line_start; q<h0 && pl<511; q++) pre[pl++]=text[q];
                pre[pl]=0;
                int x0 = gutter+6 + wuos_font_draw(pre, gutter+6, y+fh, 0, 0,0,0, NULL,0,0);
                int wseg = wuos_font_draw(text+h0, gutter+6, y+fh, 0, 0,0,0, NULL,0,0);
                (void)wseg;
                for (int yy=y-2; yy<y+lh-2; yy++) for (int xx=x0; xx<x0+wseg && xx<w; xx++){
                    if (xx>=0 && yy>=0){
                        size_t i=((size_t)yy*w+xx)*4;
                        fb[i]=255; fb[i+1]=238; fb[i+2]=120;
                    }
                }
            }
        }

        /* syntax highlight: lex the line, paint each span */
        size_t nsp = 0;
        if (e->lex && le > line_start)
            nsp = lex_run(e->lex, text+line_start, le-line_start, spans, 256);
        size_t sp = 0;
        size_t col = line_start;
        while (col < le){
            LexTok k = TK_TEXT;
            size_t seg_end = le;
            if (sp < nsp){ k = spans[sp].kind; seg_end = line_start + spans[sp].end; sp++; }
            unsigned char cr,cg,cb; tok_color(k, &cr,&cg,&cb);
            /* draw this token span */
            char seg[512]; size_t sl=0;
            for (size_t q=col; q<seg_end && sl<511; q++){ seg[sl++]=text[q]; }
            seg[sl]=0;
            wuos_font_draw(seg, gutter+6, y+fh, 0, cr,cg,cb, fb, w, H);
            col = seg_end;
        }

        /* caret */
        if ((int)line == e->caret_line && (e->frames/30)%2==0){
            int cx = gutter + 6;
            char pre[256]; size_t pl=0;
            size_t cp = line_start;
            while (cp < le && pl<255){ pre[pl++]=text[cp]; if (cp==caret) break; cp++; }
            pre[pl]=0;
            cx += wuos_font_draw(pre, gutter+6, y+fh, 0, 0,0,0, NULL, 0, 0); /* measure */
            for (int yy=y; yy<y+lh-2; yy++) for (int xx=cx; xx<cx+2; xx++){
                if (xx>=0&&yy>=0&&xx<w&&yy<H){ size_t i=((size_t)yy*w+xx)*4; fb[i]=20;fb[i+1]=20;fb[i+2]=20; }
            }
        }

        line++;
        if (pos < tlen){ pos++; line_start = pos; }
        else break;

        /* column / block selection overlay (Notepad++ Alt+drag analogue) */
        if (e->col_mode && e->sel_l1 >= e->sel_l0){
            int lo = e->sel_l0, hi = e->sel_l1, c0 = e->sel_c0, c1 = e->sel_c1;
            if (c1 < c0){ int t=c0; c0=c1; c1=t; }
            if ((int)line-1 >= lo && (int)line-1 <= hi){
                int x0 = gutter + c0*9, x1 = gutter + c1*9;
                for (int yy=y; yy<y+lh; yy++) for (int xx=x0; xx<x1 && xx<w; xx++){
                    if (xx>=0&&yy>=0){ size_t i=((size_t)yy*w+xx)*4;
                        fb[i]=(fb[i]+60)>>1; fb[i+1]=(fb[i+1]+120)>>1; fb[i+2]=(fb[i+2]+180)>>1; }
                }
            }
        }

        y += lh;
    }

    /* INT-8 P0: live spell-check red squiggle. Walk the buffer with the same
     * proportional metrics the editor uses; under misspelled words draw a red
     * zigzag. Only paints words on currently-visible lines. */
    if (e->spd && text){
        int top = 6 + dofst;
        int x = gutter + 6;
        int ln = 0;
        /* small state machine: accumulate a word, check, draw squiggle */
        const char *word = NULL; int wlen = 0;
        for (size_t i=0;i<=tlen;i++){
            char c = (i<tlen)? text[i] : '\n';
            int isword = ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='\''||(unsigned char)c>=128);
            if (isword){
                if (!word) word = text+i;
                wlen++;
            } else {
                if (word && wlen>1){
                    char buf[256]; size_t k=0;
                    for (int q=0;q<wlen && k<255;q++) buf[k++]=word[q];
                    buf[k]=0;
                    if (!spell_check(e->spd, buf)){
                        int sy = top + ln*lh + fh + 2;
                        if (sy < H){
                            for (int xx=x-wlen*0; xx < x; xx+=2){
                                int yy = sy + ((xx/2)%2? 1:0);
                                if (xx>=0 && xx<w && yy>=0 && yy<H){
                                    size_t ii=((size_t)yy*w+xx)*4;
                                    fb[ii]=220; fb[ii+1]=40; fb[ii+2]=40;
                                }
                            }
                        }
                    }
                }
                word=NULL; wlen=0;
            }
            if (c=='\n'){ ln++; x = gutter+6; }
            else if (c!='\t'){ x += wuos_font_draw(&c, 0, 0, 0, 0,0,0, NULL,0,0); }
            else { x += 4*9; }
        }
    }
    free(text);

    /* ---- function-list panel (right gutter) ---- */
    if (e->cf && codefold_symmode(e->cf)){
        char *st = doc_text(e->doc);
        size_t sl = doc_length(e->doc);
        LexSym syms[256];
        size_t ns = lex_symbols(st, sl, syms, 256);
        free(st);
        int pw = 220, px = w - pw;
        for (int yy=0; yy<H; yy++) for (int xx=px; xx<w; xx++){ size_t i=((size_t)yy*w+xx)*4; fb[i]=238;fb[i+1]=240;fb[i+2]=244; }
        for (int xx=px; xx<w; xx++){ size_t i=((size_t)px*w+xx)*4; fb[i]=140;fb[i+1]=144;fb[i+2]=152; }
        char title[64]; snprintf(title,sizeof title,"Functions (%zu)", ns);
        wuos_font_draw(title, px+8, 4+fh, 0, 60,64,72, fb,w,H);
        for (size_t k=0; k<ns && k<60; k++){
            char nm[64]; size_t L = syms[k].name_len; if (L>63) L=63;
            char *dt = doc_text(e->doc); memcpy(nm, dt+syms[k].name_off, L); nm[L]=0; free(dt);
            char row[96]; snprintf(row,sizeof row,"%s : L%zu", nm, syms[k].line+1);
            wuos_font_draw(row, px+8, 4+fh + (int)(k+1)*lh, 0, 30,34,42, fb,w,H);
        }
    }

    e->frames++;
    if (e->find_msg_t > 0) e->find_msg_t--;

    /* INT-2 P0: periodic crash-recovery snapshot. Detect edits by doc length
     * change (catches typing/paste/replace/delete uniformly), then tick. */
    if (e->asv){
        size_t len = doc_length(e->doc);
        if (len != e->asv_len){ wubuautosave_mark_dirty(e->asv); e->asv_len = len; }
        e->asv_tick++;
        if (e->asv_tick >= 60){ e->asv_tick = 0;
            wubumodel_doc *m = editor_doc_to_model(e->doc);
            if (m){ wubuautosave_tick(e->asv, m); wubumodel_doc_destroy(m); }
        }
    }

    /* ---- find bar (drawn over the bottom of the buffer) ---- */
    if (e->find_mode && e->fb){
        const char *q = findbar_query(e->fb);
        const char *r = findbar_replace(e->fb);
        int wq = (int)strlen(q), wr = (int)strlen(r);
        if (wq > 80) wq = 80;   /* keep combined label within buffer */
        if (wr > 80) wr = 80;
        int bh = lh + 4;
        int by = H - bh;
        for (int yy=by; yy<H; yy++) for (int xx=0; xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=245; fb[i+1]=245; fb[i+2]=248;
        }
        /* separators + labels */
        char label[224];
        if (e->find_mode==1){
            snprintf(label,sizeof label,"Find: %.*s", wq, q);
        } else {
            snprintf(label,sizeof label,"Find: %.*s   Replace: %.*s", wq, q, wr, r);
        }
        /* caret position inside the active field */
        const char *left = (e->find_mode==2 && e->find_focus==1)? "Replace: " : "Find: ";
        int field_x = gutter + 6 + (int)wuos_font_draw(left, gutter+6, by+4+fh, 0, 60,64,72, NULL,0,0);
        wuos_font_draw(label, gutter+6, by+4+fh, 0, 30,32,40, fb, w, H);
        /* draw options + match count on the right */
        char opts[96];
        snprintf(opts,sizeof opts,"%s%s  %s  %s",
                 findbar_icase(e->fb)?"[Aa]":"[aa]",
                 findbar_regex(e->fb)?"[.*]":"[ab]",
                 (e->find_mode==2)?"F3 next|Enter rep|Ctrl+R all":"F3 next|Enter find",
                 findbar_active(e->fb)? "":(q[0]?"no match":"type & Enter"));
        if (findbar_active(e->fb)){
            int fidx=0, ftot=0; findbar_counts(e->fb, &fidx, &ftot);
            char cnt[32]; snprintf(cnt,sizeof cnt,"  [%d/%d]", fidx, ftot);
            strncat(opts, cnt, sizeof opts-1);
        }
        wuos_font_draw(opts, w - (int)wuos_font_draw(opts,w,0,0,0,0,0,NULL,0,0) - 8,
                       by+4+fh, 0, 110,114,122, fb, w, H);
        if (e->find_msg_t > 0){
            wuos_font_draw(findbar_msg(e->fb), gutter+6, by - lh + fh, 0, 200,40,40, fb, w, H);
        }
        /* a thin field focus underline */
        int ulx0 = field_x - 2, ulx1 = w-8;
        for (int xx=ulx0; xx<ulx1 && xx<w; xx++){ size_t i=((size_t)(by+bh-3)*w+xx)*4; fb[i]=180;fb[i+1]=184;fb[i+2]=192; }
    }

    /* ---- auto-completion popup ---- */
    if (e->ac && autocomp_opened(e->ac)){
        int n = autocomp_count(e->ac);
        int sel = autocomp_selected(e->ac);
        if (n > 0){
        int pw = 200, ph = 18 + n*18, px = gutter+8, py = H - ph - 8;
        if (py < 24) py = 24;
        /* panel bg */
        for (int yy=py; yy<py+ph && yy<H; yy++) for (int xx=px; xx<px+pw && xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=245;fb[i+1]=246;fb[i+2]=250;
        }
        for (int yy=py; yy<py+ph && yy<H; yy++){ size_t i=((size_t)yy*w+px)*4; fb[i]=120;fb[i+1]=124;fb[i+2]=132;
                                                  size_t j=((size_t)yy*w+(px+pw-1))*4; fb[j]=120;fb[j+1]=124;fb[j+2]=132; }
        for (int k=0; k<n; k++){
            int ry = py + 14 + k*18;
            if (k==sel){
                for (int xx=px; xx<px+pw && xx<w; xx++){ size_t i=((size_t)ry*w+xx)*4; fb[i]=210;fb[i+1]=224;fb[i+2]=245; }
                wuos_font_draw(autocomp_candidate(e->ac, k), px+8, ry+4, 0, 20,40,90, fb,w,H);
            } else {
                wuos_font_draw(autocomp_candidate(e->ac, k), px+8, ry+4, 0, 40,44,52, fb,w,H);
            }
        }
        }
    }

    /* ---- go-to-line bar ---- */
    if (e->gto && gotoline_active(e->gto)){
        int bh = lh + 4, by = H - bh;
        for (int yy=by; yy<H; yy++) for (int xx=0; xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=245; fb[i+1]=245; fb[i+2]=248;
        }
        char gl[64]; snprintf(gl,sizeof gl,"Go to line: %s", gotoline_buf(e->gto));
        wuos_font_draw(gl, gutter+6, by+4+fh, 0, 30,32,40, fb, w, H);
        wuos_font_draw("Enter jump | Esc cancel", w - (int)wuos_font_draw("Enter jump | Esc cancel",w,0,0,0,0,0,NULL,0,0) - 8,
                       by+4+fh, 0, 110,114,122, fb, w, H);
    }

    *rgba = fb; *rw = w; *rh = H;
    return 0;
}

static void on_key(WuView *v, int key, int down){
    Editor *e = v->priv;
    if (!down) return;
    e->frames = 0;  /* reset blink on activity */

    /* ---- auto-completion popup intercepts keys ---- */
    if (e->ac && autocomp_opened(e->ac)){
        switch (key){
            case WUOS_KEY_ESC: autocomp_close(e->ac); return;
            case WUOS_KEY_UP:   autocomp_move(e->ac, -1); return;
            case WUOS_KEY_DOWN: autocomp_move(e->ac, +1); return;
            case WUOS_KEY_TAB:
            case WUOS_KEY_RETURN: autocomp_accept(e->ac, e->doc); return;
            default: break;  /* typing closes the popup and falls through */
        }
        autocomp_close(e->ac);
    }

    /* ---- go-to-line mode intercepts keys ---- */
    if (e->gto && gotoline_active(e->gto)){
        /* normalize WuosKeys to the plain codes gotoline understands */
        int k = key;
        if (key == WUOS_KEY_RETURN)   k = 13;
        else if (key == WUOS_KEY_ESC) k = 27;
        else if (key == WUOS_KEY_BACKSPACE) k = 8;
        int r = gotoline_key(e->gto, k);
        if (r == 1){                       /* committed */
            int ln = gotoline_commit(e->gto);
            if (ln >= 1){
                size_t off = doc_offset_of_line(e->doc, ln);
                doc_set_cursor(e->doc, off);
                doc_set_selection(e->doc, off, off);
            }
        }
        if (r != 0) return;                /* 1 or 2: prompt consumed the key */
        return;
    }

    /* ---- find / replace mode intercepts keys ---- */
    if (e->find_mode && e->fb){
        switch (key){
            case WUOS_KEY_ESC:    find_close(v); return;
            case WUOS_KEY_FIND:   e->find_mode = 1; e->find_focus = 0; return;
            case WUOS_KEY_REPLACE: e->find_mode = 2; e->find_focus = 0; return;
            case WUOS_KEY_FINDNEXT:
                if (!findbar_query(e->fb)[0]) return;
                if (findbar_active(e->fb)){ size_t m=0,x=0; findbar_match(e->fb,&m,&x); findbar_next(e->fb, e->doc, x); }
                else findbar_next(e->fb, e->doc, 0);
                return;
            case WUOS_KEY_FINDPREV:
                if (!findbar_query(e->fb)[0]) return;
                findbar_prev(e->fb, e->doc);
                return;
            case WUOS_KEY_REPLACEALL:
                if (e->find_mode==2) findbar_replace_all(e->fb, e->doc);
                return;
            case WUOS_KEY_TAB:
                if (e->find_mode==2) e->find_focus ^= 1;  /* toggle field */
                return;
            case WUOS_KEY_RETURN:
                if (e->find_mode==2 && e->find_focus==1){
                    findbar_replace_one(e->fb, e->doc);              /* replace current */
                } else {
                    if (findbar_query(e->fb)[0]){
                        if (findbar_active(e->fb)){ size_t m=0,x=0; findbar_match(e->fb,&m,&x); findbar_next(e->fb, e->doc, x); }
                        else findbar_next(e->fb, e->doc, 0);
                    }
                }
                return;
            case WUOS_KEY_BACKSPACE: {
                char buf[512];
                const char *cur = (e->find_mode==2 && e->find_focus==1)
                    ? findbar_replace(e->fb) : findbar_query(e->fb);
                snprintf(buf, sizeof buf, "%s", cur);
                size_t l = strlen(buf);
                if (l) buf[l-1]=0;
                if (e->find_mode==2 && e->find_focus==1) findbar_set_replace(e->fb, buf);
                else findbar_set_query(e->fb, buf);
                return;
            }
            default:
                if (key>=32 && key<128){
                    char buf[512];
                    const char *cur = (e->find_mode==2 && e->find_focus==1)
                        ? findbar_replace(e->fb) : findbar_query(e->fb);
                    snprintf(buf, sizeof buf, "%s", cur);
                    size_t l = strlen(buf);
                    if (l < sizeof buf - 1){ buf[l]=(char)key; buf[l+1]=0; }
                    if (e->find_mode==2 && e->find_focus==1) findbar_set_replace(e->fb, buf);
                    else findbar_set_query(e->fb, buf);
                    if (!findbar_active(e->fb)) findbar_next(e->fb, e->doc, 0);
                    return;
                }
                return; /* swallow other keys (arrows etc.) while in find bar */
        }
    }

    /* ---- normal editing ---- */
    size_t cur = doc_cursor(e->doc);
    /* macro capture: while recording, log edit ops (skip meta keys) */
    if (e->macro && macro_recording(e->macro)){
        if (key >= 32 && key < 127) macro_record(e->macro, MACRO_OP_CHAR, (unsigned char)key);
        else if (key==WUOS_KEY_RETURN) macro_record(e->macro, MACRO_OP_RETURN, 0);
        else if (key==WUOS_KEY_BACKSPACE) macro_record(e->macro, MACRO_OP_BACKSPACE, 0);
    }
    switch (key){
        case WUOS_KEY_FIND:    find_open(v, 1); return;
        case WUOS_KEY_REPLACE: find_open(v, 2); return;
        case WUOS_KEY_GOTO:    if (e->gto) gotoline_open(e->gto); return;
        case WUOS_KEY_EOL:     convert_eol(e, e->eol_crlf? 0 : 1); return;
        case WUOS_KEY_THEME:  e->dark ^= 1; return;
        case WUOS_KEY_NEWDOC: {
            size_t i = docs_open(e->docs, NULL, "", "c");
            if (i != SIZE_MAX){ docs_set_active(e->docs, i); editor_sync_active(e); }
            return;
        }
        case WUOS_KEY_CLOSE: {
            if (docs_count(e->docs) > 1){
                size_t a = docs_active(e->docs);
                docs_close(e->docs, a);
                if (a >= docs_count(e->docs)) a = docs_count(e->docs)-1;
                docs_set_active(e->docs, a);
                editor_sync_active(e);
            }
            return;
        }
        case WUOS_KEY_DOCPREV:
        case WUOS_KEY_DOCNEXT: {
            size_t n = docs_count(e->docs);
            if (n > 1){
                size_t a = docs_active(e->docs);
                a = (key==WUOS_KEY_DOCNEXT)? (a+1)%n : (a+n-1)%n;
                docs_set_active(e->docs, a);
                editor_sync_active(e);
            }
            return;
        }
        case WUOS_KEY_TOGGLE_BK: bkmk_toggle(e->bk, editor_line_of(e)); return;
        case WUOS_KEY_NEXT_BK:   bk_jump(e, +1); return;
        case WUOS_KEY_PREV_BK:   bk_jump(e, -1); return;
        case WUOS_KEY_COLMODE:
            e->col_mode ^= 1;
            if (e->col_mode){
                e->sel_l0 = e->sel_l1 = editor_line_of(e);
                e->sel_c0 = e->sel_c1 = editor_col_of(e);
            }
            return;
        case WUOS_KEY_REC:
            if (e->macro) macro_toggle_rec(e->macro);
            return;
        case WUOS_KEY_PLAY: {
            if (!e->macro || !macro_count(e->macro)) return;
            int was = macro_recording(e->macro);
            /* don't re-record the playback */
            if (was) macro_toggle_rec(e->macro);
            macro_play(e->macro, editor_macro_replay_op, v);
            if (was) macro_toggle_rec(e->macro);   /* restore */
            return;
        }
        case WUOS_KEY_AC: if (e->ac) autocomp_open(e->ac, e->doc); return;
        case WUOS_KEY_SESSION: session_save(e); return;
        case WUOS_KEY_FOLD: fold_toggle_block(e); return;
        case WUOS_KEY_FUNCLIST: sym_toggle(e); return;
        case WUOS_KEY_BACKSPACE:
            if (cur>0) doc_delete(e->doc, cur-1, 1);
            break;
        case WUOS_KEY_RETURN:
            doc_type(e->doc, "\n", 1);
            break;
        case WUOS_KEY_TAB:
            doc_type(e->doc, "    ", 4);
            break;
        case WUOS_KEY_LEFT:
            if (e->col_mode){
                int c = editor_col_of(e);
                if (c>0){ e->sel_c1 = c-1; e->sel_l1 = editor_line_of(e); doc_set_cursor(e->doc, doc_offset_of_line(e->doc, e->sel_l1+1)+e->sel_c1); }
            } else if (cur>0) doc_set_cursor(e->doc, cur-1);
            break;
        case WUOS_KEY_RIGHT:
            if (e->col_mode){
                int c = editor_col_of(e);
                e->sel_c1 = c+1; e->sel_l1 = editor_line_of(e); doc_set_cursor(e->doc, doc_offset_of_line(e->doc, e->sel_l1+1)+e->sel_c1);
            } else doc_set_cursor(e->doc, cur+1);
            break;
        case WUOS_KEY_HOME: {
            char *t = doc_text(e->doc); size_t p=doc_cursor(e->doc);
            while (p>0 && t[p-1]!='\n') p--;
            doc_set_cursor(e->doc, p); free(t);
            break; }
        case WUOS_KEY_END: {
            char *t = doc_text(e->doc); size_t p=doc_cursor(e->doc), n=doc_length(e->doc);
            while (p<n && t[p]!='\n') p++;
            doc_set_cursor(e->doc, p); free(t);
            break; }
        case WUOS_KEY_UP: case WUOS_KEY_DOWN: {
            int dl = (key==WUOS_KEY_UP)? -1 : 1;
            if (e->col_mode){
                editor_caret_vert(e, dl);
                e->sel_l1 = editor_line_of(e);
            } else {
                char *t = doc_text(e->doc); size_t p=doc_cursor(e->doc), n=doc_length(e->doc);
                size_t line=0,col=0; for (size_t q=0;q<p;q++){ if(t[q]=='\n'){line++;col=0;}else col++; }
                size_t target = (key==WUOS_KEY_UP)? (line>0?line-1:0) : line+1;
                size_t lstart=0, curline=0;
                for (size_t q=0;q<n;q++){ if (curline==target){lstart=q;break;} if(t[q]=='\n')curline++; }
                size_t lend=lstart; while (lend<n && t[lend]!='\n') lend++;
                size_t newp = lstart + (col < (lend-lstart)? col : (lend-lstart));
                doc_set_cursor(e->doc, newp); free(t);
            }
            break; }
        case WUOS_KEY_PGUP: for(int i=0;i<20;i++) on_key(v,WUOS_KEY_UP,1); break;
        case WUOS_KEY_PGDN: for(int i=0;i<20;i++) on_key(v,WUOS_KEY_DOWN,1); break;
        case WUOS_KEY_SAVE:
            save(v);
            break;
        default:
            if (key>=32 && key<128){ char c=(char)key; doc_type(e->doc,&c,1); }
            break;
    }
}

static void on_wheel(WuView *v, int dy){ (void)v; (void)dy; /* line scroll handled in render via caret */ }

static char *status(WuView *v){
    Editor *e = v->priv;
    char *t = doc_text(e->doc);
    size_t cur = doc_cursor(e->doc);
    size_t line=1,col=1; for (size_t q=0;q<cur && t && t[q];q++){ if(t[q]=='\n'){line++;col=1;}else col++; }
    free(t);
    const char *lang = e->lex? lex_lang(e->lex) : "none";
    const char *fn = e->path? e->path : "(unsaved)";
    const char *eol = e->eol_crlf? "CRLF" : "LF";
    const char *enc = e->enc_label? e->enc_label : "UTF-8";
    char docn[32]; docn[0]=0;
    if (e->docs && docs_count(e->docs) > 1)
        snprintf(docn,sizeof docn,"[doc %zu/%zu] ", docs_active(e->docs)+1, docs_count(e->docs));
    char buf[256];
    snprintf(buf,sizeof buf,"%s%s  Ln %zu  Col %zu  %s  %s  %s  %s  [%s]",
             docn, fn, line, col, doc_has_selection(e->doc)?"SEL":"   ",
             doc_can_undo(e->doc)?"*":" ", eol, enc, lang);
    return strdup(buf);
}

static void destroy(WuView *v){
    Editor *e = v->priv;
    if (e->lex) lex_free(e->lex);
    if (e->docs) docs_free(e->docs);
    if (e->fb) findbar_destroy(e->fb);
    if (e->ac) autocomp_destroy(e->ac);
    if (e->bk) bkmk_destroy(e->bk);
    if (e->cf) codefold_destroy(e->cf);
    if (e->macro) macro_destroy(e->macro);
    if (e->gto) gotoline_destroy(e->gto);
    if (e->asv){ wubuautosave_clear(e->asv); wubuautosave_destroy(e->asv); }
    if (e->spd) spell_free(e->spd);
    free(e);
    free(v);
}

static void save(WuView *v){
    Editor *e = v->priv;
    if (!e->docs) return;
    if (!e->path) return;
    char *t = doc_text(e->doc);
    if (t){
        wuos_write_file(e->path, t, strlen(t));
        docs_set_dirty(e->docs, docs_active(e->docs), 0);
        free(t);
    }
    /* INT-2 P0: work was committed to the real file — drop the crash snapshot. */
    if (e->asv){
        wubumodel_doc *m = editor_doc_to_model(e->doc);
        if (m){ wubuautosave_flush(e->asv, m); wubumodel_doc_destroy(m); }
        wubuautosave_clear(e->asv);
    }
}

static const char *get_path(WuView *v){ return ((Editor*)v->priv)->path; }

/* Test/inspection accessor: report find state without exposing the struct. */
int wuos_editor_find_stats(WuView *v, int *active, int *total){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->fb) return -1;
    if (active) *active = findbar_active(e->fb);
    if (total)  *total  = 0;
    { int t=0; findbar_counts(e->fb, NULL, &t); if (total) *total = t; }
    return 0;
}

/* Test accessor: returns the editor's current document text (caller frees). */
char *wuos_editor_text(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e) return NULL;
    return doc_text(e->doc);
}

/* Test accessor: current caret byte offset (for go-to-line assertions). */
size_t wuos_editor_cursor(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    return doc_cursor(e->doc);
}

/* Test accessor: current dark-theme state (for theme-toggle assertion). */
int wuos_editor_dark(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    return e->dark;
}

/* Test accessor: multi-doc session size + active index (for doc-tab tests). */
size_t wuos_editor_doc_count(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->docs) return 0;
    return docs_count(e->docs);
}
size_t wuos_editor_doc_active(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->docs) return 0;
    return docs_active(e->docs);
}
/* Test accessor: number of active bookmarks (line-ops). */
int wuos_editor_bookmarks(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->bk) return 0;
    return bkmk_count(e->bk);
}
/* Test accessor: spell-check a word through the editor's attached dictionary
 * (INT-8). Returns 1 if known, 0 if misspelled, -1 if no spell engine. */
int wuos_editor_spell(WuView *v, const char *word){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->spd) return -1;
    return spell_check(e->spd, word);
}
/* Test accessor: column/block selection state (mode + block bounds). */
int wuos_editor_col(WuView *v, int *l0, int *c0, int *l1, int *c1){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    if (l0) *l0 = e->sel_l0;
    if (c0) *c0 = e->sel_c0;
    if (l1) *l1 = e->sel_l1;
    if (c1) *c1 = e->sel_c1;
    return e->col_mode;
}
/* Test accessor: macro record state (recording flag + recorded op count). */
int wuos_editor_macro(WuView *v, int *ops){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    if (ops) *ops = e->macro ? macro_count(e->macro) : 0;
    return e->macro ? macro_recording(e->macro) : 0;
}
/* Test accessor: auto-completion popup state (open + candidate count + sel). */
int wuos_editor_ac(WuView *v, int *n, int *sel){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->ac) return 0;
    if (n) *n = autocomp_count(e->ac);
    if (sel) *sel = autocomp_selected(e->ac);
    return autocomp_opened(e->ac);
}
/* Test accessor: folded-line count + function-list panel state. */
int wuos_editor_fold(WuView *v, int *count){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->cf) return 0;
    int c = codefold_folded_count(e->cf);
    if (count) *count = c;
    return c;   /* nonzero if anything folded */
}
int wuos_editor_sym(WuView *v, int *n){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->cf) return 0;
    if (n){ char *t=doc_text(e->doc); size_t len=doc_length(e->doc); LexSym s[256];
            size_t k=lex_symbols(t,len,s,256); free(t); *n=(int)k; }
    return codefold_symmode(e->cf);
}

/* ---- wubuautosave bridge (INT-2 P0) ----
 * The Editor's live buffer is a WuBuPad Doc (piece table); wubuautosave
 * snapshots a wubumodel_doc. These two helpers cross the model boundary so
 * the real crash-recovery engine actually protects the edited text. */

/* Build a single-section wubumodel_doc holding the Doc's current text. */
static wubumodel_doc *editor_doc_to_model(const void *d){
    wubumodel_doc *m = wubumodel_doc_create();
    if (!m) return NULL;
    wubumodel_node *sec  = wubumodel_node_create(m, WUBUMODEL_SECTION);
    wubumodel_node *para = wubumodel_node_create(m, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run  = wubumodel_node_create(m, WUBUMODEL_RUN);
    char *t = doc_text((void*)d);
    if (t){ wubumodel_run_set_text(run, t); free(t); }
    wubumodel_node_append(m, para, run);
    wubumodel_node_append(m, sec, para);
    return m;
}

/* Walk a wubumodel_doc's runs, returning a malloc'd concatenation (caller frees). */
static char *editor_model_text(const wubumodel_doc *m){
    wubumodel_node *sec = wubumodel_doc_root(m);
    if (!sec) return NULL;
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    for (wubumodel_node *p = wubumodel_node_first_child(sec); p;
         p = wubumodel_node_next_sibling(p)){
        for (wubumodel_node *r = wubumodel_node_first_child(p); r;
             r = wubumodel_node_next_sibling(r)){
            const char *rt = wubumodel_run_text(r);
            if (!rt) continue;
            size_t rl = strlen(rt);
            if (len + rl + 1 >= cap){ cap = len + rl + 256; char *n = realloc(out, cap); if(!n){ free(out); return NULL; } out = n; }
            memcpy(out + len, rt, rl + 1);
            len += rl;
        }
    }
    return out;
}

WuView *wuos_editor_create(const char *path){
    Editor *e = calloc(1, sizeof *e);
    if (!e) return NULL;

    const char *seed =
        "/* WuBuPad -- Notepad++ parity, embedded in WuBuOffice */\n"
        "#include <stdio.h>\n"
        "\n"
        "int main(void) {\n"
        "    int total = 0;\n"
        "    for (int i = 1; i <= 10; i++) {\n"
        "        total += i;            /* sum 1..10 */\n"
        "        if (total > 50) break; // early out\n"
        "    }\n"
        "    printf(\"sum=%d\\n\", total);\n"
        "    return 0;\n"
        "}\n";

    e->docs = docs_create();
    if (!e->docs){ free(e); return NULL; }
    e->fb = findbar_create();
    if (!e->fb){ docs_free(e->docs); free(e); return NULL; }
    e->ac = autocomp_create();
    if (!e->ac){ findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->bk = bkmk_create();
    if (!e->bk){ autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->cf = codefold_create();
    if (!e->cf){ bkmk_destroy(e->bk); autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->macro = macro_create();
    if (!e->macro){ codefold_destroy(e->cf); bkmk_destroy(e->bk); autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->gto = gotoline_create();
    if (!e->gto){ macro_destroy(e->macro); codefold_destroy(e->cf); bkmk_destroy(e->bk); autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }

    if (path){
        /* load via docs (detects encoding, seeds Doc); sets active doc */
        size_t idx = docs_load_file(e->docs, path, NULL);
        if (idx == SIZE_MAX){
            /* read failed: open blank with the requested name */
            docs_open(e->docs, path, "", "c");
        }
    } else if (getenv("WUBUOS_RESTORE")){
        /* no file arg + restore requested: reopen the saved session */
        session_restore(e);
    }
    if (docs_count(e->docs) == 0){
        docs_open(e->docs, NULL, seed, "c");
    }
    editor_sync_active(e);

    /* INT-2 P0: attach real crash-recovery autosave to the live editor.
     * Only meaningful for a real on-disk file; the seed/blank doc has no
     * path to snapshot against. Offer to recover a previous crash snapshot. */
    if (e->path){
        if (wubuautosave_has_recovery(e->path)){
            wubumodel_doc *rec = NULL;
            if (wubuautosave_recover(e->path, &rec) > 0 && rec){
                /* splice recovered text over the active doc */
                char *rt = editor_model_text(rec);
                if (rt){ doc_replace(e->doc, 0, doc_length(e->doc), rt); free(rt); }
                wubumodel_doc_destroy(rec);
                wubuautosave_discard_recovery(e->path);
            }
        }
        e->asv = wubuautosave_create(e->path, 5000);  /* 5s min gap */
        e->asv_len = doc_length(e->doc);
    }

    /* INT-8 P0: wire the spell engine so the editor shows live red squiggles
     * under misspelled words. Seed with built-in English + load any user dict. */
    {
        e->spd = spell_create();
        if (e->spd){
            spell_seed_english(e->spd);
            const char *ud = getenv("WUBUOS_DICT");
            if (ud) spell_load(e->spd, ud);
        }
    }

    /* pick lexer by active doc extension (default c) */
    const char *lang = "c";
    const char *ap = e->path;
    if (ap){
        const char *dot = strrchr(ap, '.');
        if (dot){
            if      (!strcasecmp(dot, ".json")) lang = "json";
            else if (!strcasecmp(dot, ".h") || !strcasecmp(dot, ".cxx") ||
                     !strcasecmp(dot, ".cpp") || !strcasecmp(dot, ".cc")) lang = "c";
            else if (!strcasecmp(dot, ".py")) lang = "c"; /* lexer has no python; reuse c */
        }
    }
    e->lex = lex_create(lang);
    e->frames = 0;
    WuView *v = calloc(1, sizeof *v);
    v->name = "Editor";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = render;
    v->on_key   = on_key;
    v->on_wheel = on_wheel;
    v->status   = status;
    v->save     = save;
    v->get_path = get_path;
    return v;
}
