#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "wububasic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { char *text; int num; } line;
typedef struct { char *name; char *val; } var;

struct wububasic {
    line *lines; size_t nlines, lcap;
    var *vars; size_t nvars, vcap;
    wububasic_input_fn in_fn; void *in_ud;
    wububasic_output_fn out_fn; void *out_ud;
    int pc;                 /* current line index */
    size_t goto_pc;         /* GOSUB return stack */
    int *gosub_stack; size_t gosub_top, gosub_cap;
    char err[160];
    int running;
};

wububasic *wububasic_create(void){
    return (wububasic*)calloc(1, sizeof(wububasic));
}

void wububasic_destroy(wububasic *b){
    if (!b) return;
    for (size_t i=0;i<b->nlines;i++) free(b->lines[i].text);
    free(b->lines);
    for (size_t i=0;i<b->nvars;i++){ free(b->vars[i].name); free(b->vars[i].val); }
    free(b->vars);
    free(b->gosub_stack);
    free(b);
}

static int find_line(const wububasic *b, int num){
    for (size_t i=0;i<b->nlines;i++) if (b->lines[i].num == num) return (int)i;
    return -1;
}

int wububasic_load(wububasic *b, const char *program){
    if (!b || !program) return -1;
    /* reset lines */
    for (size_t i=0;i<b->nlines;i++) free(b->lines[i].text);
    free(b->lines); b->lines=NULL; b->nlines=0; b->lcap=0;
    const char *p = program;
    while (*p){
        /* read one line */
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char buf[1024];
        if (len >= sizeof buf) len = sizeof buf - 1;
        memcpy(buf, p, len); buf[len] = 0;
        /* trim */
        while (len && isspace((unsigned char)buf[len-1])) buf[--len]=0;
        if (len){
            if (b->nlines == b->lcap){
                size_t nc = b->lcap?b->lcap*2:16;
                line *nl = (line*)realloc(b->lines, nc*sizeof(line));
                if (!nl) return -1;
                b->lines = nl; b->lcap = nc;
            }
            char *s = strdup(buf);
            if (!s) return -1;
            /* optional line number prefix */
            char *q = s; int num = -1;
            if (isdigit((unsigned char)*q)){
                char *end; long v = strtol(q, &end, 10);
                if (end != q){ num = (int)v; q = end; while (isspace((unsigned char)*q)) q++; }
            }
            b->lines[b->nlines].text = strdup(q ? q : "");
            b->lines[b->nlines].num = num;
            free(s);
            if (!b->lines[b->nlines].text) return -1;
            b->nlines++;
        }
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

void wububasic_set_input(wububasic *b, wububasic_input_fn fn, void *ud){ if(b){b->in_fn=fn;b->in_ud=ud;} }
void wububasic_set_output(wububasic *b, wububasic_output_fn fn, void *ud){ if(b){b->out_fn=fn;b->out_ud=ud;} }

static void out(wububasic *b, const char *s){
    if (b->out_fn) b->out_fn(s, b->out_ud);
    else fputs(s, stdout);
}

static var *find_var(wububasic *b, const char *name){
    for (size_t i=0;i<b->nvars;i++) if (strcmp(b->vars[i].name, name)==0) return &b->vars[i];
    return NULL;
}

int wububasic_set_var(wububasic *b, const char *name, const char *value){
    if (!b || !name || !value) return -1;
    var *v = find_var(b, name);
    if (v){ free(v->val); v->val = strdup(value); return v->val?0:-1; }
    if (b->nvars == b->vcap){
        size_t nc = b->vcap?b->vcap*2:16;
        var *nv = (var*)realloc(b->vars, nc*sizeof(var));
        if (!nv) return -1;
        b->vars = nv; b->vcap = nc;
    }
    b->vars[b->nvars].name = strdup(name);
    b->vars[b->nvars].val = strdup(value);
    if (!b->vars[b->nvars].name || !b->vars[b->nvars].val) return -1;
    b->nvars++;
    return 0;
}

const char *wububasic_get_var(const wububasic *b, const char *name){
    var *v = find_var((wububasic*)b, name);
    return v ? v->val : NULL;
}

static void seterr(wububasic *b, const char *m){ strncpy(b->err, m, sizeof b->err - 1); b->err[sizeof b->err-1]=0; }
const char *wububasic_error(const wububasic *b){ return b?b->err:NULL; }

/* Evaluate a simple expression: identifier / number / string-literal,
 * optionally with + concatenation. Returns malloc'd string. */
static char *eval(wububasic *b, const char *expr, int *ok){
    *ok = 0;
    while (*expr && isspace((unsigned char)*expr)) expr++;
    char res[256]; res[0]=0;
    const char *p = expr;
    /* first operand */
    if (*p == '"'){
        const char *q = strchr(p+1, '"');
        if (!q){ seterr(b,"unterminated string"); return NULL; }
        size_t n = (size_t)(q-(p+1)); if (n >= sizeof res) n = sizeof res -1;
        memcpy(res, p+1, n); res[n]=0; p = q+1;
    } else {
        /* number or var */
        const char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p!='+' && *p!='-' && *p!='*' && *p!='/' && *p!=',') p++;
        size_t n = (size_t)(p-start);
        char tok[128]; if (n>=sizeof tok) n=sizeof tok-1; memcpy(tok,start,n); tok[n]=0;
        var *v = find_var(b, tok);
        const char *val = v ? v->val : tok;
        strncpy(res, val, sizeof res -1); res[sizeof res-1]=0;
    }
    /* handle trailing +/- for numbers */
    while (*p && isspace((unsigned char)*p)) p++;
    while (*p == '+' || *p == '-'){
        char op = *p; p++;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p!='+' && *p!='-' && *p!=',') p++;
        size_t n = (size_t)(p-start);
        char tok[128]; if (n>=sizeof tok) n=sizeof tok-1; memcpy(tok,start,n); tok[n]=0;
        var *v = find_var(b, tok);
        const char *val = v ? v->val : tok;
        /* if both numeric, do arithmetic; else string concat */
        char *eres; double a = strtod(res, &eres);
        int num_res = (eres && *eres==0) ? 1 : 0;
        char *e2; double bv = strtod(val, &e2);
        int num_val = (e2 && *e2==0) ? 1 : 0;
        if (num_res && num_val){
            double r = (op=='+') ? a+bv : a-bv;
            snprintf(res, sizeof res, "%g", r);
        } else if (op=='+'){
            strncat(res, val, sizeof res - strlen(res) - 1);
        } else {
            seterr(b,"- on non-numeric"); return NULL;
        }
        while (*p && isspace((unsigned char)*p)) p++;
    }
    *ok = 1;
    return strdup(res);
}

