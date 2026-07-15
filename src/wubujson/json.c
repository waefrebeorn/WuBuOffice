/* json.c -- minimal JSON parse + emit. Self-contained C11. */
#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct JVal {
    JType type;
    union {
        int    b;
        double n;
        char  *s;        /* J_STR (owned) */
        struct { JVal **e; size_t n, cap; } a;   /* J_ARR */
        struct { char **k; JVal **v; size_t n, cap; } o; /* J_OBJ */
    } u;
};

static void *xrealloc(void *p, size_t n) { void *r = realloc(p, n?n:1); if(!r) abort(); return r; }
static char *xstrdup(const char *s) { size_t n = strlen(s)+1; char *r = xrealloc(NULL, n); memcpy(r, s, n); return r; }

JVal *j_null(void) { JVal *v = xrealloc(NULL, sizeof *v); memset(v,0,sizeof *v); v->type=J_NULL; return v; }
JVal *j_bool(int v) { JVal *x = xrealloc(NULL, sizeof *x); memset(x,0,sizeof *x); x->type=J_BOOL; x->u.b=v?1:0; return x; }
JVal *j_num(double v) { JVal *x = xrealloc(NULL, sizeof *x); memset(x,0,sizeof *x); x->type=J_NUM; x->u.n=v; return x; }
JVal *j_str(const char *s) { JVal *x = xrealloc(NULL, sizeof *x); memset(x,0,sizeof *x); x->type=J_STR; x->u.s=xstrdup(s?s:""); return x; }
JVal *j_arr(void) { JVal *x = xrealloc(NULL, sizeof *x); memset(x,0,sizeof *x); x->type=J_ARR; return x; }
JVal *j_obj(void) { JVal *x = xrealloc(NULL, sizeof *x); memset(x,0,sizeof *x); x->type=J_OBJ; return x; }

void j_free(JVal *v) {
    if (!v) return;
    switch (v->type) {
        case J_STR: free(v->u.s); break;
        case J_ARR: for (size_t i=0;i<v->u.a.n;i++) j_free(v->u.a.e[i]); free(v->u.a.e); break;
        case J_OBJ: for (size_t i=0;i<v->u.o.n;i++) { free(v->u.o.k[i]); j_free(v->u.o.v[i]); } free(v->u.o.k); free(v->u.o.v); break;
        default: break;
    }
    free(v);
}

void j_arr_push(JVal *arr, JVal *child) {
    if (arr->type != J_ARR) { j_free(child); return; }
    if (arr->u.a.n == arr->u.a.cap) { arr->u.a.cap = arr->u.a.cap?arr->u.a.cap*2:8; arr->u.a.e = xrealloc(arr->u.a.e, arr->u.a.cap*sizeof *arr->u.a.e); }
    arr->u.a.e[arr->u.a.n++] = child;
}
void j_obj_put(JVal *obj, const char *key, JVal *val) {
    if (obj->type != J_OBJ) { j_free(val); return; }
    /* replace if key exists */
    for (size_t i=0;i<obj->u.o.n;i++) if (strcmp(obj->u.o.k[i], key)==0) { j_free(obj->u.o.v[i]); obj->u.o.v[i]=val; return; }
    if (obj->u.o.n == obj->u.o.cap) { obj->u.o.cap = obj->u.o.cap?obj->u.o.cap*2:8; obj->u.o.k = xrealloc(obj->u.o.k, obj->u.o.cap*sizeof *obj->u.o.k); obj->u.o.v = xrealloc(obj->u.o.v, obj->u.o.cap*sizeof *obj->u.o.v); }
    obj->u.o.k[obj->u.o.n] = xstrdup(key); obj->u.o.v[obj->u.o.n] = val; obj->u.o.n++;
}

