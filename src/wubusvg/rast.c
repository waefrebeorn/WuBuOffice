/* rast.c -- minimal SVG -> RGBA rasterizer (see rast.h). */
#include "rast.h"
#include "wubusvg.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- small helpers ---- */
static double atof_def(const char *s, double def){
    if (!s || !*s) return def;
    return atof(s);
}
static int atoi_def(const char *s, int def){
    if (!s || !*s) return def;
    return (int)atof(s);
}

/* CSS color "#rrggbb" or "none"/named; returns 1 if parsed, 0 if none/unknown */
static int parse_color(const char *c, unsigned char *r, unsigned char *g, unsigned char *b){
    if (!c || !*c) return 0;
    if (!strcmp(c, "none")) return 0;
    if (c[0] == '#' && (strlen(c) == 7)){
        unsigned int v;
        if (sscanf(c+1, "%x", &v) == 1){
            *r = (v>>16)&0xff; *g = (v>>8)&0xff; *b = v&0xff; return 1;
        }
        return 0;
    }
    if (!strcmp(c,"red")){ *r=255;*g=0;*b=0; return 1; }
    if (!strcmp(c,"black")){ *r=0;*g=0;*b=0; return 1; }
    if (!strcmp(c,"white")){ *r=255;*g=255;*b=255; return 1; }
    if (!strcmp(c,"blue")){ *r=0;*g=0;*b=255; return 1; }
    if (!strcmp(c,"green")){ *r=0;*g=128;*b=0; return 1; }
    return 0;
}

static void set_px(unsigned char *fb, int W, int H, int x, int y,
                   unsigned char r, unsigned char g, unsigned char b){
    if (x<0||y<0||x>=W||y>=H) return;
    size_t i = ((size_t)y*W + x)*4;
    fb[i]=r; fb[i+1]=g; fb[i+2]=b; fb[i+3]=255;
}
static void hline(unsigned char *fb, int W, int H, int x0, int x1, int y,
                  unsigned char r, unsigned char g, unsigned char b){
    if (x1<x0){ int t=x0; x0=x1; x1=t; }
    for (int x=x0; x<=x1; x++) set_px(fb,W,H,x,y,r,g,b);
}
static void vline(unsigned char *fb, int W, int H, int y0, int y1, int x,
                  unsigned char r, unsigned char g, unsigned char b){
    if (y1<y0){ int t=y0; y0=y1; y1=t; }
    for (int y=y0; y<=y1; y++) set_px(fb,W,H,x,y,r,g,b);
}
static void line(unsigned char *fb, int W, int H, int x0,int y0,int x1,int y1,
                 unsigned char r, unsigned char g, unsigned char b){
    int dx = abs(x1-x0), dy = -abs(y1-y0);
    int sx = x0<x1?1:-1, sy = y0<y1?1:-1;
    int err = dx+dy;
    for(;;){
        set_px(fb,W,H,x0,y0,r,g,b);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 >= dy){ err += dy; x0 += sx; }
        if (e2 <= dx){ err += dx; y0 += sy; }
    }
}
static void fill_rect(unsigned char *fb, int W, int H, int x,int y,int w,int h,
                      unsigned char r, unsigned char g, unsigned char b){
    for (int yy=y; yy<y+h; yy++) hline(fb,W,H,x,x+w-1,yy,r,g,b);
}
static void stroke_rect(unsigned char *fb, int W, int H, int x,int y,int w,int h,
                        unsigned char r, unsigned char g, unsigned char b){
    hline(fb,W,H,x,x+w-1,y,r,g,b); hline(fb,W,H,x,x+w-1,y+h-1,r,g,b);
    vline(fb,W,H,y,y+h-1,x,r,g,b); vline(fb,W,H,y,y+h-1,x+w-1,r,g,b);
}
static void fill_ellipse(unsigned char *fb, int W, int H, int cx,int cy,int rx,int ry,
                         unsigned char r, unsigned char g, unsigned char b){
    for (int yy=cy-ry; yy<=cy+ry; yy++){
        for (int xx=cx-rx; xx<=cx+rx; xx++){
            double nx = (double)(xx-cx)/ (rx?rx:1);
            double ny = (double)(yy-cy)/ (ry?ry:1);
            if (nx*nx + ny*ny <= 1.0) set_px(fb,W,H,xx,yy,r,g,b);
        }
    }
}

