/* view_ocr.c -- OCR view: runs the real wubuocr pipeline and is interactive.
 * The recognized blocks are listed in a right-hand panel; Up/Down move a
 * selection; the selected block's text is shown in a detail strip and Enter
 * "copies" it (recorded in sel_text so other code / tests can read it). */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_theme.h"
#include "png.h"          /* ocr_image_from_png */
#include "image.h"        /* OcrImage */
#include "wubuocr.h"      /* ocr_page_analyze / OcrPage */
#include "fontbank.h"     /* ocr_fontbank_recognizer / OcrFontBank */
#include "wubufont.h"     /* Font, font_open, font_free, font_rasterize */
#include "page_compose.h" /* ocr_compose_line */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint8_t *read_file(const char *p, size_t *len){
    FILE *f = fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t *b=malloc(n?n:1); if(fread(b,1,n,f)!=(size_t)n){ free(b); fclose(f); return NULL; }
    fclose(f); *len=(size_t)n; return b;
}

/* Build a real (training-free) multi-font recognizer bank from the system
 * DejaVu fonts so the OCR panel shows actual recognized text instead of
 * empty geometry. Returns NULL if no usable font is present (honest: the
 * pipeline then reports geometry only). */
static OcrFontBank *build_recognizer_bank(const void ***out_fonts, size_t *out_n){
    static const char *cands[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        NULL
    };
    const void **fonts = malloc(OCR_FONTBANK_MAX * sizeof *fonts);
    if (!fonts){ if(out_fonts)*out_fonts=NULL; if(out_n)*out_n=0; return NULL; }
    size_t n = 0;
    for (int i=0; cands[i] && n < OCR_FONTBANK_MAX; i++){
        size_t fl=0; uint8_t *fb=read_file(cands[i],&fl);
        if (!fb) continue;
        Font *fn = font_open_owned(fb, fl, 1);
        free(fb);
        if (!fn) continue;
        fonts[n++] = fn;
    }
    if (n == 0){ free(fonts); if(out_fonts)*out_fonts=NULL; if(out_n)*out_n=0; return NULL; }
    OcrFontBank *bank = ocr_fontbank_build(fonts, n, 5, 48, NULL);
    if (!bank){
        for (size_t i=0;i<n;i++) font_free((Font*)fonts[i]);
        free(fonts);
        if(out_fonts)*out_fonts=NULL;
        if(out_n)*out_n=0;
        return NULL;
    }
    if(out_fonts)*out_fonts=fonts;
    if(out_n)*out_n=n;
    return bank;
}

typedef struct { OcrImage *im; OcrPage *pg; OcrFontBank *bank; const void **fonts; size_t nfonts;
                 int sel; char *sel_text; } OcrV;

static OcrImage *make_sample(void){
    static const char *path="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    size_t fl=0; uint8_t *fb=read_file(path,&fl); if(!fb) return NULL;
    Font *font = font_open_owned(fb, fl, 1); if(!font){ free(fb); return NULL; }
    /* REAL sample page: compose each line with ocr_compose_line (deterministic,
     * no warp) and stack the line images into one page. The old code passed
     * whole SENTENCES to ocr_compose_page, whose `chars` parameter expects
     * single-character strings -- the composer scattered mangled sentence
     * blobs and the recognizer could only output '! ! ! !'. */
    static const char *lines[] = {
        "The quick brown fox",
        "jumps over 1234 dogs",
        "Pack my box with",
        "five dozen 5678 quill",
        NULL
    };
    size_t n=0; while (lines[n]) n++;
    OcrImage **parts = calloc(n, sizeof *parts);
    size_t nparts = 0;
    for (size_t i=0;i<n;i++){
        /* 48ppm matches the fontbank template PPM (ocr_fontbank_build's last
         * arg) — glyph bitmaps must be the same scale as the templates or the
         * nearest-neighbor match degrades to noise (the engine's own
         * tests/test_fontbank.c uses the same 48/48 pairing). */
        OcrImage *ln = ocr_compose_line(font, lines[i], 48);
        if (ln) parts[nparts++] = ln;
    }
    font_free(font); free(fb);
    if (nparts == 0){ free(parts); return NULL; }
    /* page dims: widest line x total height + spacing */
    size_t W=0, H=0;
    for (size_t i=0;i<nparts;i++){
        size_t lw = ocr_image_width(parts[i]), lh2 = ocr_image_height(parts[i]);
        if (lw > W) W = lw;
        H += lh2 + 12;
    }
    if (H < 12) H = 12;
    OcrImage *page = ocr_image_create(W, H);
    if (!page){
        for (size_t i=0;i<nparts;i++) ocr_image_free(parts[i]);
        free(parts); return NULL;
    }
    /* white page, black text */
    for (size_t y=0;y<H;y++) for (size_t x=0;x<W;x++) ocr_image_set(page,x,y,255);
    size_t oy = 6;
    for (size_t i=0;i<nparts;i++){
        size_t lw = ocr_image_width(parts[i]), lh2 = ocr_image_height(parts[i]);
        for (size_t y=0;y<lh2;y++) for (size_t x=0;x<lw;x++){
            int g = ocr_image_get(parts[i],x,y);
            /* composite: keep the darkest pixel so glyphs stay solid */
            int cur = ocr_image_get(page,x,oy+y);
            if (g < cur) ocr_image_set(page,x,oy+y,(uint8_t)g);
        }
        oy += lh2 + 12;
    }
    for (size_t i=0;i<nparts;i++) ocr_image_free(parts[i]);
    free(parts);
    return page;
}

