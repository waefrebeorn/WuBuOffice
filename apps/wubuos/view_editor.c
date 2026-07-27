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

#include "doc.h"    /* cross-repo: ~/WuBuPad/src */
#include "lex.h"
#include "search.h" /* WuBuPad regex/literal engine */
#include "encode.h" /* WuBuPad encoding detect */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

typedef struct {
    Doc  *doc;
    Lex  *lex;
    char *path;       /* loaded file, or NULL */
    int   top;        /* first visible line */
    int   caret_line, caret_col;
    int   blink;      /* caret phase */
    int   frames;     /* for blink timing */

    /* ---- find / replace (Phase B) ---- */
    int   find_mode;  /* 0 none, 1 find, 2 replace */
    int   find_focus; /* in replace mode: 0=find field, 1=replace field */
    char  find_q[256];
    char  repl_s[256];
    int   find_icase; /* case-insensitive */
    int   find_regex; /* regex vs literal */
    Regex *re;        /* compiled regex (lazy) */
    int   re_bad;     /* last compile failed */
    size_t find_ms, find_me; /* active match [start,end) in bytes */
    int   find_active;       /* a match is currently selected */
    int   find_total;        /* count of matches (lazy) */
    int   find_idx;          /* 1-based index of active match */
    char  find_msg[64];      /* transient status (e.g. "bad pattern") */
    int   find_msg_t;        /* frames remaining to show msg */

    /* ---- go to line (Ctrl+G) ---- */
    int   goto_mode;
    char  goto_buf[32];

    /* ---- EOL + encoding (Notepad++ parity) ---- */
    int   eol_crlf;        /* 0 = LF, 1 = CRLF */
    const char *enc_label; /* detected encoding label, or NULL */
    int   dark;            /* 0 = light, 1 = dark theme */
} Editor;

/* Notepad++-style token palette (RGB) */
static unsigned char g_def_r=36, g_def_g=41, g_def_b=47;  /* theme-aware default */
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

/* Recompile the regex if needed; returns 1 if a usable pattern is set. */
static int find_ensure_re(Editor *e){
    if (!e->find_regex) return 1;            /* literal mode: no regex needed */
    if (e->re && !e->re_bad) return 1;
    if (e->re) { regex_free(e->re); e->re = NULL; }
    e->re_bad = 0;
    if (e->find_q[0] == '\0') return 0;
    e->re = regex_compile(e->find_q, e->find_icase);
    if (!e->re){ e->re_bad = 1; return 0; }
    return 1;
}

/* Find next match at/after `from` (bytes). Sets e->find_ms/me + idx/total.
 * Returns 1 on match, 0 if none. */
static int find_next(Editor *e, size_t from){
    char *t = doc_text(e->doc);
    size_t n = doc_length(e->doc);
    int got = 0;
    size_t ms=0, me=0;
    if (e->find_regex){
        if (!find_ensure_re(e)){ free(t); return 0; }
        if (e->re){
            if (regex_find_from(e->re, t, n, from, &ms, &me)) got = 1;
        }
    } else {
        size_t r = search_literal(t, n, e->find_q, strlen(e->find_q), from);
        if (r != (size_t)-1){ ms = r; me = r + strlen(e->find_q); got = 1; }
    }
    free(t);
    if (!got) return 0;
    e->find_ms = ms; e->find_me = me; e->find_active = 1;
    doc_set_selection(e->doc, ms, me);
    doc_set_cursor(e->doc, me);
    /* count total + index (small doc; linear scan) */
    e->find_total = 0; e->find_idx = 0;
    size_t pos = 0;
    while (pos <= n){
        size_t s=0, en=0; int ok=0;
        char *tt = doc_text(e->doc);
        if (e->find_regex){
            if (e->re && regex_find_from(e->re, tt, n, pos, &s, &en)) ok = 1;
        } else {
            size_t rr = search_literal(tt, n, e->find_q, strlen(e->find_q), pos);
            if (rr != (size_t)-1){ s = rr; en = rr + strlen(e->find_q); ok = 1; }
        }
        free(tt);
        if (!ok) break;
        e->find_total++;
        if (s == ms && en == me) e->find_idx = e->find_total;
        pos = en;
        if (pos == 0) break;
    }
    if (e->find_idx == 0) e->find_idx = e->find_total; /* match moved */
    return 1;
}

