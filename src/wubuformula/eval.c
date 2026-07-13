/* WuBuOffice -- wubuformula/eval
 * AST evaluator. Recurses into cell references through the resolver, expands
 * range references into a flat value array for aggregate functions, and
 * detects circular references.
 *
 * Clean-room, from-scratch (SLERM). */

#include "eval.h"
#include "parser.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

static int eval_node(const ast *n, wubuformula_resolver resolve, void *ctx, wubuval *out);

static void coerce_number(const wubuval *v, double *d, int *ok) {
    *ok = 1;
    switch (v->kind) {
        case WV_NUM:   *d = v->num; break;
        case WV_BOOL:  *d = v->boolean ? 1.0 : 0.0; break;
        case WV_EMPTY: *d = 0.0; break;
        case WV_STR: {
            char *end; double x = strtod(v->str, &end);
            if (end == v->str || *end != '\0') { *ok = 0; *d = 0.0; } else *d = x;
            break;
        }
        default: *ok = 0; *d = 0.0;
    }
}

/* gather all values from a range into out (caller frees *out) */
static int gather_range(const wubucell_range *r, wubuformula_resolver resolve, void *ctx,
                        wubuval **out, int *nout) {
    int c0 = r->a.col, c1 = r->b.col, r0 = r->a.row, r1 = r->b.row;
    if (c1 < c0) { int t = c0; c0 = c1; c1 = t; }
    if (r1 < r0) { int t = r0; r0 = r1; r1 = t; }
    int cap = 8, n = 0; wubuval *arr = malloc(sizeof *arr * cap);
    for (int rr = r0; rr <= r1; rr++) {
        for (int cc = c0; cc <= c1; cc++) {
            wubucell_ref ref = (cc == r->a.col && rr == r->a.row) ? r->a : r->b;
            ref.col = cc; ref.row = rr;
            /* a cell inherits its endpoint's sheet qualification; the common
             * case (both endpoints same sheet, or a single cell) is exact.
             * For genuinely 3D ranges Excel is undefined; we keep each cell on
             * its endpoint's own sheet rather than forcing a onto b. */
            wubuval v; memset(&v, 0, sizeof v);
            int rc = resolve(ctx, &ref, &v);
            if (rc != 0) { wubuval_free(&v); continue; }
            if (n == cap) { cap *= 2; arr = realloc(arr, sizeof *arr * cap); }
            arr[n++] = v;
        }
    }
    *out = arr; *nout = n;
    return 0;
}

static int eval_func(const ast *n, wubuformula_resolver resolve, void *ctx, wubuval *out) {
    /* evaluate each argument; expand ranges into flat */
    wubuval *args = calloc((size_t)(n->nargs ? n->nargs : 1), sizeof(wubuval));
    int flat_cap = 16, flat_n = 0; wubuval *flat = malloc(sizeof(wubuval) * flat_cap);
    for (int i = 0; i < n->nargs; i++) {
        wubuval av; memset(&av, 0, sizeof av);
        if (eval_node(n->args[i], resolve, ctx, &av) != 0) { wubuval_free(&av); }
        args[i] = av;
        if (n->args[i]->kind == N_RANGE) {
            wubuval *ra; int rn;
            if (gather_range(&n->args[i]->range, resolve, ctx, &ra, &rn) == 0) {
                for (int k = 0; k < rn; k++) {
                    if (flat_n == flat_cap) { flat_cap *= 2; flat = realloc(flat, sizeof(wubuval) * flat_cap); }
                    wubuval_copy(&flat[flat_n++], &ra[k]);
                }
                free(ra);
            }
        } else {
            if (flat_n == flat_cap) { flat_cap *= 2; flat = realloc(flat, sizeof(wubuval) * flat_cap); }
            wubuval_copy(&flat[flat_n++], &av);
        }
    }
    wubu_func_impl fn = wubu_func_lookup(n->str);
    if (!fn) {
        wubuval_set_err(out, WERR_NAME);
        for (int i = 0; i < n->nargs; i++) wubuval_free(&args[i]);
        for (int i = 0; i < flat_n; i++) wubuval_free(&flat[i]);
        free(args); free(flat);
        return 0;
    }
    fn(args, n->nargs, flat, flat_n, out);
    for (int i = 0; i < n->nargs; i++) wubuval_free(&args[i]);
    for (int i = 0; i < flat_n; i++) wubuval_free(&flat[i]);
    free(args); free(flat);
    return 0;
}