static int render(WuView *v, int w, int h, int scroll,
                  unsigned char **rgba, int *rw, int *rh){
    OcrV *e = v->priv;
    (void)scroll;
    int dark = wubusettings_dark(wubusettings_shared());
    WuosRGB ocr_bg = dark ? WUOS_DARK(TAB_BAR) : WUOS_LIGHT(TAB_BAR);
    unsigned char *fb = malloc((size_t)w*h*4);
    if (!fb) return -1;
    for (int i=0;i<w*h;i++){ size_t k=(size_t)i*4; fb[k]=ocr_bg.r;fb[k+1]=ocr_bg.g;fb[k+2]=ocr_bg.b;fb[k+3]=255; }

    int panel_x = w - 300; if (panel_x < w/2) panel_x = w/2;
    /* GUI_SPEC tokens for the recognized-text panel (was hard-coded light
     * rgb -- dark text on dark bg made the list invisible in dark mode). */
    WuosRGB pan_bg  = dark ? WUOS_DARK(OVERLAY_SURFACE) : WUOS_LIGHT(OVERLAY_SURFACE);
    WuosRGB pan_bd  = dark ? WUOS_DARK(OVERLAY_BD)     : WUOS_LIGHT(OVERLAY_BD);
    WuosRGB pan_hdr = dark ? WUOS_DARK(OVERLINE_TEXT)  : WUOS_LIGHT(OVERLINE_TEXT);
    WuosRGB pan_txt = dark ? WUOS_DARK(OVERLAY_TEXT)   : WUOS_LIGHT(OVERLAY_TEXT);
    WuosRGB pan_sel = dark ? WUOS_DARK(TAB_ON)         : WUOS_LIGHT(TAB_ON);
    WuosRGB page_bg = dark ? WUOS_DARK(CONTENT)        : WUOS_LIGHT(CONTENT); /* behind the scan */

    if (!e->im){ wuos_font_draw("No OCR input", WUOS_SPACE_8*2, WUOS_SPACE_8*4, 1, 220,80,80, fb,w,h); *rgba=fb;*rw=w;*rh=h; return 0; }
    /* paint the page backdrop so a white scan doesn't float on chrome */
    { int px0=WUOS_SPACE_8, py0=WUOS_SPACE_8, pw=panel_x-WUOS_SPACE_16, ph=h-WUOS_SPACE_16;
      for (int yy=py0; yy<py0+ph && yy<h; yy++) for (int xx=px0; xx<px0+pw && xx<panel_x; xx++){
          size_t i=((size_t)yy*w+xx)*4; fb[i]=page_bg.r;fb[i+1]=page_bg.g;fb[i+2]=page_bg.b; } }
    size_t iw=ocr_image_width(e->im), ih=ocr_image_height(e->im);
    int scale = (iw>0)? (panel_x-40)/ (int)iw : 1; if (scale<1) scale=1;
    int ox=20, oy=30;
    for (size_t y=0;y<ih && oy+(int)y*scale<h-70;y++)
        for (size_t x=0;x<iw && ox+(int)x*scale<panel_x-10;x++){
            uint8_t g = ocr_image_get(e->im,x,y);
            for (int dy=0;dy<scale;dy++) for (int dx=0;dx<scale;dx++){
                int xx=ox+(int)x*scale+dx, yy=oy+(int)y*scale+dy;
                if (xx>=0&&yy>=0&&xx<w&&yy<h){ size_t i=((size_t)yy*w+xx)*4;
                    fb[i]=g;fb[i+1]=g;fb[i+2]=g; }
            }
        }

    /* panel card: bg + 1px border (was bare text on chrome) */
    for (int yy=0; yy<h; yy++) for (int xx=panel_x; xx<w; xx++){
        size_t i=((size_t)yy*w+xx)*4; fb[i]=pan_bg.r;fb[i+1]=pan_bg.g;fb[i+2]=pan_bg.b; }
    for (int yy=0; yy<h; yy++){ size_t i=((size_t)yy*w+panel_x)*4; fb[i]=pan_bd.r;fb[i+1]=pan_bd.g;fb[i+2]=pan_bd.b; }

    /* panel header — two lines so it fits the 300px panel without clipping */
    wuos_font_draw("Recognized", panel_x+WUOS_SPACE_8, WUOS_SPACE_8+wuos_font_height(), 1, pan_hdr.r,pan_hdr.g,pan_hdr.b, fb,w,h);
    wuos_font_draw("(Up/Down, Enter copies)", panel_x+WUOS_SPACE_8, WUOS_SPACE_8+wuos_font_height()*2, 0, pan_hdr.r,pan_hdr.g,pan_hdr.b, fb,w,h);
    { int hy = WUOS_SPACE_8 + wuos_font_height()*3;
      for (int xx=panel_x; xx<w; xx++){ size_t i=((size_t)hy*w+xx)*4; fb[i]=pan_bd.r;fb[i+1]=pan_bd.g;fb[i+2]=pan_bd.b; } }
    if (e->pg){
        size_t n = ocr_page_block_count(e->pg);
        int ty = WUOS_SPACE_8 + wuos_font_height()*4; int idx=0;
        for (size_t i=0;i<n && ty<h-60;i++){
            const char *t = ocr_page_block_text(e->pg,i);
            if (!t || !*t){ free((void*)t); continue; }
            /* clip long lines to the panel width instead of running off-edge */
            char line[64];
            snprintf(line, sizeof line, "%.*s", (int)sizeof line - 1, t);
            while (line[0] && wuos_font_text_width(line, wuos_font_height()) > w-panel_x-WUOS_SPACE_16)
                line[strlen(line)-1] = '\0';
            free((void*)t);
            t = NULL;
            {
            int on = (idx==e->sel);
            int ry = ty;
            if (on){ for (int xx=panel_x+WUOS_SPACE_4; xx<w-WUOS_SPACE_4; xx++) for(int yy=ry-2; yy<ry+wuos_font_height()+4; yy++)
                        if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t ii=((size_t)yy*w+xx)*4; fb[ii]=pan_sel.r;fb[ii+1]=pan_sel.g;fb[ii+2]=pan_sel.b; } }
            wuos_font_draw(line, panel_x+WUOS_SPACE_8, ry, 0, pan_txt.r,pan_txt.g,pan_txt.b, fb,w,h);
            ty += wuos_font_height()+6; idx++;
            }
        }
        /* detail strip: themed card, not hard-coded dark */
        int dy = h-26;
        for (int xx=0; xx<w; xx++) for(int yy=dy; yy<h; yy++)
            if(xx>=0&&yy>=0&&xx<w&&yy<h){ size_t ii=((size_t)yy*w+xx)*4; fb[ii]=pan_bg.r;fb[ii+1]=pan_bg.g;fb[ii+2]=pan_bg.b; }
        for (int xx=0; xx<w; xx++){ size_t ii=((size_t)dy*w+xx)*4; fb[ii]=pan_bd.r;fb[ii+1]=pan_bd.g;fb[ii+2]=pan_bd.b; }
        if (e->sel_text) wuos_font_draw(e->sel_text, WUOS_SPACE_8, dy+5, 0, pan_txt.r,pan_txt.g,pan_txt.b, fb,w,h);
    }
    *rgba = fb; *rw = w; *rh = h;
    return 0;
}