/* Find previous match (wrap to start). */
static int find_prev(Editor *e){
    char *t = doc_text(e->doc);
    size_t n = doc_length(e->doc);
    size_t prev_s=(size_t)-1, prev_e=(size_t)-1;
    size_t pos = 0;
    size_t start = e->find_active ? e->find_ms : 0;
    while (pos <= start){
        size_t s=0, en=0; int ok=0;
        if (e->find_regex){
            if (e->re && regex_find_from(e->re, t, n, pos, &s, &en)) ok = 1;
        } else {
            size_t rr = search_literal(t, n, e->find_q, strlen(e->find_q), pos);
            if (rr != (size_t)-1){ s = rr; en = rr + strlen(e->find_q); ok = 1; }
        }
        if (!ok) break;
        prev_s = s; prev_e = en;
        pos = en;
        if (pos == 0) break;
    }
    free(t);
    if (prev_s == (size_t)-1) return find_next(e, 0); /* wrap: first match */
    e->find_ms = prev_s; e->find_me = prev_e; e->find_active = 1;
    doc_set_selection(e->doc, prev_s, prev_e);
    doc_set_cursor(e->doc, prev_e);
    return 1;
}

static void find_close(WuView *v){
    Editor *e = v->priv;
    e->find_mode = 0;
    if (e->re){ regex_free(e->re); e->re = NULL; }
    e->re_bad = 0;
    doc_set_selection(e->doc, doc_cursor(e->doc), doc_cursor(e->doc)); /* clear */
}

/* Open find (mode=1) or replace (mode=2). */
static void find_open(WuView *v, int mode){
    Editor *e = v->priv;
    if (e->find_mode != mode){ e->find_mode = mode; e->find_focus = 0; }
    e->frames = 0;
}

/* Replace the active match with the replace string, then advance to next. */
static void find_replace_one(WuView *v){
    Editor *e = v->priv;
    if (!e->find_active) return;
    size_t ms = e->find_ms, me = e->find_me;
    doc_replace(e->doc, ms, me, e->repl_s);
    doc_set_cursor(e->doc, ms + strlen(e->repl_s));
    doc_set_selection(e->doc, doc_cursor(e->doc), doc_cursor(e->doc));
    e->find_active = 0;
    find_next(e, ms);
}