/* recursive paint of a wubusvg subtree */
static void paint(SvgNode *n, unsigned char *fb, int W, int H, svg_text_fn tf){
    const char *tag = svg_node_name(n);
    if (!tag) return;
    if (!strcmp(tag, "rect")){
        int x = atoi_def(svg_attr(n,"x"),0);
        int y = atoi_def(svg_attr(n,"y"),0);
        int w = atoi_def(svg_attr(n,"width"),0);
        int h = atoi_def(svg_attr(n,"height"),0);
        unsigned char r,g,b;
        const char *fill = svg_attr(n,"fill");
        const char *stroke = svg_attr(n,"stroke");
        if (parse_color(fill,&r,&g,&b)) fill_rect(fb,W,H,x,y,w,h,r,g,b);
        if (parse_color(stroke,&r,&g,&b)) stroke_rect(fb,W,H,x,y,w,h,r,g,b);
    } else if (!strcmp(tag,"line")){
        int x1=atoi_def(svg_attr(n,"x1"),0), y1=atoi_def(svg_attr(n,"y1"),0);
        int x2=atoi_def(svg_attr(n,"x2"),0), y2=atoi_def(svg_attr(n,"y2"),0);
        unsigned char r,g,b;
        if (parse_color(svg_attr(n,"stroke"),&r,&g,&b)) line(fb,W,H,x1,y1,x2,y2,r,g,b);
    } else if (!strcmp(tag,"ellipse") || !strcmp(tag,"circle")){
        int cx=atoi_def(svg_attr(n,"cx"),0), cy=atoi_def(svg_attr(n,"cy"),0);
        int rx=atoi_def(svg_attr(n,"rx"),0), ry=atoi_def(svg_attr(n,"ry"),0);
        if (!strcmp(tag,"circle")) ry=rx;
        unsigned char r,g,b;
        if (parse_color(svg_attr(n,"fill"),&r,&g,&b)) fill_ellipse(fb,W,H,cx,cy,rx,ry,r,g,b);
    } else if (!strcmp(tag,"polyline") || !strcmp(tag,"polygon")){
        const char *pts = svg_attr(n,"points");
        unsigned char r,g,b;
        if (pts && parse_color(svg_attr(n,"stroke"),&r,&g,&b)){
            double px=0,py=0; int got=0, firstx=0,firsty=0;
            const char *p = pts;
            while (*p){
                while (*p==' '||*p==',') p++;
                if (!*p) break;
                double x = atof(p);
                while (*p && *p!=' ' && *p!=',') p++;
                while (*p==' '||*p==',') p++;
                if (!*p) break;
                double y = atof(p);
                while (*p && *p!=' ' && *p!=',') p++;
                if (got) line(fb,W,H,(int)px,(int)py,(int)x,(int)y,r,g,b);
                else { firstx=(int)x; firsty=(int)y; }
                px=x; py=y; got=1;
            }
            if (!strcmp(tag,"polygon") && got) line(fb,W,H,firstx,firsty,(int)px,(int)py,r,g,b);
        }
    } else if (!strcmp(tag,"text")){
        if (tf){
            const char *x = svg_attr(n,"x"); const char *y = svg_attr(n,"y");
            int tx = atoi_def(x,0), ty = atoi_def(y,0);
            unsigned char r,g,b; if (!parse_color(svg_attr(n,"fill"),&r,&g,&b)){ r=0;g=0;b=0; }
            int size = atoi_def(svg_attr(n,"font-size"),16);
            if (size<8) size=8;
            const char *t = svg_node_text(n);
            if (t && *t) tf(t, tx, ty, size, r, g, b, fb, W, H);
        }
    }
    size_t c = svg_child_count(n);
    for (size_t i=0;i<c;i++) paint(svg_child(n,i), fb, W, H, tf);
}

int svg_rasterize_cb(const char *svg, size_t len,
                     unsigned char **out, int *w, int *h, svg_text_fn tf){
    if (!svg || !out || !w || !h) return 0;
    SvgDoc *doc = svg_parse(svg, len);
    if (!doc) return 0;
    SvgNode *root = svg_root(doc);
    if (!root){ svg_free(doc); return 0; }
    int W = atoi_def(svg_attr(root,"width"), 640);
    int H = atoi_def(svg_attr(root,"height"), 400);
    if (W<=0) W=640; if (H<=0) H=400;
    unsigned char *fb = calloc((size_t)W*H, 4);
    if (!fb){ svg_free(doc); return 0; }
    for (int i=0;i<W*H;i++){ fb[i*4]=255; fb[i*4+1]=255; fb[i*4+2]=255; fb[i*4+3]=255; }
    paint(root, fb, W, H, tf);
    *out = fb; *w = W; *h = H;
    svg_free(doc);
    return 1;
}

int svg_rasterize(const char *svg, size_t len,
                  unsigned char **out, int *w, int *h){
    return svg_rasterize_cb(svg, len, out, w, h, NULL);
}