static char *status(WuView *v){
    OcrV *e = v->priv;
    size_t n = e->pg ? ocr_page_block_count(e->pg) : 0;
    char *s = malloc(96); if(!s) return NULL;
    snprintf(s,96,"OCR — %zu block(s), selected %d", n, e->sel);
    return s;
}

static void on_key(WuView *v, int key, int down){
    OcrV *e = v->priv;
    if (!down || !e->pg) return;
    size_t n = ocr_page_block_count(e->pg);
    int visible = 0;
    for (size_t i=0;i<n;i++){ const char *t=ocr_page_block_text(e->pg,i); if(t&&*t) visible++; free((void*)t); }
    if (key==WUOS_KEY_DOWN){ if (e->sel < visible-1) e->sel++; }
    else if (key==WUOS_KEY_UP){ if (e->sel>0) e->sel--; }
    else if (key==WUOS_KEY_RETURN){
        free(e->sel_text); e->sel_text=NULL;
        /* map selection to the i-th non-empty block */
        int idx=0;
        for (size_t i=0;i<n;i++){ const char *t=ocr_page_block_text(e->pg,i);
            if(!t||!*t){ free((void*)t); continue; }
            if (idx==e->sel){ size_t L=strlen(t); e->sel_text=malloc(L+1); if(e->sel_text) memcpy(e->sel_text,t,L+1); free((void*)t); break; }
            idx++; free((void*)t); }
    }
}