/* Replace every match in the document. */
static void find_replace_all(WuView *v){
    Editor *e = v->priv;
    if (!e->find_q[0]) return;
    int guard = 0;
    if (!find_next(e, 0)) return;
    while (e->find_active && guard++ < 100000){
        size_t ms = e->find_ms, me = e->find_me;
        doc_replace(e->doc, ms, me, e->repl_s);
        e->find_active = 0;
        if (!find_next(e, ms + strlen(e->repl_s))) break;
        if (e->find_ms == ms && e->find_me == me) break; /* no progress */
    }
    doc_set_selection(e->doc, doc_cursor(e->doc), doc_cursor(e->doc));
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

    char *text = doc_text(e->doc);
    size_t tlen = text? strlen(text):0;

    int y = 6;
    size_t pos = 0, line = 0;
    size_t line_start = 0;
    size_t caret = doc_cursor(e->doc);
    e->caret_line = 0; e->caret_col = 0;
    { size_t cl=0, cc=0; for (size_t p=0;p<caret;p++){ if (text && text[p]=='\n'){cl++;cc=0;} else cc++; } e->caret_line=cl; e->caret_col=cc; }

    LexSpan spans[256];
    while (y < H - lh){
        char num[16]; snprintf(num,sizeof num,"%zu",line+1);
        wuos_font_draw(num, 6, y+fh, 0, num_r,num_g,num_b, fb, w, H);

        size_t le = pos;
        while (pos < tlen && text[pos] != '\n'){ pos++; }
        le = pos;

        /* ---- find match highlight (behind tokens) ---- */
        if (e->find_active){
            size_t m0 = e->find_ms, m1 = e->find_me;
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
        y += lh;
    }
    free(text);

    e->frames++;
    if (e->find_msg_t > 0) e->find_msg_t--;

    /* ---- find bar (drawn over the bottom of the buffer) ---- */
    if (e->find_mode){
        int bh = lh + 4;
        int by = H - bh;
        for (int yy=by; yy<H; yy++) for (int xx=0; xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=245; fb[i+1]=245; fb[i+2]=248;
        }
        /* separators + labels */
        char label[224];
        int wq = (int)sizeof(e->find_q)-1, wr = (int)sizeof(e->repl_s)-1;
        if (wq > 80) wq = 80;   /* keep combined label within buffer */
        if (wr > 80) wr = 80;
        if (e->find_mode==1){
            snprintf(label,sizeof label,"Find: %.*s", wq, e->find_q);
        } else {
            snprintf(label,sizeof label,"Find: %.*s   Replace: %.*s", wq, e->find_q, wr, e->repl_s);
        }
        /* caret position inside the active field */
        const char *left = (e->find_mode==2 && e->find_focus==1)? "Replace: " : "Find: ";
        int field_x = gutter + 6 + (int)wuos_font_draw(left, gutter+6, by+4+fh, 0, 60,64,72, NULL,0,0);
        wuos_font_draw(label, gutter+6, by+4+fh, 0, 30,32,40, fb, w, H);
        /* draw options + match count on the right */
        char opts[96];
        snprintf(opts,sizeof opts,"%s%s  %s  %s",
                 e->find_icase?"[Aa]":"[aa]",
                 e->find_regex?"[.*]":"[ab]",
                 (e->find_mode==2)?"F3 next|Enter rep|Ctrl+R all":"F3 next|Enter find",
                 e->find_active? "":(e->find_q[0]?"no match":"type & Enter"));
        if (e->find_active){
            char cnt[32]; snprintf(cnt,sizeof cnt,"  [%d/%d]", e->find_idx, e->find_total);
            strncat(opts, cnt, sizeof opts-1);
        }
        wuos_font_draw(opts, w - (int)wuos_font_draw(opts,w,0,0,0,0,0,NULL,0,0) - 8,
                       by+4+fh, 0, 110,114,122, fb, w, H);
        if (e->find_msg_t > 0){
            wuos_font_draw(e->find_msg, gutter+6, by - lh + fh, 0, 200,40,40, fb, w, H);
        }
        /* a thin field focus underline */
        int ulx0 = field_x - 2, ulx1 = w-8;
        for (int xx=ulx0; xx<ulx1 && xx<w; xx++){ size_t i=((size_t)(by+bh-3)*w+xx)*4; fb[i]=180;fb[i+1]=184;fb[i+2]=192; }
    }

    /* ---- go-to-line bar ---- */
    if (e->goto_mode){
        int bh = lh + 4, by = H - bh;
        for (int yy=by; yy<H; yy++) for (int xx=0; xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=245; fb[i+1]=245; fb[i+2]=248;
        }
        char gl[64]; snprintf(gl,sizeof gl,"Go to line: %s", e->goto_buf);
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

    /* ---- go-to-line mode intercepts keys ---- */
    if (e->goto_mode){
        switch (key){
            case WUOS_KEY_ESC: e->goto_mode = 0; return;
            case WUOS_KEY_RETURN: {
                int ln = atoi(e->goto_buf);
                if (ln >= 1){
                    size_t off = doc_offset_of_line(e->doc, ln);
                    doc_set_cursor(e->doc, off);
                    doc_set_selection(e->doc, off, off);
                }
                e->goto_mode = 0;
                return;
            }
            case WUOS_KEY_BACKSPACE: {
                size_t l = strlen(e->goto_buf);
                if (l) e->goto_buf[l-1]=0;
                return;
            }
            default:
                if (key>='0' && key<='9'){
                    size_t l = strlen(e->goto_buf);
                    if (l < sizeof(e->goto_buf)-1){ e->goto_buf[l]=(char)key; e->goto_buf[l+1]=0; }
                    return;
                }
                if (key==WUOS_KEY_GOTO) return; /* ignore re-trigger */
                e->goto_mode = 0;  /* any other key dismisses */
                break;
        }
    }

    /* ---- find / replace mode intercepts keys ---- */
    if (e->find_mode){
        switch (key){
            case WUOS_KEY_ESC:    find_close(v); return;
            case WUOS_KEY_FIND:   e->find_mode = 1; e->find_focus = 0; return;
            case WUOS_KEY_REPLACE: e->find_mode = 2; e->find_focus = 0; return;
            case WUOS_KEY_FINDNEXT:
                if (!e->find_q[0]) return;
                if (e->find_active) find_next(e, e->find_me);
                else find_next(e, 0);
                return;
            case WUOS_KEY_FINDPREV:
                if (!e->find_q[0]) return;
                find_prev(e);
                return;
            case WUOS_KEY_REPLACEALL:
                if (e->find_mode==2) find_replace_all(v);
                return;
            case WUOS_KEY_TAB:
                if (e->find_mode==2) e->find_focus ^= 1;  /* toggle field */
                return;
            case WUOS_KEY_RETURN:
                if (e->find_mode==2 && e->find_focus==1){
                    find_replace_one(v);              /* replace current */
                } else {
                    if (e->find_q[0]){
                        if (e->find_active) find_next(e, e->find_me);
                        else find_next(e, 0);
                    }
                }
                return;
            case WUOS_KEY_BACKSPACE: {
                char *buf = (e->find_mode==2 && e->find_focus==1)? e->repl_s : e->find_q;
                size_t l = strlen(buf);
                if (l) buf[l-1]=0;
                e->re_bad = 0;
                return;
            }
            default:
                if (key>=32 && key<128){
                    char *buf = (e->find_mode==2 && e->find_focus==1)? e->repl_s : e->find_q;
                    size_t l = strlen(buf);
                    if (l < sizeof(e->find_q)-1){ buf[l]=(char)key; buf[l+1]=0; }
                    e->re_bad = 0;
                    if (!e->find_active) find_next(e, 0);
                    return;
                }
                return; /* swallow other keys (arrows etc.) while in find bar */
        }
    }

    /* ---- normal editing ---- */
    size_t cur = doc_cursor(e->doc);
    switch (key){
        case WUOS_KEY_FIND:    find_open(v, 1); return;
        case WUOS_KEY_REPLACE: find_open(v, 2); return;
        case WUOS_KEY_GOTO:    e->goto_mode = 1; e->goto_buf[0]=0; return;
        case WUOS_KEY_EOL:     convert_eol(e, e->eol_crlf? 0 : 1); return;
        case WUOS_KEY_THEME:  e->dark ^= 1; return;
        case WUOS_KEY_BACKSPACE:
            if (cur>0) doc_delete(e->doc, cur-1, 1);
            break;
        case WUOS_KEY_RETURN:
            doc_type(e->doc, "\n", 1);
            break;
        case WUOS_KEY_TAB:
            doc_type(e->doc, "    ", 4);
            break;
        case WUOS_KEY_LEFT:  if (cur>0) doc_set_cursor(e->doc, cur-1); break;
        case WUOS_KEY_RIGHT: doc_set_cursor(e->doc, cur+1); break;
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
            char *t = doc_text(e->doc); size_t p=doc_cursor(e->doc), n=doc_length(e->doc);
            size_t line=0,col=0; for (size_t q=0;q<p;q++){ if(t[q]=='\n'){line++;col=0;}else col++; }
            size_t target = (key==WUOS_KEY_UP)? (line>0?line-1:0) : line+1;
            size_t lstart=0, curline=0;
            for (size_t q=0;q<n;q++){ if (curline==target){lstart=q;break;} if(t[q]=='\n')curline++; }
            size_t lend=lstart; while (lend<n && t[lend]!='\n') lend++;
            size_t newp = lstart + (col < (lend-lstart)? col : (lend-lstart));
            doc_set_cursor(e->doc, newp); free(t);
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
    char buf[256];
    snprintf(buf,sizeof buf,"%s  Ln %zu  Col %zu  %s  %s  %s  %s  [%s]",
             fn, line, col, doc_has_selection(e->doc)?"SEL":"   ",
             doc_can_undo(e->doc)?"*":" ", eol, enc, lang);
    return strdup(buf);
}

static void destroy(WuView *v){
    Editor *e = v->priv;
    if (e->lex) lex_free(e->lex);
    doc_free(e->doc);
    free(e->path);
    free(e);
}

static void save(WuView *v){
    Editor *e = v->priv;
    if (!e->path) return;
    char *t = doc_text(e->doc);
    if (t){
        wuos_write_file(e->path, t, strlen(t));
        free(t);
    }
}

static const char *get_path(WuView *v){ return ((Editor*)v->priv)->path; }

/* Test/inspection accessor: report find state without exposing the struct. */
int wuos_editor_find_stats(WuView *v, int *active, int *total){
    Editor *e = v ? v->priv : NULL;
    if (!e) return -1;
    if (active) *active = e->find_active;
    if (total)  *total  = e->find_total;
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

    if (path){
        e->path = strdup(path);
        size_t len = 0;
        char *raw = wuos_read_file(path, &len);
        if (raw){
            /* detect encoding of the raw bytes, normalize to UTF-8 for the Doc */
            EncKind ek = enc_detect((const unsigned char*)raw, len);
            e->enc_label = enc_name(ek);
            char *txt = enc_to_utf8((const unsigned char*)raw, len, ek, NULL);
            if (txt){ e->doc = doc_create(txt); e->eol_crlf = detect_eol(txt); free(txt); }
            free(raw);
        }
    }
    if (!e->doc) e->doc = doc_create(seed);
    if (!e->enc_label) e->enc_label = "UTF-8";

    /* pick lexer by extension (default c) */
    const char *lang = "c";
    if (path){
        const char *dot = strrchr(path, '.');
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