JType j_type(const JVal *v) { return v ? v->type : J_NULL; }
int   j_as_bool(const JVal *v) { return v && v->type==J_BOOL ? v->u.b : 0; }
double j_as_num(const JVal *v) { return v && v->type==J_NUM ? v->u.n : 0; }
const char *j_as_str(const JVal *v) { return (v && v->type==J_STR) ? v->u.s : ""; }
size_t j_len(const JVal *v) { if(!v) return 0; return v->type==J_ARR ? v->u.a.n : v->type==J_OBJ ? v->u.o.n : 0; }
const JVal *j_arr_at(const JVal *arr, size_t i) { if(arr && arr->type==J_ARR && i<arr->u.a.n) return arr->u.a.e[i]; return NULL; }
const JVal *j_obj_get(const JVal *obj, const char *key) { if(obj && obj->type==J_OBJ) for(size_t i=0;i<obj->u.o.n;i++) if(strcmp(obj->u.o.k[i],key)==0) return obj->u.o.v[i]; return NULL; }
const char *j_obj_key_at(const JVal *obj, size_t i) { if(obj && obj->type==J_OBJ && i<obj->u.o.n) return obj->u.o.k[i]; return NULL; }

/* ---- emit ---- */
static void emit_grow(char **out, size_t *n, size_t *cap, size_t extra) {
    if (*n + extra + 1 > *cap) {
        size_t need = *n + extra + 1;
        size_t nc = *cap ? *cap : 64;
        while (nc < need) nc *= 2;
        *out = xrealloc(*out, nc);
        *cap = nc;
    }
}
static void emit_raw(char **out, size_t *n, size_t *cap, const char *s, size_t len) {
    emit_grow(out, n, cap, len);
    memcpy(*out + *n, s, len);
    *n += len;
}
static void emit_str(char **out, size_t *n, size_t *cap, const char *s) {
    emit_grow(out, n, cap, strlen(s) * 2 + 2);  /* worst case: every char escaped */
    char *o = *out + *n; *o++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\') { *o++ = '\\'; *o++ = '\\'; }
        else if (c == '"') { *o++ = '\\'; *o++ = '"'; }
        else if (c == '\n') { *o++ = '\\'; *o++ = 'n'; }
        else if (c == '\t') { *o++ = '\\'; *o++ = 't'; }
        else if (c < 0x20) { int w = snprintf(o, 8, "\\u%04x", c); o += w; }
        else *o++ = (char)c;
    }
    *o++ = '"';
    *n = (size_t)(o - *out);
}
static void emit_val(char **out, size_t *n, size_t *cap, const JVal *v) {
    if (!v) { emit_raw(out, n, cap, "null", 4); return; }
    char num[64];
    switch (v->type) {
        case J_NULL: emit_raw(out, n, cap, "null", 4); break;
        case J_BOOL: emit_raw(out, n, cap, v->u.b ? "true" : "false", v->u.b ? 4 : 5); break;
        case J_NUM: { int w = snprintf(num, sizeof num, "%.17g", v->u.n); emit_raw(out, n, cap, num, (size_t)w); break; }
        case J_STR: emit_str(out, n, cap, v->u.s); break;
        case J_ARR: {
            emit_raw(out, n, cap, "[", 1);
            for (size_t i = 0; i < v->u.a.n; i++) {
                emit_val(out, n, cap, v->u.a.e[i]);
                if (i + 1 < v->u.a.n) emit_raw(out, n, cap, ",", 1);
            }
            emit_raw(out, n, cap, "]", 1);
            break;
        }
        case J_OBJ: {
            emit_raw(out, n, cap, "{", 1);
            for (size_t i = 0; i < v->u.o.n; i++) {
                emit_str(out, n, cap, v->u.o.k[i]);
                emit_raw(out, n, cap, ":", 1);
                emit_val(out, n, cap, v->u.o.v[i]);
                if (i + 1 < v->u.o.n) emit_raw(out, n, cap, ",", 1);
            }
            emit_raw(out, n, cap, "}", 1);
            break;
        }
    }
}
char *j_emit(const JVal *v) {
    char *out = xrealloc(NULL, 64); size_t n = 0, cap = 64;
    emit_val(&out, &n, &cap, v);
    out[n] = '\0';
    return out;
}

/* ---- parse ---- */
typedef struct { const char *p; } P;

static void skip_ws(P *p) { while (*p->p && isspace((unsigned char)*p->p)) p->p++; }
static JVal *parse_value(P *p);

