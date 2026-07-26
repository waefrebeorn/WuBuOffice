/* math.c -- dependency-free C11 math expression renderer (see math.h). */
#include "math.h"
#include "wubusvg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- box model ---- */
typedef enum { BOX_TEXT, BOX_FRAC, BOX_SUP, BOX_SUB, BOX_GROUP } BoxKind;
typedef struct Box Box;
struct Box {
    BoxKind kind;
    /* text (BOX_TEXT) */
    char *text;
    /* children */
    Box *a, *b;          /* frac: numerator/denominator; sup/sub: base/script */
    /* geometry after layout (absolute, computed in pass) */
    double x, y, w, h, baseline;
    double size;         /* font size used for this box */
    struct Box *next;    /* sibling in a horizontal group */
};

/* ---- parser ---- */
typedef struct { const char *p; const char *end; } P;

static Box *box_new(BoxKind k){
    Box *b = calloc(1, sizeof *b);
    if (b) b->kind = k;
    return b;
}
static void box_free(Box *b){
    if (!b) return;
    box_free(b->a); box_free(b->b); box_free(b->next);
    free(b->text); free(b);
}

/* forward */
static Box *parse_expr(P *p, double size);

/* read a run of identifier/number/operator chars into a TEXT box */
static Box *parse_atom(P *p, double size){
    const char *start = p->p;
    while (p->p < p->end){
        char c = *p->p;
        if (c=='^'||c=='_'||c=='{'||c=='}'||c=='\\'||c=='('||c==')'||c==' ') break;
        p->p++;
    }
    size_t n = (size_t)(p->p - start);
    if (n == 0) return NULL;
    Box *b = box_new(BOX_TEXT);
    if (!b) return NULL;
    b->text = malloc(n+1); memcpy(b->text, start, n); b->text[n]=0;
    b->size = size;
    return b;
}

/* \frac{a}{b} */
static Box *parse_frac(P *p, double size){
    /* consume until first '{' */
    while (p->p < p->end && *p->p != '{') p->p++;
    if (p->p >= p->end) return NULL;
    p->p++; /* skip { */
    Box *num = parse_expr(p, size * 0.8);
    if (p->p < p->end && *p->p == '}') p->p++;
    while (p->p < p->end && *p->p != '{') p->p++;
    if (p->p >= p->end){ box_free(num); return NULL; }
    p->p++;
    Box *den = parse_expr(p, size * 0.8);
    if (p->p < p->end && *p->p == '}') p->p++;
    Box *f = box_new(BOX_FRAC);
    if (!f){ box_free(num); box_free(den); return NULL; }
    f->a = num; f->b = den; f->size = size;
    return f;
}

/* parse a primary: (group) | \frac | atom */
static Box *parse_primary(P *p, double size){
    if (p->p >= p->end) return NULL;
    char c = *p->p;
    if (c == '(' || c == '\\'){
        if (c == '\\'){
            /* command: \frac only for now */
            if (p->end - p->p >= 5 && strncmp(p->p, "\\frac", 5) == 0){
                p->p += 5;
                return parse_frac(p, size);
            }
            /* unknown command: emit as text and skip */
            const char *s = p->p; p->p += 1;
            while (p->p < p->end && *p->p != '{' && *p->p != ' ' && *p->p != '\\') p->p++;
            Box *b = box_new(BOX_TEXT); if(!b) return NULL;
            size_t n=(size_t)(p->p-s); b->text=malloc(n+1); memcpy(b->text,s,n); b->text[n]=0; b->size=size;
            return b;
        } else { /* '(' */
            p->p++; /* skip '(' */
            Box *g = parse_expr(p, size);
            if (p->p < p->end && *p->p == ')') p->p++;
            if (!g) return NULL;
            Box *grp = box_new(BOX_GROUP); if(!grp){ box_free(g); return NULL; }
            grp->a = g; grp->size = size;
            return grp;
        }
    }
    return parse_atom(p, size);
}

/* a primary followed by optional ^sup / _sub */
static Box *parse_supsub(P *p, double size){
    Box *base = parse_primary(p, size);
    if (!base) return NULL;
    while (p->p < p->end && (*p->p == '^' || *p->p == '_')){
        char op = *p->p; p->p++;
        /* script may be {..} or a single atom */
        double ssize = size * 0.7;
        Box *script;
        if (p->p < p->end && *p->p == '{'){
            p->p++;
            script = parse_expr(p, ssize);
            if (p->p < p->end && *p->p == '}') p->p++;
        } else {
            script = parse_atom(p, ssize);
        }
        if (!script){ continue; }
        Box *node = box_new(op == '^' ? BOX_SUP : BOX_SUB);
        if (!node){ box_free(script); continue; }
        node->a = base; node->b = script; node->size = size;
        base = node;
    }
    return base;
}

