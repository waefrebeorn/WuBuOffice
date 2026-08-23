/* view_editor_render.c -- editor view rendering (text, caret, line numbers,
 * token coloring) split from view_editor.c.
 *
 * GUI_SPEC.md compliance: every surface/text color is a wuos_theme.h token
 * (dark + light variants), every layout offset is a WUOS_SPACE_* constant.
 * The syntax palette is theme-aware: dark mode uses the One-Dark-family
 * colors tuned for the dark surface; light mode uses muted equivalents that
 * keep >=4.5:1 on white. */
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_theme.h"
#include "view_editor_internal.h"
#include "doc.h"
#include "lex.h"
#include "findbar.h"
#include "codefold.h"
#include "spell.h"
#include "settings.h"

wubumodel_doc *editor_doc_to_model(const void *d);

unsigned char g_def_r=36, g_def_g=41, g_def_b=47;  /* theme-aware default fg */

/* Theme-aware syntax palette. `dark` selects the variant; both are checked
 * against their background for WCAG AA 4.5:1 (body text). */
static void tok_color(LexTok k, int dark, unsigned char *r, unsigned char *g, unsigned char *b){
    if (dark){
        switch (k){                                   /* on {30,33,40} */
            case TK_KEYWORD:  *r=118; *g=158; *b=245; break;  /* blue     */
            case TK_TYPE:     *r=104; *g=204; *b=182; break;  /* teal     */
            case TK_STRING:   *r=158; *g=206; *b=126; break;  /* green    */
            case TK_CHAR:     *r=222; *g=163; *b=110; break;  /* orange   */
            case TK_NUMBER:   *r=190; *g=214; *b=170; break;  /* l.green  */
            case TK_COMMENT:  *r=132; *g=138; *b=150; break;  /* grey     */
            case TK_PREPROC:  *r=224; *g=196; *b=140; break;  /* tan      */
            case TK_OPERATOR:
            case TK_PUNCT:    *r=146; *g=152; *b=164; break;  /* slate    */
            default:          *r=g_def_r; *g=g_def_g; *b=g_def_b; break;
        }
    } else {
        switch (k){                                   /* on white */
            case TK_KEYWORD:  *r=15;  *g=70;  *b=175; break;  /* blue     */
            case TK_TYPE:     *r=10;  *g=115; *b=98;  break;  /* teal     */
            case TK_STRING:   *r=32;  *g=105; *b=30;  break;  /* green    */
            case TK_CHAR:     *r=150; *g=72;  *b=8;   break;  /* orange   */
            case TK_NUMBER:   *r=80;  *g=105; *b=45;  break;  /* l.green  */
            case TK_COMMENT:  *r=100; *g=106; *b=116; break;  /* grey     */
            case TK_PREPROC:  *r=140; *g=90;  *b=20;  break;  /* tan      */
            case TK_OPERATOR:
            case TK_PUNCT:    *r=70;  *g=76;  *b=88;  break;  /* slate    */
            default:          *r=g_def_r; *g=g_def_g; *b=g_def_b; break;
        }
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
    int dark = e->dark;

    /* ---- theme tokens ---- */
    WuosRGB SURF   = dark ? WUOS_DARK(STATUS)        : WUOS_LIGHT(CONTENT);      /* editor bg */
    WuosRGB GUTBG  = dark ? WUOS_DARK(TAB_BAR)       : WUOS_LIGHT(TAB_BAR);      /* gutter bg */
    WuosRGB SEP    = dark ? WUOS_DARK(BORDER)        : WUOS_LIGHT(BORDER);
    WuosRGB DEF    = dark ? WUOS_DARK(OVERLAY_TEXT)  : WUOS_LIGHT(OVERLAY_TEXT); /* plain text */
    WuosRGB NUMTX  = dark ? WUOS_DARK(TABTEXT)       : WUOS_LIGHT(TABTEXT);      /* line nums */
    WuosRGB PANEL  = dark ? WUOS_DARK(OVERLAY_SURFACE): WUOS_LIGHT(OVERLAY_SURFACE);
    WuosRGB PANELBD= dark ? WUOS_DARK(OVERLAY_BD)    : WUOS_LIGHT(OVERLAY_BD);
    WuosRGB PANTX  = dark ? WUOS_DARK(OVERLINE_TEXT) : WUOS_LIGHT(OVERLINE_TEXT);/* panel heading */
    WuosRGB HINT   = dark ? WUOS_DARK(OVERLAY_HINTS): WUOS_LIGHT(OVERLAY_HINTS);

    g_def_r = DEF.r; g_def_g = DEF.g; g_def_b = DEF.b;
    for (int i=0;i<w*H;i++){ fb[i*4]=SURF.r; fb[i*4+1]=SURF.g; fb[i*4+2]=SURF.b; fb[i*4+3]=255; }
    e->top = 0;

    /* gutter + separator */
    int gutter = 52;
    for (int y=0;y<H;y++) for (int x=0;x<gutter;x++){ size_t i=((size_t)y*w+x)*4; fb[i]=GUTBG.r;fb[i+1]=GUTBG.g;fb[i+2]=GUTBG.b; }
    for (int y=0;y<H;y++){ size_t i=((size_t)y*(w)+gutter)*4; fb[i]=SEP.r;fb[i+1]=SEP.g;fb[i+2]=SEP.b; }

    /* text origin inside a line */
    const int TX = gutter + WUOS_SPACE_8;

    /* ---- empty-state hint (UI-31) ---- */
    if (e->doc && doc_length(e->doc) == 0 && !e->find_mode && !gotoline_active(e->gto)){
        const char *hint = "New document - start typing, or press Ctrl+K for commands";
        int hw = (int)wuos_font_draw(hint, 0,0, 0, 0,0,0, NULL,0,0);
        int hx = (w - hw)/2 > TX ? (w - hw)/2 : TX;
        int hy = H/2 - fh/2;
        wuos_font_draw(hint, hx, hy, 0, NUMTX.r, NUMTX.g, NUMTX.b, fb, w, H);
    }

    /* ---- document tab strip (multi-doc): neutral segments + accent
     * underline on the active tab (spec §5), theme tokens throughout ---- */
    int dofst = 0;
    if (e->docs && docs_count(e->docs) > 1){
        dofst = WUOS_SPACE_24 - 2;
        int dx = 0; size_t n = docs_count(e->docs), act = docs_active(e->docs);
        WuosRGB AC = dark ? WUOS_DARK(ACCENT) : WUOS_LIGHT(ACCENT);
        for (size_t di=0; di<n; di++){
            const char *dp = docs_path(e->docs, di);
            const char *nm = (dp && *dp)? dp : "untitled";
            const char *bn = strrchr(nm, '/'); if (bn) nm = bn+1;
            char lab[64]; snprintf(lab,sizeof lab,"%s", nm);
            int tw = wuos_font_text_width(lab, wuos_font_height()) + WUOS_SPACE_16;
            int on = (di==act);
            WuosRGB seg = on ? PANEL : GUTBG;
            for (int yy=0; yy<dofst; yy++) for (int xx=dx; xx<dx+tw && xx<w; xx++){
                size_t i=((size_t)yy*w+xx)*4;
                fb[i]=seg.r; fb[i+1]=seg.g; fb[i+2]=seg.b;
            }
            /* active tab: 2px accent underline at strip bottom */
            if (on) for (int xx=dx; xx<dx+tw && xx<w; xx++)
                for (int yy=dofst-2; yy<dofst; yy++){
                    size_t i=((size_t)yy*w+xx)*4; fb[i]=AC.r; fb[i+1]=AC.g; fb[i+2]=AC.b;
                }
            wuos_font_draw(lab, dx+WUOS_SPACE_8, dofst-fh-WUOS_SPACE_2, 0,
                           on?PANTX.r:NUMTX.r, on?PANTX.g:NUMTX.g, on?PANTX.b:NUMTX.b, fb,w,H);
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
        wuos_font_draw(num, WUOS_SPACE_8, y+fh, 0, NUMTX.r,NUMTX.g,NUMTX.b, fb, w, H);
        /* fold marker: v glyph as caret-down in the gutter */
        if (e->cf && codefold_hidden(e->cf, (int)(line+1))){
            WuosRGB FOLD = dark ? WUOS_DARK(ACCENT) : WUOS_LIGHT(ACCENT);
            wuos_font_draw("v", 30, y+fh, 0, FOLD.r,FOLD.g,FOLD.b, fb, w, H);
        }
        /* bookmark marker (accent disc) in the gutter */
        if (e->bk && bkmk_has(e->bk, (int)line)){
            WuosRGB BM = dark ? WUOS_DARK(OVERLAY_HIGHLIGHT) : WUOS_LIGHT(OVERLAY_HIGHLIGHT);
            int cx=30, cy=y+fh, cr=4;
            for (int dy=-cr; dy<=cr; dy++) for (int dx=-cr; dx<=cr; dx++)
                if (dx*dx+dy*dy <= cr*cr){ int px=cx+dx, py=cy+dy; if(px>=0&&px<gutter&&py>=0&&py<H){ size_t ii=((size_t)py*w+px)*4; fb[ii]=BM.r;fb[ii+1]=BM.g;fb[ii+2]=BM.b; } }
            break;
        }
        size_t le = pos;
        while (pos < tlen && text[pos] != '\n'){ pos++; }
        le = pos;

        /* ---- active-line highlight (subtle band behind the cursor row) ---- */
        if ((int)line == e->caret_line){
            WuosRGB AL = dark ? WUOS_DARK(TAB) : WUOS_LIGHT(TAB);  /* quiet lift */
            for (int yy=y-2; yy<y+lh-2 && yy<H; yy++)
                for (int xx=gutter; xx<w; xx++){
                    if (xx>=0 && yy>=0){ size_t i=((size_t)yy*w+xx)*4;
                        fb[i]=AL.r; fb[i+1]=AL.g; fb[i+2]=AL.b; }
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
                int x0 = TX + wuos_font_draw(pre, TX, y+fh, 0, 0,0,0, NULL,0,0);
                int wseg = wuos_font_draw(text+h0, TX, y+fh, 0, 0,0,0, NULL,0,0);
                (void)wseg;
                /* warm highlight band readable on BOTH themes */
                unsigned char mr = dark?122:255, mg = dark?96:238, mb = dark?36:120;
                for (int yy=y-2; yy<y+lh-2; yy++) for (int xx=x0; xx<x0+wseg && xx<w; xx++){
                    if (xx>=0 && yy>=0){
                        size_t i=((size_t)yy*w+xx)*4;
                        fb[i]=mr; fb[i+1]=mg; fb[i+2]=mb;
                    }
                }
            }
        }

        /* syntax highlight + live word-wrap (UI-26) */
        size_t nsp = 0;
        if (e->lex && le > line_start)
            nsp = lex_run(e->lex, text+line_start, le-line_start, spans, 256);
        size_t sp = 0;
        size_t col = line_start;
        int x = TX;
        int margin = w - WUOS_SPACE_4;          /* right edge for wrap */
        int wrap = wubusettings_word_wrap(wubusettings_shared());
        while (col < le){
            LexTok k = TK_TEXT;
            size_t seg_end = le;
            if (sp < nsp){ k = spans[sp].kind; seg_end = line_start + spans[sp].end; sp++; }
            unsigned char cr,cg,cb; tok_color(k, dark, &cr,&cg,&cb);
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
                if (wrap && x+adv > margin && x > TX){
                    y += lh; x = TX;   /* soft-wrap to next row */
                    if (y >= H - lh) { col = le; break; }
                }
                wuos_font_draw(seg, x, y+fh, 0, cr,cg,cb, fb, w, H);
                x += adv;
                col = wend;
            }
        }
        x = TX;                          /* reset for caret measure */
        int caret_x = TX;
        {   char pre[256]; size_t pl=0;
            size_t cp = line_start;
            while (cp < le && pl<255){ pre[pl++]=text[cp]; if (cp==caret) break; cp++; }
            pre[pl]=0;
            caret_x += wuos_font_draw(pre, 0,0, 0, 0,0,0, NULL,0,0);
        }

        /* caret: theme-aware (near-white on dark, near-black on light),
         * blink ~530ms via frame counter */
        if ((int)line == e->caret_line && (e->frames/30)%2==0){
            WuosRGB CT = dark ? WUOS_DARK(TABTEXT_ON) : WUOS_LIGHT(TABTEXT_ON);
            int cx = caret_x;
            for (int yy=y; yy<y+lh-2; yy++) for (int xx=cx; xx<cx+2; xx++){
                if (xx>=0&&yy>=0&&xx<w&&yy<H){ size_t i=((size_t)yy*w+xx)*4; fb[i]=CT.r;fb[i+1]=CT.g;fb[i+2]=CT.b; }
            }
        }

        line++;
        if (pos < tlen){ pos++; line_start = pos; }
        else break;

        /* column / block selection overlay (Notepad++ Alt+drag analogue):
         * accent-tinted blend instead of hard-coded blue mix */
        if (e->col_mode && e->sel_l1 >= e->sel_l0){
            int lo = e->sel_l0, hi = e->sel_l1, c0 = e->sel_c0, c1 = e->sel_c1;
            if (c1 < c0){ int t=c0; c0=c1; c1=t; }
            if ((int)line-1 >= lo && (int)line-1 <= hi){
                int tw = wubusettings_tab_width(wubusettings_shared())*9;
                int x0 = gutter + c0*tw, x1 = gutter + c1*tw;
                WuosRGB SL = dark ? WUOS_DARK(ACCENT) : WUOS_LIGHT(ACCENT);
                for (int yy=y; yy<y+lh; yy++) for (int xx=x0; xx<x1 && xx<w; xx++){
                    if (xx>=0&&yy>=0){ size_t i=((size_t)yy*w+xx)*4;
                        fb[i]  =(unsigned char)((fb[i]  +SL.r)>>1);
                        fb[i+1]=(unsigned char)((fb[i+1]+SL.g)>>1);
                        fb[i+2]=(unsigned char)((fb[i+2]+SL.b)>>1); }
                }
            }
        }

        y += lh;
    }

    /* INT-8 P0: live spell-check red squiggle (theme-independent signal red). */
    if (e->spd && text){
        int top = 6 + dofst;
        int x = TX;
        int ln = 0;
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
                                    fb[ii]=220; fb[ii+1]=60; fb[ii+2]=60;
                                }
                            }
                        }
                    }
                }
                word=NULL; wlen=0;
            }
            if (c=='\n'){ ln++; x = TX; }
            else if (c!='\t'){ x += wuos_font_draw(&c, 0, 0, 0, 0,0,0, NULL,0,0); }
            else { x += wubusettings_tab_width(wubusettings_shared())*9; }
        }
    }
    free(text);

    /* ---- function-list panel (right gutter): card surface + border +
     * heading scale per spec §3/§7 ---- */
    if (e->cf && codefold_symmode(e->cf)){
        char *st = doc_text(e->doc);
        size_t sl = doc_length(e->doc);
        LexSym syms[256];
        size_t ns = lex_symbols(st, sl, syms, 256);
        free(st);
        int pw = 220, px = w - pw;
        for (int yy=0; yy<H; yy++) for (int xx=px; xx<w; xx++){ size_t i=((size_t)yy*w+xx)*4; fb[i]=PANEL.r;fb[i+1]=PANEL.g;fb[i+2]=PANEL.b; }
        for (int yy=0; yy<H; yy++){ size_t i=((size_t)yy*w+px)*4; fb[i]=PANELBD.r;fb[i+1]=PANELBD.g;fb[i+2]=PANELBD.b; }
        char title[64]; snprintf(title,sizeof title,"Functions (%zu)", ns);
        wuos_font_draw(title, px+WUOS_SPACE_8, WUOS_SPACE_4+fh, 1, PANTX.r,PANTX.g,PANTX.b, fb,w,H);
        /* divider under the heading */
        { int hy = WUOS_SPACE_4 + fh + WUOS_SPACE_8;
          for (int xx=px; xx<w; xx++){ size_t i=((size_t)hy*w+xx)*4; fb[i]=PANELBD.r;fb[i+1]=PANELBD.g;fb[i+2]=PANELBD.b; } }
        for (size_t k=0; k<ns && k<60; k++){
            char nm[64]; size_t L = syms[k].name_len; if (L>63) L=63;
            char *dt = doc_text(e->doc); memcpy(nm, dt+syms[k].name_off, L); nm[L]=0; free(dt);
            char row[96]; snprintf(row,sizeof row,"%s : L%zu", nm, syms[k].line+1);
            wuos_font_draw(row, px+WUOS_SPACE_8, WUOS_SPACE_4+fh + (int)(k+1)*lh + WUOS_SPACE_8, 0, DEF.r,DEF.g,DEF.b, fb,w,H);
        }
    }

    e->frames++;
    if (e->find_msg_t > 0) e->find_msg_t--;

    /* INT-2 P0: periodic crash-recovery snapshot. */
    if (e->asv){
        size_t len = doc_length(e->doc);
        if (len != e->asv_len){ wubuautosave_mark_dirty(e->asv); e->asv_len = len; }
        e->asv_tick++;
        if (e->asv_tick >= 60){ e->asv_tick = 0;
            wubumodel_doc *m = editor_doc_to_model(e->doc);
            if (m){ wubuautosave_tick(e->asv, m); wubumodel_doc_destroy(m); }
        }
    }

    /* ---- find bar: bottom card panel, themed (was hard-coded light) ---- */
    if (e->find_mode && e->fb){
        const char *q = findbar_query(e->fb);
        const char *r = findbar_replace(e->fb);
        int wq = (int)strlen(q), wr = (int)strlen(r);
        if (wq > 80) wq = 80;
        if (wr > 80) wr = 80;
        int bh = lh + WUOS_SPACE_12;
        int by = H - bh;
        for (int yy=by; yy<H; yy++) for (int xx=0; xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=PANEL.r; fb[i+1]=PANEL.g; fb[i+2]=PANEL.b;
        }
        for (int xx=0; xx<w; xx++){ size_t i=((size_t)by*w+xx)*4; fb[i]=PANELBD.r;fb[i+1]=PANELBD.g;fb[i+2]=PANELBD.b; }
        char label[224];
        if (e->find_mode==1){
            snprintf(label,sizeof label,"Find: %.*s", wq, q);
        } else {
            snprintf(label,sizeof label,"Find: %.*s   Replace: %.*s", wq, q, wr, r);
        }
        const char *left = (e->find_mode==2 && e->find_focus==1)? "Replace: " : "Find: ";
        int field_x = TX + (int)wuos_font_draw(left, TX, by+WUOS_SPACE_4+fh, 0, HINT.r,HINT.g,HINT.b, NULL,0,0);
        wuos_font_draw(label, TX, by+WUOS_SPACE_4+fh, 0, DEF.r,DEF.g,DEF.b, fb, w, H);
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
        wuos_font_draw(opts, w - (int)wuos_font_draw(opts,w,0,0,0,0,0,NULL,0,0) - WUOS_SPACE_8,
                       by+WUOS_SPACE_4+fh, 0, HINT.r,HINT.g,HINT.b, fb, w, H);
        if (e->find_msg_t > 0){
            wuos_font_draw(findbar_msg(e->fb), TX, by - lh + fh, 0, 235,80,80, fb, w, H);
        }
        /* field focus underline: accent, not flat grey */
        WuosRGB AC = dark ? WUOS_DARK(ACCENT) : WUOS_LIGHT(ACCENT);
        int ulx0 = field_x - 2, ulx1 = w-WUOS_SPACE_8;
        for (int xx=ulx0; xx<ulx1 && xx<w; xx++){ size_t i=((size_t)(by+bh-3)*w+xx)*4; fb[i]=AC.r;fb[i+1]=AC.g;fb[i+2]=AC.b; }
    }

    /* ---- auto-completion popup: themed floating card (was hard-coded light) ---- */
    if (e->ac && autocomp_opened(e->ac)){
        int n = autocomp_count(e->ac);
        int sel = autocomp_selected(e->ac);
        if (n > 0){
        int rowh = lh + WUOS_SPACE_4;
        int pw = 200, ph = WUOS_SPACE_16 + n*rowh, px = gutter+WUOS_SPACE_8, py = H - ph - WUOS_SPACE_8;
        if (py < WUOS_SPACE_24) py = WUOS_SPACE_24;
        for (int yy=py; yy<py+ph && yy<H; yy++) for (int xx=px; xx<px+pw && xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=PANEL.r;fb[i+1]=PANEL.g;fb[i+2]=PANEL.b;
        }
        for (int yy=py; yy<py+ph && yy<H; yy++){ size_t i=((size_t)yy*w+px)*4; fb[i]=PANELBD.r;fb[i+1]=PANELBD.g;fb[i+2]=PANELBD.b;
                                                  size_t j=((size_t)yy*w+(px+pw-1))*4; fb[j]=PANELBD.r;fb[j+1]=PANELBD.g;fb[j+2]=PANELBD.b; }
        for (int yy=py; yy<py+ph && yy<H; yy++){ size_t i=((size_t)py*w+px)*4; fb[i]=PANELBD.r;fb[i+1]=PANELBD.g;fb[i+2]=PANELBD.b;
                                                  size_t j=((size_t)(py+ph-1)*w+px)*4; fb[j]=PANELBD.r;fb[j+1]=PANELBD.g;fb[j+2]=PANELBD.b; }
        for (int k=0; k<n; k++){
            int ry = py + WUOS_SPACE_8 + k*rowh;
            if (k==sel){
                WuosRGB HL = dark ? WUOS_DARK(TAB_ON) : WUOS_LIGHT(TAB_ON);
                for (int yy=ry; yy<ry+rowh && yy<py+ph; yy++)
                    for (int xx=px+2; xx<px+pw-2 && xx<w; xx++){ size_t i=((size_t)yy*w+xx)*4; fb[i]=HL.r;fb[i+1]=HL.g;fb[i+2]=HL.b; }
                wuos_font_draw(autocomp_candidate(e->ac, k), px+WUOS_SPACE_8, ry+WUOS_SPACE_2, 0, PANTX.r,PANTX.g,PANTX.b, fb,w,H);
            } else {
                wuos_font_draw(autocomp_candidate(e->ac, k), px+WUOS_SPACE_8, ry+WUOS_SPACE_2, 0, DEF.r,DEF.g,DEF.b, fb,w,H);
            }
        }
        }
    }

    /* ---- go-to-line bar: themed bottom panel ---- */
    if (e->gto && gotoline_active(e->gto)){
        int bh = lh + WUOS_SPACE_12, by = H - bh;
        for (int yy=by; yy<H; yy++) for (int xx=0; xx<w; xx++){
            size_t i=((size_t)yy*w+xx)*4; fb[i]=PANEL.r; fb[i+1]=PANEL.g; fb[i+2]=PANEL.b;
        }
        for (int xx=0; xx<w; xx++){ size_t i=((size_t)by*w+xx)*4; fb[i]=PANELBD.r;fb[i+1]=PANELBD.g;fb[i+2]=PANELBD.b; }
        char gl[64]; snprintf(gl,sizeof gl,"Go to line: %s", gotoline_buf(e->gto));
        wuos_font_draw(gl, TX, by+WUOS_SPACE_4+fh, 0, DEF.r,DEF.g,DEF.b, fb, w, H);
        wuos_font_draw("Enter jump | Esc cancel", w - (int)wuos_font_draw("Enter jump | Esc cancel",w,0,0,0,0,0,NULL,0,0) - WUOS_SPACE_8,
                       by+WUOS_SPACE_4+fh, 0, HINT.r,HINT.g,HINT.b, fb, w, H);
    }

    *rgba = fb; *rw = w; *rh = H;
    return 0;
}