static void destroy(WuView *v){
    OcrV *e = v->priv;
    if(e->pg) ocr_page_free(e->pg);
    if(e->im) ocr_image_free(e->im);
    if(e->bank) ocr_fontbank_free(e->bank);
    if(e->fonts){ for (size_t i=0;i<e->nfonts;i++) font_free((Font*)e->fonts[i]); free((void*)e->fonts); }
    free(e->sel_text);
    free(e);
    free(v);
}

/* Mouse: click a row in the Recognized panel (right 300px) to select it.
 * Row geometry mirrors the renderer: first entry baseline at
 * WUOS_SPACE_8 + fh*4, line pitch = fh + 6. */
static void on_click(WuView *v, int x, int y){
    OcrV *e = v->priv;
    if (!e || !e->pg) return;
    int panel_x = 960 - 300; if (panel_x < 480) panel_x = 480;
    if (x < panel_x) return;
    int fh = wuos_font_height();
    int ty = WUOS_SPACE_8 + fh*4;
    int pitch = fh + 6;
    if (y < ty - 2) return;
    int idx = (y - ty) / pitch;
    if (idx < 0) idx = 0;
    /* clamp to visible non-empty block count */
    size_t n = ocr_page_block_count(e->pg);
    int visible = 0;
    for (size_t i=0;i<n;i++){ const char *t=ocr_page_block_text(e->pg,i); if(t&&*t) visible++; free((void*)t); }
    if (idx > visible-1) idx = visible-1;
    e->sel = idx;
}

WuView *wuos_ocr_create(const char *path){
    OcrV *e = calloc(1, sizeof *e);
    const char *src = path ? path : "/tmp/ocr_in.png";
    size_t pl=0; uint8_t *pb=read_file(src,&pl);
    if (!pb && path){ pb = read_file("/tmp/ocr_in.png",&pl); }
    if (pb){ int interlaced=0; e->im = ocr_image_from_png(pb, pl, &interlaced); free(pb); }
    if (!e->im) e->im = make_sample();
    if (e->im){
        OcrLayoutParams pr; memset(&pr,0,sizeof pr);
        /* XY-cut tuning for the composed sample page: measured gaps at 90ppm —
         * letter gap 1-3px, word gap 23-30px, line gap 12px. min_gutter_v=60
         * (bigger than any in-line gap) keeps each line whole; the old value 6
         * cut at every word gap -> 76 per-letter blocks. min_gutter_h=8 splits
         * lines. */
        pr.min_block_w=8; pr.min_block_h=8; pr.min_gutter_v=60; pr.min_gutter_h=8;
        e->bank = build_recognizer_bank(&e->fonts, &e->nfonts);
        e->pg = ocr_page_analyze(e->im, &pr,
                                 ocr_fontbank_recognizer(), e->bank);
    }
    e->sel = 0; e->sel_text = NULL;
    WuView *v = calloc(1, sizeof *v);
    v->name = "OCR";
    v->priv = e;
    v->destroy = destroy;
    v->render  = render;
    v->status  = status;
    v->on_key  = on_key;
    v->on_click = on_click;
    return v;
}

/* ---- test accessors ---- */
int wuos_ocr_blocks(WuView *v){
    OcrV *e = v->priv; return e->pg ? (int)ocr_page_block_count(e->pg) : 0;
}
char *wuos_ocr_selected(WuView *v){
    OcrV *e = v->priv; if(!e->sel_text) return NULL;
    size_t L=strlen(e->sel_text); char *r=malloc(L+1); if(r) memcpy(r,e->sel_text,L+1); return r;
}
char *wuos_ocr_text(WuView *v){
    OcrV *e = v->priv;
    if (!e->pg) return NULL;
    size_t n = ocr_page_block_count(e->pg);
    size_t total = 1;
    for (size_t i=0;i<n;i++){ const char *t=ocr_page_block_text(e->pg,i); if(t) total += strlen(t)+1; free((void*)t); }
    char *out = malloc(total);
    if (!out) return NULL;
    out[0]=0;
    for (size_t i=0;i<n;i++){ const char *t=ocr_page_block_text(e->pg,i); if(t&&*t){ strcat(out,t); strcat(out," "); } free((void*)t); }
    return out;
}