static JVal *parse_str(P *p) {
    if (*p->p != '"') return NULL;
    p->p++;
    char buf[1024]; size_t n=0;
    while (*p->p && *p->p != '"') {
        char c = *p->p++;
        if (c == '\\') {
            char e = *p->p++;
            switch (e) {
                case 'n': c='\n'; break; case 't': c='\t'; break; case 'r': c='\r'; break;
                case '"': c='"'; break; case '\\': c='\\'; break; case '/': c='/'; break;
                case 'b': c='\b'; break; case 'f': c='\f'; break;
                case 'u': { /* \uXXXX -> utf-8 (basic BMP) */
                    if (p->p[0]&&p->p[1]&&p->p[2]&&p->p[3]) {
                        int cp=0; for(int k=0;k<4;k++){ char h=p->p[k]; int d = (h>='0'&&h<='9')?h-'0':(h>='a'&&h<='f')?h-'a'+10:(h>='A'&&h<='F')?h-'A'+10:0; cp=cp*16+d; } p->p+=4;
                        if (cp<0x80) c=(char)cp; else if (cp<0x800) { buf[n++]=(char)(0xC0|(cp>>6)); buf[n++]=(char)(0x80|(cp&0x3F)); c=(char)(0x80|(cp&0x3F)); }
                        else { buf[n++]=(char)(0xE0|(cp>>12)); buf[n++]=(char)(0x80|((cp>>6)&0x3F)); buf[n++]=(char)(0x80|(cp&0x3F)); c=(char)(0x80|(cp&0x3F)); }
                    } else return NULL;
                } break;
                default: return NULL;
            }
        }
        if (n+1 >= sizeof buf) { /* overflow guard for simple cases */ }
        buf[n++] = c;
    }
    if (*p->p != '"') return NULL;
    p->p++;
    buf[n] = '\0';
    return j_str(buf);
}
static JVal *parse_num(P *p) {
    const char *start = p->p;
    if (*p->p=='-') p->p++;
    while (isdigit((unsigned char)*p->p)) p->p++;
    if (*p->p=='.') { p->p++; while (isdigit((unsigned char)*p->p)) p->p++; }
    if (*p->p=='e'||*p->p=='E') { p->p++; if(*p->p=='+'||*p->p=='-')p->p++; while(isdigit((unsigned char)*p->p))p->p++; }
    double v = strtod(start, NULL);
    return j_num(v);
}
static JVal *parse_arr(P *p) {
    p->p++; /* [ */
    JVal *a = j_arr();
    skip_ws(p);
    if (*p->p == ']') { p->p++; return a; }
    for (;;) {
        skip_ws(p);
        JVal *e = parse_value(p); if (!e) { j_free(a); return NULL; }
        j_arr_push(a, e);
        skip_ws(p);
        if (*p->p == ',') { p->p++; continue; }
        if (*p->p == ']') { p->p++; return a; }
        j_free(a); return NULL;
    }
}
static JVal *parse_obj(P *p) {
    p->p++; /* { */
    JVal *o = j_obj();
    skip_ws(p);
    if (*p->p == '}') { p->p++; return o; }
    for (;;) {
        skip_ws(p);
        if (*p->p != '"') { j_free(o); return NULL; }
        JVal *k = parse_str(p); if (!k) { j_free(o); return NULL; }
        skip_ws(p);
        if (*p->p != ':') { j_free(k); j_free(o); return NULL; }
        p->p++;
        skip_ws(p);
        JVal *v = parse_value(p); if (!v) { j_free(k); j_free(o); return NULL; }
        j_obj_put(o, k->u.s, v);
        j_free(k);
        skip_ws(p);
        if (*p->p == ',') { p->p++; continue; }
        if (*p->p == '}') { p->p++; return o; }
        j_free(o); return NULL;
    }
}
static JVal *parse_value(P *p) {
    skip_ws(p);
    switch (*p->p) {
        case '"': return parse_str(p);
        case '{': return parse_obj(p);
        case '[': return parse_arr(p);
        case 't': if (strncmp(p->p,"true",4)==0){p->p+=4;return j_bool(1);} return NULL;
        case 'f': if (strncmp(p->p,"false",5)==0){p->p+=5;return j_bool(0);} return NULL;
        case 'n': if (strncmp(p->p,"null",4)==0){p->p+=4;return j_null();} return NULL;
        default:
            if (*p->p=='-' || isdigit((unsigned char)*p->p)) return parse_num(p);
            return NULL;
    }
}
JVal *j_parse(const char *text, const char **out_end) {
    P p; p.p = text;
    JVal *v = parse_value(&p);
    if (!v) return NULL;
    skip_ws(&p);
    if (out_end) *out_end = p.p;
    return v;
}