/* Get numeric value of a variable or literal token. */
static double num_of(wububasic *b, const char *tok){
    var *v = find_var(b, tok);
    const char *val = v ? v->val : tok;
    return strtod(val, NULL);
}

static int run_line(wububasic *b, char *src);

/* Jump to line number. Returns 1 if found & advanced pc, 0 otherwise. */
static int goto_line(wububasic *b, int num){
    int idx = find_line(b, num);
    if (idx < 0){ seterr(b,"GOTO line not found"); return 0; }
    b->pc = idx;
    return 1;
}

/* For a control flow statement at pc, execute and possibly change pc.
 * Returns 0 on success, -1 on runtime error. */
static int exec(wububasic *b, char *s){
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncasecmp(p,"PRINT",5)==0 && (isspace((unsigned char)p[5])||!p[5])){
        p += 5;
        char acc[512]; size_t ai=0;
        (void)ai;
        /* concatenate comma/plus-separated items into a single expr for simplicity */
        strncpy(acc, p, sizeof acc -1); acc[sizeof acc-1]=0;
        char *end = acc + strlen(acc); while (end>acc && isspace((unsigned char)end[-1])) { *--end=0; }
        /* replace , and ; with + */
        for (char *c=acc; *c; c++) if (*c==','||*c==';') *c='+';
        int ok; char *val = eval(b, acc, &ok);
        if (!ok){ return -1; }
        out(b, val);
        out(b, "\n");
        free(val);
        return 0;
    }
    if (strncasecmp(p,"LET ",4)==0){
        p += 4;
        char *eq = strchr(p, '=');
        if (!eq){ seterr(b,"LET missing ="); return -1; }
        *eq = 0;
        char *name = p; while (*name && isspace((unsigned char)*name)) name++;
        char *nend = name + strlen(name); while (nend>name && isspace((unsigned char)nend[-1])) *--nend=0;
        int ok; char *val = eval(b, eq+1, &ok);
        if (!ok) return -1;
        int rc = wububasic_set_var(b, name, val);
        free(val);
        return rc==0?0:-1;
    }
    /* bare assignment: <var> = <expr>  (LET optional) */
    {
        char *eq = strchr(p, '=');
        if (eq && eq > p){
            /* ensure it's not part of a keyword (e.g. "IF ... = ...") by
             * checking the token before '=' is a simple identifier */
            char tok[64]; size_t tl = (size_t)(eq - p);
            char *ts = p; while (*ts && isspace((unsigned char)*ts)) ts++;
            tl = (size_t)(eq - ts);
            /* trim trailing whitespace from the identifier */
            while (tl > 0 && isspace((unsigned char)ts[tl-1])) tl--;
            if (tl > 0 && tl < sizeof tok){
                int ident = 1;
                for (size_t k=0;k<tl;k++) if (!isalnum((unsigned char)ts[k]) && ts[k]!='_' && ts[k]!='$') { ident=0; break; }
                if (ident){
                    memcpy(tok, ts, tl); tok[tl]=0;
                    /* don't treat GOTO 100 / other as assignment */
                    if (strncasecmp(tok,"GOTO",4)!=0 && strncasecmp(tok,"GOSUB",5)!=0){
                        int ok; char *val = eval(b, eq+1, &ok);
                        if (!ok) return -1;
                        int rc = wububasic_set_var(b, tok, val);
                        free(val);
                        return rc==0?0:-1;
                    }
                }
            }
        }
    }
    if (strncasecmp(p,"INPUT ",6)==0){
        p += 6;
        char *name = p; while (*name && isspace((unsigned char)*name)) name++;
        char *nend = name + strlen(name); while (nend>name && isspace((unsigned char)nend[-1])) *--nend=0;
        char buf[256];
        if (b->in_fn){ if (!b->in_fn(buf, sizeof buf, b->in_ud)){ seterr(b,"INPUT failed"); return -1; } }
        else { if (!fgets(buf, sizeof buf, stdin)){ seterr(b,"INPUT EOF"); return -1; }
               char *nl = strchr(buf,'\n'); if (nl) *nl=0; }
        int rc = wububasic_set_var(b, name, buf);
        return rc==0?0:-1;
    }
    if (strncasecmp(p,"IF ",3)==0){
        /* IF <x> <op> <y> THEN <stmt> [ELSE <stmt>] */
        char *then = strcasestr(p, " THEN ");
        char *elsep = strcasestr(p, " ELSE ");
        char *stmt_then = NULL, *stmt_else = NULL;
        char cond[256]; cond[0]=0;
        if (then){
            char condbuf[256];
            size_t cl = (size_t)(then - (p+3));
            if (cl >= sizeof condbuf) cl = sizeof condbuf-1;
            memcpy(condbuf, p+3, cl); condbuf[cl]=0;
            char *s = condbuf; while (*s && isspace((unsigned char)*s)) s++;
            strncpy(cond, s, sizeof cond-1); cond[sizeof cond-1]=0;
            stmt_then = then + 6; /* skip " THEN " */
            if (elsep && elsep > then){
                stmt_then = then + 6;
                stmt_else = elsep + 6;
                /* trim stmt_then up to elsep into a stack buffer */
                static char tbuf[512];
                size_t tlen = (size_t)(elsep - stmt_then);
                if (tlen >= sizeof tbuf) tlen = sizeof tbuf - 1;
                memcpy(tbuf, stmt_then, tlen); tbuf[tlen]=0;
                char *tmp = tbuf; while (*tmp && isspace((unsigned char)*tmp)) tmp++;
                stmt_then = tmp;
            }
        } else {
            seterr(b,"IF missing THEN"); return -1;
        }
        /* parse condition: a op b */
        char left[128], right[128];
        char *o1 = strstr(cond, "=");
        char *o2 = strstr(cond, "<>");
        char *o3 = strstr(cond, ">");
        char *o4 = strstr(cond, "<");
        char *opos = NULL; const char *opsym = NULL;
        if (o2){ opos = o2; opsym = "<>"; }
        else if (o1){ opos = o1; opsym = "="; }
        else if (o3){ opos = o3; opsym = ">"; }
        else if (o4){ opos = o4; opsym = "<"; }
        if (!opos){ seterr(b,"IF bad condition"); return -1; }
        size_t ln = (size_t)(opos - cond);
        size_t rn = strlen(opos + strlen(opsym));
        if (ln>=sizeof left) ln=sizeof left-1;
        if (rn>=sizeof right) rn=sizeof right-1;
        memcpy(left, cond, ln); left[ln]=0;
        memcpy(right, opos + strlen(opsym), rn); right[rn]=0;
        char *l2 = left; while (*l2 && isspace((unsigned char)*l2)) l2++;
        char *r2 = right; while (*r2 && isspace((unsigned char)*r2)) r2++;
        /* trim trailing whitespace from both operands */
        size_t ll = strlen(l2); while (ll && isspace((unsigned char)l2[ll-1])) l2[--ll]=0;
        size_t rr = strlen(r2); while (rr && isspace((unsigned char)r2[rr-1])) r2[--rr]=0;
        /* string or numeric compare? */
        int string_cmp = 0;
        if (*l2=='"' || *r2=='"') string_cmp = 1;
        int truth;
        if (string_cmp){
            const char *lv = l2, *rv = r2;
            var *vl = find_var(b, l2); if (vl) lv = vl->val;
            var *vr = find_var(b, r2); if (vr) rv = vr->val;
            int c = strcmp(lv, rv);
            if (!strcmp(opsym,"=")) truth = c==0;
            else if (!strcmp(opsym,"<>")) truth = c!=0;
            else if (!strcmp(opsym,">")) truth = c>0;
            else truth = c<0;
        } else {
            double a = num_of(b, l2), bb = num_of(b, r2);
            if (!strcmp(opsym,"=")) truth = a==bb;
            else if (!strcmp(opsym,"<>")) truth = a!=bb;
            else if (!strcmp(opsym,">")) truth = a>bb;
            else truth = a<bb;
        }
        char *execstmt = truth ? stmt_then : stmt_else;
        if (execstmt) return run_line(b, execstmt);
        return 0;
    }
    if (strncasecmp(p,"GOTO ",5)==0){
        int num = atoi(p+5);
        return goto_line(b, num) ? 0 : -1;
    }
    if (strncasecmp(p,"GOSUB ",6)==0){
        int num = atoi(p+6);
        int idx = find_line(b, num);
        if (idx < 0){ seterr(b,"GOSUB line not found"); return -1; }
        if (b->gosub_top == b->gosub_cap){
            size_t nc = b->gosub_cap?b->gosub_cap*2:16;
            int *ns = (int*)realloc(b->gosub_stack, nc*sizeof(int));
            if (!ns) return -1;
            b->gosub_stack = ns; b->gosub_cap = nc;
        }
        b->gosub_stack[b->gosub_top++] = b->pc;
        b->pc = idx;
        return 1; /* pc changed */
    }
    if (strncasecmp(p,"RETURN",6)==0){
        if (b->gosub_top == 0){ seterr(b,"RETURN without GOSUB"); return -1; }
        b->pc = b->gosub_stack[--b->gosub_top] + 1;
        return 1; /* pc changed */
    }
    if (strncasecmp(p,"FOR ",4)==0){
        /* FOR i = start TO end [STEP inc] ... NEXT i  -- execute body, then loop */
        char *eq = strchr(p+4, '=');
        if (!eq){ seterr(b,"FOR missing ="); return -1; }
        char *vn = p+4; while (*vn && isspace((unsigned char)*vn)) vn++;
        size_t vl2 = (size_t)(eq - vn);
        char var[64]; if (vl2 >= sizeof var) vl2 = sizeof var-1;
        memcpy(var, vn, vl2); var[vl2]=0;
        char *vle = var + vl2; while (vle>var && isspace((unsigned char)vle[-1])) *--vle=0;
        char *to = strcasestr(eq+1, " TO ");
        if (!to){ seterr(b,"FOR missing TO"); return -1; }
        char *ss = eq+1; while (*ss && isspace((unsigned char)*ss)) ss++;
        char startv[64]; size_t sllen = (size_t)(to - ss); if (sllen>=sizeof startv) sllen=sizeof startv-1;
        memcpy(startv, ss, sllen); startv[sllen]=0;
        char *sle = startv + sllen; while (sle>startv && isspace((unsigned char)sle[-1])) *--sle=0;
        double start = num_of(b, startv);
        char *step = strcasestr(to+4, " STEP ");
        double end, stepv = 1;
        if (step){
            char *es = to+4; while (*es && isspace((unsigned char)*es)) es++;
            char ev[64]; size_t ell = (size_t)(step - es); if (ell>=sizeof ev) ell=sizeof ev-1;
            memcpy(ev, es, ell); ev[ell]=0; char *ee=ev+ell; while (ee>ev && isspace((unsigned char)ee[-1])) *--ee=0;
            end = num_of(b, ev);
            stepv = num_of(b, step+6);
        } else {
            char *es = to+4; while (*es && isspace((unsigned char)*es)) es++;
            end = num_of(b, es);
        }
        char nv[32]; snprintf(nv, sizeof nv, "%g", start);
        wububasic_set_var(b, var, nv);
        /* find NEXT */
        int next_idx = -1;
        for (size_t i=b->pc+1;i<b->nlines;i++){
            char *t = b->lines[i].text; while (*t && isspace((unsigned char)*t)) t++;
            if (strncasecmp(t,"NEXT",4)==0){ next_idx=(int)i; break; }
        }
        if (next_idx<0){ seterr(b,"FOR without NEXT"); return -1; }
        int iter = 0;
        double cur = start;
        while ((stepv>=0 && cur<=end) || (stepv<0 && cur>=end)){
            if (++iter > 100000){ seterr(b,"FOR too many iterations"); return -1; }
            /* execute body lines from pc+1 to next_idx-1 */
            int saved_pc = b->pc;
            for (int i=b->pc+1; i<next_idx; i++){
                b->pc = i;
                char *copy = strdup(b->lines[i].text);
                int r = run_line(b, copy);
                free(copy);
                if (r != 0){ return -1; }
                if (b->pc != i){ /* control flow changed; bail out of FOR */
                    return r;
                }
            }
            b->pc = saved_pc;
            cur += stepv;
            char nv2[32]; snprintf(nv2, sizeof nv2, "%g", cur);
            wububasic_set_var(b, var, nv2);
        }
        /* set pc to NEXT line */
        b->pc = next_idx;
        return 1; /* pc changed */
    }
    if (strncasecmp(p,"END",3)==0 || strncasecmp(p,"STOP",4)==0){
        b->running = 0;
        b->pc = (int)b->nlines; /* terminate */
        return 1;
    }
    if (strncasecmp(p,"REM ",4)==0 || strncasecmp(p,"'",1)==0){
        return 0; /* comment */
    }
    /* unknown statement -> ignore (lenient macro mode) */
    return 0;
}

static int run_line(wububasic *b, char *src){
    char *p = src;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;
    return exec(b, p);
}

int wububasic_run(wububasic *b){
    if (!b) return -1;
    b->err[0]=0;
    b->running = 1;
    b->gosub_top = 0;
    b->pc = 0;
    while (b->pc >= 0 && (size_t)b->pc < b->nlines){
        if (!b->running) break;
        char *copy = strdup(b->lines[b->pc].text);
        if (!copy){ seterr(b,"oom"); return -1; }
        int r = run_line(b, copy);
        free(copy);
        if (r != 0){
            if (r < 0) return r;             /* hard error */
            /* r==1 means pc was set by a control-flow statement (GOTO/GOSUB/
             * RETURN/FOR/END); honor it and do not auto-increment. */
            continue;
        }
        b->pc++;
    }
    if (b->err[0]) return -1;
    return 0;
}