/* an expression: sequence of sup/sub primaries joined horizontally */
static Box *parse_expr(P *p, double size){
    Box *head = NULL, *tail = NULL;
    while (p->p < p->end){
        char c = *p->p;
        if (c == ')' || c == '}') break;
        Box *part = parse_supsub(p, size);
        if (!part) break;
        if (tail) tail->next = part; else head = part;
        tail = part;
    }
    if (!head) return NULL;
    if (!head->next) return head;          /* single box => itself */
    Box *g = box_new(BOX_GROUP);
    g->a = head; g->size = size;
    return g;
}

/* ---- layout (assign w/h/baseline; positions filled in render pass) ---- */
/* estimated glyph advance for a text box */
static double text_w(const char *t, double size){
    double w = 0;
    for (const char *p=t; *p; p++){
        unsigned char c = (unsigned char)*p;
        if (c == ' ') w += size*0.3;
        else if (c >= '0' && c <= '9') w += size*0.55;
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) w += size*0.6;
        else w += size*0.65; /* operators, symbols */
    }
    return w < size*0.3 ? size*0.3 : w;
}
static void layout(Box *b){
    if (!b) return;
    layout(b->a); layout(b->b); layout(b->next);
    double s = b->size;
    switch (b->kind){
    case BOX_TEXT:
        b->w = text_w(b->text, s);
        b->h = s;
        b->baseline = s * 0.8;
        break;
    case BOX_GROUP: {
        double w=0, h=0, base=0;
        for (Box *c=b->a; c; c=c->next){ w += c->w; if (c->h>h) h=c->h; if (c->baseline>base) base=c->baseline; }
        b->w=w; b->h=h; b->baseline=base;
        break;
    }
    case BOX_FRAC: {
        double pad = s*0.15;
        double nw = b->a ? b->a->w : 0, dw = b->b ? b->b->w : 0;
        b->w = (nw>dw?nw:dw) + 2*pad;
        b->h = (b->a?b->a->h:0) + (b->b?b->b->h:0) + s*0.25;
        b->baseline = (b->a?b->a->h:0) + s*0.12;
        break;
    }
    case BOX_SUP: {
        double sw = b->b ? b->b->w : 0, sh = b->b ? b->b->h : 0;
        b->w = (b->a?b->a->w:0) + sw;
        double top = (b->a? b->a->baseline - b->a->h : 0) + sh*0.5;
        b->h = (b->a? (b->a->h - b->a->baseline) : 0) + (b->a?b->a->baseline:0) + sh*0.5;
        b->baseline = (b->a?b->a->baseline:0) + sh*0.5;
        (void)top;
        break;
    }
    case BOX_SUB: {
        double sw = b->b ? b->b->w : 0;
        b->w = (b->a?b->a->w:0) + sw;
        b->h = (b->a?b->a->h:0) + (b->b?(b->b->h - b->b->baseline):0) + s*0.1;
        b->baseline = b->a ? b->a->baseline : s*0.8;
        break;
    }
    }
}

/* ---- render: position pass + SVG ---- */
typedef struct { char *p; size_t len, cap; } Buf2;
static void b2_init(Buf2 *b){ b->p=NULL; b->len=0; b->cap=0; }
static void b2_free(Buf2 *b){ free(b->p); b->p=NULL; }
static int b2_add(Buf2 *b, const char *s){
    size_t al=strlen(s);
    if (b->len+al+1>b->cap){ size_t nc=b->cap?b->cap*2:256; while(nc<b->len+al+1)nc*=2;
        char *np=realloc(b->p,nc); if(!np)return -1; b->p=np; b->cap=nc; }
    memcpy(b->p+b->len,s,al+1); b->len+=al; return 0;
}
static int b2_pf(Buf2 *b, const char *fmt, ...){
    char t[256]; va_list ap; va_start(ap,fmt); int n=vsnprintf(t,sizeof t,fmt,ap); va_end(ap);
    if(n<0)return -1;
    if((size_t)n>=sizeof t){ char *big=malloc((size_t)n+1); if(!big)return -1; va_start(ap,fmt); vsnprintf(big,(size_t)n+1,fmt,ap); va_end(ap); int r=b2_add(b,big); free(big); return r; }
    return b2_add(b,t);
}