static int eval_node(const ast *n, wubuformula_resolver resolve, void *ctx, wubuval *out) {
    switch (n->kind) {
        case N_NUM:  wubuval_set_num(out, n->num); return 0;
        case N_STR:  wubuval_set_str(out, n->str); return 0;
        case N_BOOL: wubuval_set_bool(out, n->boolean); return 0;
        case N_REF: {
            wubucell_ref ref = n->ref;
            int rc = resolve(ctx, &ref, out);
            if (rc != 0) { wubuval_set_err(out, WERR_REF); return 0; }
            return 0;
        }
        case N_RANGE: {
            /* a bare range as a value: Excel would error; we return the first
             * numeric or #VALUE! */
            wubuval *arr; int rn;
            if (gather_range(&n->range, resolve, ctx, &arr, &rn) != 0) { wubuval_set_err(out, WERR_REF); return 0; }
            int ok; double d = 0; int found = 0;
            (void)ok;
            for (int i = 0; i < rn; i++) { if (arr[i].kind == WV_NUM) { d = arr[i].num; found = 1; } }
            for (int i = 0; i < rn; i++) wubuval_free(&arr[i]);
            free(arr);
            if (found) wubuval_set_num(out, d); else wubuval_set_err(out, WERR_VALUE);
            return 0;
        }
        case N_UNARY: {
            wubuval c; memset(&c, 0, sizeof c);
            if (eval_node(n->child, resolve, ctx, &c) != 0) { wubuval_free(&c); return -1; }
            if (n->op == '-') {
                int ok; double x; coerce_number(&c, &x, &ok);
                if (!ok) { wubuval_set_err(out, WERR_VALUE); wubuval_free(&c); return 0; }
                wubuval_set_num(out, -x);
            } else { wubuval_copy(out, &c); }
            wubuval_free(&c);
            return 0;
        }
        case N_BINARY: {
            wubuval l, r; memset(&l, 0, sizeof l); memset(&r, 0, sizeof r);
            if (eval_node(n->l, resolve, ctx, &l) != 0) { wubuval_free(&l); return -1; }
            if (eval_node(n->r, resolve, ctx, &r) != 0) { wubuval_free(&l); wubuval_free(&r); return -1; }
            /* Error values propagate through all binary operations. */
            if (l.kind == WV_ERR) { *out = l; wubuval_free(&r); return 0; }
            if (r.kind == WV_ERR) { *out = r; wubuval_free(&l); return 0; }
            int lok, rok; double x, y;
            switch (n->op) {
                case '+': coerce_number(&l,&x,&lok); coerce_number(&r,&y,&rok);
                          if(!lok||!rok){wubuval_set_err(out,WERR_VALUE);}else wubuval_set_num(out,x+y); break;
                case '-': coerce_number(&l,&x,&lok); coerce_number(&r,&y,&rok);
                          if(!lok||!rok){wubuval_set_err(out,WERR_VALUE);}else wubuval_set_num(out,x-y); break;
                case '*': coerce_number(&l,&x,&lok); coerce_number(&r,&y,&rok);
                          if(!lok||!rok){wubuval_set_err(out,WERR_VALUE);}else wubuval_set_num(out,x*y); break;
                case '/': coerce_number(&l,&x,&lok); coerce_number(&r,&y,&rok);
                          if(!lok||!rok){wubuval_set_err(out,WERR_VALUE);}
                          else if(y==0){wubuval_set_err(out,WERR_DIV0);}
                          else { wubuval_set_num(out,x/y); } break;
                case '^': coerce_number(&l,&x,&lok); coerce_number(&r,&y,&rok);
                          if(!lok||!rok){wubuval_set_err(out,WERR_VALUE);}else wubuval_set_num(out,pow(x,y)); break;
                case '&': {
                    char *lt = NULL;
                    if (l.kind==WV_STR) lt = strdup(l.str ? l.str : "");
                    else if (l.kind==WV_BOOL) lt = strdup(l.boolean ? "TRUE" : "FALSE");
                    else if (l.kind==WV_NUM) { char b[64]; if(l.num==(long long)l.num&&fabs(l.num)<1e15)snprintf(b,sizeof b,"%.0f",l.num);else snprintf(b,sizeof b,"%.12g",l.num); lt=strdup(b); }
                    else lt = strdup("");
                    char *rt = NULL;
                    if (r.kind==WV_STR) rt = strdup(r.str ? r.str : "");
                    else if (r.kind==WV_BOOL) rt = strdup(r.boolean ? "TRUE" : "FALSE");
                    else if (r.kind==WV_NUM) { char b[64]; if(r.num==(long long)r.num&&fabs(r.num)<1e15)snprintf(b,sizeof b,"%.0f",r.num);else snprintf(b,sizeof b,"%.12g",r.num); rt=strdup(b); }
                    else rt = strdup("");
                    size_t L = strlen(lt) + strlen(rt) + 1;
                    char *s = malloc(L);
                    snprintf(s, L, "%s%s", lt, rt);
                    wubuval_set_str(out, s);
                    free(s); free(lt); free(rt);
                    break;
                }
                case '=': {
                    int res;
                    if (l.kind==WV_NUM && r.kind==WV_NUM) res = (l.num==r.num);
                    else if (l.kind==WV_BOOL && r.kind==WV_BOOL) res = (l.boolean==r.boolean);
                    else if (l.kind==WV_STR && r.kind==WV_STR) res = (strcmp(l.str?l.str:"",r.str?r.str:"")==0);
                    else res = 0;
                    wubuval_set_bool(out, res); break;
                }
                case 256+2: { /* <>  (T_NE) */
                    int res;
                    if (l.kind==WV_NUM && r.kind==WV_NUM) res=(l.num!=r.num);
                    else if (l.kind==WV_BOOL && r.kind==WV_BOOL) res=(l.boolean!=r.boolean);
                    else if (l.kind==WV_STR && r.kind==WV_STR) res=(strcmp(l.str?l.str:"",r.str?r.str:"")!=0);
                    else res=1;
                    wubuval_set_bool(out,res); break;
                }
                case '<': { /* T_LT */
                    double a,b; int o1,o2; coerce_number(&l,&a,&o1); coerce_number(&r,&b,&o2);
                    wubuval_set_bool(out, (o1&&o2)? a<b : 0); break;
                }
                case '>': { /* T_GT */
                    double a,b; int o1,o2; coerce_number(&l,&a,&o1); coerce_number(&r,&b,&o2);
                    wubuval_set_bool(out, (o1&&o2)? a>b : 0); break;
                }
                case 256+1: { /* <=  (T_LE) */
                    double a,b; int o1,o2; coerce_number(&l,&a,&o1); coerce_number(&r,&b,&o2);
                    wubuval_set_bool(out, (o1&&o2)? a<=b : 0); break;
                }
                case 256+3: { /* >=  (T_GE) */
                    double a,b; int o1,o2; coerce_number(&l,&a,&o1); coerce_number(&r,&b,&o2);
                    wubuval_set_bool(out, (o1&&o2)? a>=b : 0); break;
                }
                default:
                    wubuval_set_err(out, WERR_VALUE);
            }
            wubuval_free(&l); wubuval_free(&r);
            return 0;
        }
        case N_FUNC:
            return eval_func(n, resolve, ctx, out);
        default:
            wubuval_set_err(out, WERR_VALUE);
            return 0;
    }
}

int wubu_eval(const ast *node, wubuformula_resolver resolve, void *ctx, wubuval *out) {
    if (!node) { wubuval_set_err(out, WERR_PARSE); return -1; }
    return eval_node(node, resolve, ctx, out);
}

int wubu_formula_eval(const char *src, wubuformula_resolver resolve, void *ctx, wubuval *out) {
    char errbuf[128];
    ast *root = wubu_parse(src, strlen(src), errbuf, sizeof errbuf);
    if (!root) { wubuval_set_err(out, WERR_PARSE); return -1; }
    int rc = eval_node(root, resolve, ctx, out);
    ast_free(root);
    return rc;
}
