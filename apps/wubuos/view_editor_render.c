/* view_editor_render.c -- editor view rendering (text, caret, line numbers,
 * token coloring) split from view_editor.c. */
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wuos.h"
#include "wuos_font.h"
#include "view_editor_internal.h"
#include "doc.h"
#include "lex.h"
#include "findbar.h"
#include "codefold.h"
#include "spell.h"
#include "settings.h"

wubumodel_doc *editor_doc_to_model(const void *d);

unsigned char g_def_r=36, g_def_g=41, g_def_b=47;  /* theme-aware default fg */

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

int render(WuView *v, int w, int h, int scroll,
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
            int tw = wuos_font_text_width(lab, wuos_font_height()) + 12;   /* real font width (no overlap) */
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

        /* ---- active-line highlight (modern editor affordance) ---- */
        if ((int)line == e->caret_line){
            unsigned char al_r = e->dark ? (bg_r+10) : (bg_r-12);
            unsigned char al_g = e->dark ? (bg_g+12) : (bg_g-12);
            unsigned char al_b = e->dark ? (bg_b+16) : (bg_b-12);
            for (int yy=y-2; yy<y+lh-2 && yy<H; yy++)
                for (int xx=gutter; xx<w; xx++){
                    if (xx>=0 && yy>=0){ size_t i=((size_t)yy*w+xx)*4;
                        fb[i]=al_r; fb[i+1]=al_g; fb[i+2]=al_b; }
                }
        }

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

        /* syntax highlight + live word-wrap (UI-26): paint token spans,
         * wrapping at the right margin on word boundaries. */
        size_t nsp = 0;
        if (e->lex && le > line_start)
            nsp = lex_run(e->lex, text+line_start, le-line_start, spans, 256);
        size_t sp = 0;
        size_t col = line_start;
        int x = gutter+6;
        int margin = w - 4;          /* right edge for wrap */
        int wrap = wubusettings_word_wrap(wubusettings_shared());
        while (col < le){
            LexTok k = TK_TEXT;
            size_t seg_end = le;
            if (sp < nsp){ k = spans[sp].kind; seg_end = line_start + spans[sp].end; sp++; }
            unsigned char cr,cg,cb; tok_color(k, &cr,&cg,&cb);
            /* emit this span, breaking when it would cross the margin */
            while (col < seg_end){
                /* find end of current word (whitespace-delimited) */
                size_t wend = col;
                while (wend < seg_end && text[wend]!=' ' && text[wend]!='\t') wend++;
                if (wend < seg_end) wend++;   /* include the break char */
                char seg[512]; size_t sl=0;
                for (size_t q=col; q<wend && sl<511; q++) seg[sl++]=text[q];
                seg[sl]=0;
                int adv = wuos_font_draw(seg, 0,0, 0, 0,0,0, NULL,0,0);
                if (wrap && x+adv > margin && x > gutter+6){
                    y += lh; x = gutter+6;   /* soft-wrap to next row */
                    if (y >= H - lh) { col = le; break; }
                }
                wuos_font_draw(seg, x, y+fh, 0, cr,cg,cb, fb, w, H);
                x += adv;
                col = wend;
            }
        }
        x = gutter+6;                    /* reset for caret measure */
        int caret_x = gutter+6;
        {   char pre[256]; size_t pl=0;
            size_t cp = line_start;
            while (cp < le && pl<255){ pre[pl++]=text[cp]; if (cp==caret) break; cp++; }
            pre[pl]=0;
            caret_x += wuos_font_draw(pre, 0,0, 0, 0,0,0, NULL,0,0);
        }

        /* caret: use the wrap-aware measured position (caret_x) */
        if ((int)line == e->caret_line && (e->frames/30)%2==0){
            int cx = caret_x;
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
                int tw = wubusettings_tab_width(wubusettings_shared())*9;
                int x0 = gutter + c0*tw, x1 = gutter + c1*tw;
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
            else { x += wubusettings_tab_width(wubusettings_shared())*9; }
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