/* assign absolute positions: cursor x advances; vertically align by baseline */
static double place(Box *b, double x, double y_top){
    if (!b) return x;
    double x0 = x;
    b->x = x;
    switch (b->kind){
    case BOX_TEXT:
    case BOX_GROUP: {
        b->y = y_top + (b->baseline) - (b->kind==BOX_TEXT? b->baseline : b->a->baseline);
        /* simpler: top = y_top; place children so their baselines align at y_top+baseline */
        double base_y = y_top + b->baseline;
        double cx = x;
        if (b->kind==BOX_GROUP){
            for (Box *c=b->a; c; c=c->next){
                double cy = base_y - c->baseline;
                place(c, cx, cy);
                cx += c->w;
            }
        }
        b->y = y_top;
        break;
    }
    case BOX_FRAC: {
        double pad=b->size*0.15;
        b->y = y_top;
        double num_y = y_top;                       /* numerator top */
        double den_y = y_top + (b->a?b->a->h:0) + b->size*0.25; /* denominator top */
        if (b->a) place(b->a, x + pad + ((b->w-2*pad) - (b->a?b->a->w:0))/2, num_y);
        if (b->b) place(b->b, x + pad + ((b->w-2*pad) - (b->b?b->b->w:0))/2, den_y);
        break;
    }
    case BOX_SUP: {
        b->y = y_top;
        if (b->a) place(b->a, x, y_top + (b->baseline - b->a->baseline));
        double sx = x + (b->a?b->a->w:0);
        if (b->b) place(b->b, sx, y_top);   /* script raised by construction */
        break;
    }
    case BOX_SUB: {
        b->y = y_top;
        if (b->a) place(b->a, x, y_top);
        double sx = x + (b->a?b->a->w:0);
        if (b->b) place(b->b, sx, y_top + (b->a?b->a->baseline:0) - (b->b?b->b->baseline:0) + b->size*0.1);
        break;
    }
    }
    return x0 + b->w;
}

static void emit(Buf2 *o, Box *b){
    if (!b) return;
    switch (b->kind){
    case BOX_TEXT: {
        b2_pf(o, "<text x=\"%.2f\" y=\"%.2f\" font-family=\"serif\" font-size=\"%.2f\" "
                 "fill=\"#000\">", b->x, b->y + b->baseline, b->size);
        for (const char *p=b->text; *p; p++){
            if (*p=='&') b2_add(o,"&amp;"); else if(*p=='<') b2_add(o,"&lt;");
            else if (*p=='>') b2_add(o,"&gt;"); else { char t[2]={*p,0}; b2_add(o,t); }
        }
        b2_add(o, "</text>\n");
        break;
    }
    case BOX_GROUP:
        for (Box *c=b->a; c; c=c->next) emit(o, c);
        break;
    case BOX_FRAC: {
        /* fraction rule between numerator and denominator */
        double rule_y = b->y + (b->a?b->a->h:0) + b->size*0.12;
        b2_pf(o, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" stroke=\"#000\" stroke-width=\"1.2\"/>\n",
              b->x, rule_y, b->x + b->w, rule_y);
        emit(o, b->a); emit(o, b->b);
        break;
    }
    case BOX_SUP:
    case BOX_SUB:
        emit(o, b->a); emit(o, b->b);
        break;
    }
}

char *math_render_svg(const char *expr){
    if (!expr || !*expr) return NULL;
    P p = { expr, expr + strlen(expr) };
    Box *root = parse_expr(&p, 20.0);
    if (!root) return NULL;
    layout(root);

    /* total size */
    double W = root->w + 4, H = root->h + 4;
    place(root, 2, 2);

    Buf2 o; b2_init(&o);
    b2_pf(&o, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    b2_pf(&o, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.2f\" height=\"%.2f\" "
              "viewBox=\"0 0 %.2f %.2f\">\n", W, H, W, H);
    emit(&o, root);
    b2_add(&o, "</svg>\n");

    char *out = NULL;
    SvgDoc *doc = svg_parse(o.p, o.len);
    if (doc){ out = svg_regurgitate(doc); svg_free(doc); }
    b2_free(&o);
    box_free(root);
    return out;
}
