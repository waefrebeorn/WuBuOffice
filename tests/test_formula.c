/* WuBuOffice -- tests/test_formula
 * Validates the wubuformula engine: parsing, evaluation, functions, ranges,
 * cell references, error values, and circular-reference detection.
 *
 * The resolver is a tiny in-memory grid so formulas can reference cells. */

#include "value.h"
#include "eval.h"
#include "funcs.h"
#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* forward declarations for the multi-sheet resolver used by check_num_x */
typedef struct { const char *sheet; int col, row; wubuval v; } sslot;
static int ssheet_resolve(void *ctx, const wubucell_ref *ref, wubuval *out);

/* --- tiny grid resolver --- */
typedef struct { int col, row; wubuval v; } cell_slot;
static cell_slot GRID[1024];
static int GRID_N = 0;

/* formula-aware resolver's persistent grid (declared early so grid_reset can
 * see it). Cell text starting with '=' is a formula; evaluated recursively
 * with cycle detection via a "visiting" set keyed by col,row. */
typedef struct { int col, row; int state; wubuval v; } fcell; /* state 0=clean 1=visiting 2=done */
static fcell FGRID[1024];
static int FGRID_N = 0;

/* Stores a COPY of v in the grid. For string values it TAKES OWNERSHIP of v->str
 * (transfers the pointer, nulling the caller's copy) so there is exactly one
 * allocation and the caller's local is safe to reuse without leaking. */
static void grid_set(int col, int row, wubuval *v) {
    for (int i = 0; i < GRID_N; i++)
        if (GRID[i].col == col && GRID[i].row == row) { wubuval_free(&GRID[i].v); GRID[i].v = *v; if (v->kind==WV_STR) GRID[i].v.str = v->str; v->str = NULL; return; }
    GRID[GRID_N].col = col; GRID[GRID_N].row = row; GRID[GRID_N].v = *v;
    if (v->kind==WV_STR) GRID[GRID_N].v.str = v->str;
    v->str = NULL;
    GRID_N++;
}

/* Free every strdup'd value currently held in GRID/FGRID, then reset the slot
 * count. Used before the test re-purposes the grid, so the previous grid's
 * heap strings are not orphaned (LeakSanitizer would flag them). */
static void grid_reset(void) {
    for (int i = 0; i < GRID_N; i++)
        if (GRID[i].v.kind == WV_STR && GRID[i].v.str) { free(GRID[i].v.str); GRID[i].v.str = NULL; }
    for (int i = 0; i < FGRID_N; i++)
        if (FGRID[i].v.kind == WV_STR && FGRID[i].v.str) { free(FGRID[i].v.str); FGRID[i].v.str = NULL; }
    GRID_N = 0; FGRID_N = 0;
}

/* Free the strdup'd value strings held in the test's persistent grids so the
 * suite is clean under LeakSanitizer. Defined below main(). */
static void cleanup_formula_test(void);

static int grid_resolve(void *ctx, const wubucell_ref *ref, wubuval *out) {
    (void)ctx;
    for (int i = 0; i < GRID_N; i++)
        if (GRID[i].col == ref->col && GRID[i].row == ref->row) { wubuval_copy(out, &GRID[i].v); return 0; }
    wubuval_set_empty(out);
    return 0; /* empty, not error */
}

/* --- helpers --- */
static int check_num(const char *f, double expect, double tol) {
    wubuval v; memset(&v, 0, sizeof v);
    int rc = wubu_formula_eval(f, grid_resolve, NULL, &v);
    int ok = (rc == 0 && v.kind == WV_NUM && fabs(v.num - expect) <= tol);
    if (!ok) printf("  FAIL num: \"%s\" -> kind=%d num=%g (want %g)\n", f, v.kind, v.num, expect);
    wubuval_free(&v);
    return ok;
}
static int check_str(const char *f, const char *expect) {
    wubuval v; memset(&v, 0, sizeof v);
    int rc = wubu_formula_eval(f, grid_resolve, NULL, &v);
    int ok = (rc == 0 && v.kind == WV_STR && strcmp(v.str ? v.str : "", expect) == 0);
    if (!ok) printf("  FAIL str: \"%s\" -> kind=%d str=%s (want %s)\n", f, v.kind, v.str ? v.str : "(null)", expect);
    wubuval_free(&v);
    return ok;
}
static int check_err(const char *f, wubuval_err e) {
    wubuval v; memset(&v, 0, sizeof v);
    int rc = wubu_formula_eval(f, grid_resolve, NULL, &v);
    int ok = (rc == 0 && v.kind == WV_ERR && v.err == e);
    if (!ok) printf("  FAIL err: \"%s\" -> kind=%d err=%d (want err=%d)\n", f, v.kind, v.err, e);
    wubuval_free(&v);
    return ok;
}
static int check_bool(const char *f, int expect) {
    wubuval v; memset(&v, 0, sizeof v);
    int rc = wubu_formula_eval(f, grid_resolve, NULL, &v);
    int ok = (rc == 0 && v.kind == WV_BOOL && v.boolean == expect);
    if (!ok) printf("  FAIL bool: \"%s\" -> kind=%d bool=%d (want %d)\n", f, v.kind, v.boolean, expect);
    wubuval_free(&v);
    return ok;
}
/* like check_num but uses the multi-sheet resolver (for Sheet!A1 refs) */
static int check_num_x(const char *f, double expect, double tol) {
    wubuval v; memset(&v, 0, sizeof v);
    int rc = wubu_formula_eval(f, ssheet_resolve, NULL, &v);
    int ok = (rc == 0 && v.kind == WV_NUM && fabs(v.num - expect) <= tol);
    if (!ok) printf("  FAIL numx: \"%s\" -> kind=%d num=%g (want %g)\n", f, v.kind, v.num, expect);
    wubuval_free(&v);
    return ok;
}

/* formula-aware resolver: cell text starting with '=' is a formula; evaluates
 * it recursively with cycle detection via a "visiting" set keyed by col,row. */

static fcell *fgrid_find(int col, int row) {
    for (int i = 0; i < FGRID_N; i++) if (FGRID[i].col==col && FGRID[i].row==row) return &FGRID[i];
    return NULL;
}

static int formula_resolve(void *ctx, const wubucell_ref *ref, wubuval *out) {
    (void)ctx;
    fcell *c = fgrid_find(ref->col, ref->row);
    if (!c) { wubuval_set_empty(out); return 0; }
    if (c->state == 2) { wubuval_copy(out, &c->v); return 0; }
    if (c->state == 1) { wubuval_set_err(out, WERR_CYCLE); return 0; } /* cycle */
    /* literal or formula? */
    if (c->v.kind == WV_STR && c->v.str && c->v.str[0] == '=') {
        c->state = 1; /* mark visiting */
        wubuval res; memset(&res, 0, sizeof res);
        int rc = wubu_formula_eval(c->v.str + 1, formula_resolve, NULL, &res);
        c->state = 2;
        wubuval_free(&c->v);   /* release the strdup'd formula text before caching the result */
        c->v = res; /* cache */
        if (c->v.kind == WV_STR) c->v.str = strdup(c->v.str ? c->v.str : "");
        wubuval_copy(out, &c->v);
        (void)rc;
        return 0;
    }
    wubuval_copy(out, &c->v);
    c->state = 2;
    return 0;
}

/* multi-sheet grid resolver for cross-sheet (Sheet!A1) reference tests */
static sslot SGRID[32];
static int SGRID_N = 0;

static int ssheet_resolve(void *ctx, const wubucell_ref *ref, wubuval *out) {
    (void)ctx;
    const char *want = (ref->sheet == -2) ? ref->sheet_name : "";
    for (int i = 0; i < SGRID_N; i++)
        if (strcmp(SGRID[i].sheet, want) == 0 &&
            SGRID[i].col == ref->col && SGRID[i].row == ref->row) {
            wubuval_copy(out, &SGRID[i].v);
            return 0;
        }
    wubuval_set_empty(out);
    return 0;
}

int main(void) {
    int fails = 0;

    /* arithmetic + precedence */
    fails += !check_num("1+2*3", 7, 1e-9);
    fails += !check_num("(1+2)*3", 9, 1e-9);
    fails += !check_num("2^10", 1024, 1e-9);
    fails += !check_num("-5+3", -2, 1e-9);
    fails += !check_num("10/4", 2.5, 1e-9);
    fails += !check_num("7%", 0.07, 1e-9);
    fails += !check_num("2^3^2", 512, 1e-9);          /* right-associative */
    fails += !check_num("1+2+3+4+5", 15, 1e-9);

    /* string concat + comparison */
    fails += !check_str("\"foo\"&\"bar\"", "foobar");
    fails += !check_bool("1=1", 1);
    fails += !check_bool("1<>2", 1);
    fails += !check_bool("3>2", 1);
    fails += !check_bool("2>=2", 1);
    fails += !check_bool("2<=1", 0);

    /* functions */
    fails += !check_num("SUM(1,2,3,4)", 10, 1e-9);
    fails += !check_num("AVERAGE(2,4,6)", 4, 1e-9);
    fails += !check_num("MIN(3,1,2)", 1, 1e-9);
    fails += !check_num("MAX(3,1,2)", 3, 1e-9);
    fails += !check_num("ROUND(3.14159,2)", 3.14, 1e-9);
    fails += !check_num("ABS(-7)", 7, 1e-9);
    fails += !check_num("MOD(10,3)", 1, 1e-9);
    fails += !check_num("POWER(2,8)", 256, 1e-9);
    fails += !check_num("SQRT(16)", 4, 1e-9);
    fails += !check_num("INT(3.9)", 3, 1e-9);
    fails += !check_num("PI()*2", 2*3.14159265358979323846, 1e-9);
    fails += !check_num("PRODUCT(2,3,4)", 24, 1e-9);
    fails += !check_num("COUNT(1,2,\"x\",4)", 3, 1e-9);

    /* logic */
    fails += !check_num("IF(1>0,100,200)", 100, 1e-9);
    fails += !check_num("IF(1<0,100,200)", 200, 1e-9);
    fails += !check_bool("AND(1,1,0)", 0);
    fails += !check_bool("OR(0,0,1)", 1);
    fails += !check_bool("NOT(FALSE)", 1);
    fails += !check_str("IFERROR(1/0,\"oops\")", "oops");

    /* text */
    fails += !check_str("CONCATENATE(\"a\",\"b\",\"c\")", "abc");
    fails += !check_str("UPPER(\"abc\")", "ABC");
    fails += !check_str("LOWER(\"ABC\")", "abc");
    fails += !check_num("LEN(\"hello\")", 5, 1e-9);
    fails += !check_str("LEFT(\"hello\",2)", "he");
    fails += !check_str("RIGHT(\"hello\",2)", "lo");
    fails += !check_str("MID(\"hello\",2,3)", "ell");
    fails += !check_str("REPT(\"ab\",3)", "ababab");

    /* error values */
    fails += !check_err("1/0", WERR_DIV0);
    fails += !check_err("FOOBAR(1)", WERR_NAME);
    fails += !check_err("SQRT(-1)", WERR_NUM);

    /* cell references + ranges */
    grid_reset();
    wubuval v; memset(&v,0,sizeof v);
    wubuval_set_num(&v, 10); grid_set(1,1,&v);   /* A1 */
    wubuval_set_num(&v, 20); grid_set(1,2,&v);   /* A2 */
    wubuval_set_num(&v, 30); grid_set(1,3,&v);   /* A3 */
    wubuval_set_num(&v, 5);  grid_set(2,1,&v);   /* B1 */
    fails += !check_num("A1", 10, 1e-9);
    fails += !check_num("A1+A2+A3", 60, 1e-9);
    fails += !check_num("SUM(A1:A3)", 60, 1e-9);
    fails += !check_num("SUM(A1:A3,B1)", 65, 1e-9);
    fails += !check_num("A1*2", 20, 1e-9);
    fails += !check_num("SUM(A1:A3)/3", 20, 1e-9);

    /* cross-sheet qualified references (Sheet!A1). Regression guard: the lexer
     * must retain the Sheet! prefix in the REF token text, else the ref resolves
     * to the current sheet (Other!A1*2 was wrongly 20, want 198). */
    SGRID_N = 0;
    wubuval sv; memset(&sv,0,sizeof sv);
    SGRID[SGRID_N].sheet="Other"; SGRID[SGRID_N].col=1; SGRID[SGRID_N].row=1; wubuval_set_num(&sv,99); SGRID[SGRID_N].v=sv; SGRID_N++;
    SGRID[SGRID_N].sheet="Other"; SGRID[SGRID_N].col=1; SGRID[SGRID_N].row=2; wubuval_set_num(&sv, 7); SGRID[SGRID_N].v=sv; SGRID_N++;
    SGRID[SGRID_N].sheet="";     SGRID[SGRID_N].col=1; SGRID[SGRID_N].row=1; wubuval_set_num(&sv, 5); SGRID[SGRID_N].v=sv; SGRID_N++;
    fails += !check_num_x("Other!A1*2", 198, 1e-9);
    fails += !check_num_x("A1+Other!A1", 104, 1e-9);
    fails += !check_num_x("SUM(Other!A1,Other!A2)", 106, 1e-9);  /* 99+7 */
    fails += !check_num_x("Other!A1:A1", 99, 1e-9);               /* single-cell range on Other */


    /* ---- lookups & conditional aggregates over a real range ----
     * Build a lookup table at A10:B12 and a criteria table at D1:D3. */
    grid_reset();
    wubuval gv; memset(&gv,0,sizeof gv);
    /* id->name table: A10=1 "apple", A11=2 "banana", A12=3 "cherry" (col B) */
    wubuval_set_num(&gv,1);  grid_set(1,10,&gv);
    wubuval_set_str(&gv,"apple");  grid_set(2,10,&gv);
    wubuval_set_num(&gv,2);  grid_set(1,11,&gv);
    wubuval_set_str(&gv,"banana"); grid_set(2,11,&gv);
    wubuval_set_num(&gv,3);  grid_set(1,12,&gv);
    wubuval_set_str(&gv,"cherry"); grid_set(2,12,&gv);
    fails += !check_num("VLOOKUP(3,A10:B12,1)", 3, 1e-9);
    fails += !check_str("VLOOKUP(1,A10:B12,2)", "apple");
    fails += !check_str("VLOOKUP(2,A10:B12,2)", "banana");
    fails += !check_str("VLOOKUP(3,A10:B12,2)", "cherry");
    /* SUMIF / COUNTIF over D1:D3 = {10,20,30} */
    grid_reset();
    wubuval sv2; memset(&sv2,0,sizeof sv2);
    wubuval_set_num(&sv2,10); grid_set(4,1,&sv2);
    wubuval_set_num(&sv2,20); grid_set(4,2,&sv2);
    wubuval_set_num(&sv2,30); grid_set(4,3,&sv2);
    fails += !check_num("SUMIF(D1:D3,\">15\")", 50, 1e-9);   /* 20+30 */
    fails += !check_num("COUNTIF(D1:D3,\">15\")", 2, 1e-9);   /* 20,30 */
    fails += !check_num("SUMIF(D1:D3,10)", 10, 1e-9);        /* exact match */
    fails += !check_num("COUNTIF(D1:D3,30)", 1, 1e-9);
    fails += !check_num("SUMIF(D1:D3,\"<20\")", 10, 1e-9);   /* 10 */

    /* ---- text functions ---- */
    fails += !check_str("SUBSTITUTE(\"a-b-c\",\"-\",\"+\")", "a+b+c");
    fails += !check_str("SUBSTITUTE(\"a-b-c\",\"-\",\"+\",1)", "a+b-c");
    fails += !check_num("FIND(\"bc\",\"abcd\")", 2, 1e-9);
    fails += !check_num("SEARCH(\"BC\",\"ABCD\")", 2, 1e-9);
    fails += !check_str("PROPER(\"hello world\")", "Hello World");
    fails += !check_str("REPLACE(\"abcdef\",2,3,\"XYZ\")", "aXYZef");

    /* ---- date / financial / lookup (INDEX/MATCH/CHOOSE) ---- */
    fails += !check_num("DATE(2000,1,1)", 36526, 1e-9);
    fails += !check_num("DATE(2026,7,13)", 46216, 1e-9);
    fails += !check_num("DATE(1900,3,1)", 61, 1e-9);     /* 1900 leap-bug shift */
    fails += !check_num("YEAR(DATE(2026,7,13))", 2026, 1e-9);
    fails += !check_num("MONTH(DATE(2026,7,13))", 7, 1e-9);
    fails += !check_num("DAY(DATE(2026,7,13))", 13, 1e-9);
    fails += !check_num("DATE(YEAR(DATE(2026,7,13)),MONTH(DATE(2026,7,13)),DAY(DATE(2026,7,13)))", 46216, 1e-9); /* round-trip */
    /* TODAY/NOW: just a sane serial in a plausible range (>= 45000) */
    {
        wubuval v; memset(&v,0,sizeof v);
        wubu_formula_eval("TODAY()", grid_resolve, NULL, &v);
        int ok = (v.kind==WV_NUM && v.num >= 45000);
        if (!ok) printf("  FAIL TODAY: kind=%d num=%g\n", v.kind, v.num);
        fails += !ok; wubuval_free(&v);
        wubuval w; memset(&w,0,sizeof w);
        wubu_formula_eval("NOW()", grid_resolve, NULL, &w);
        ok = (w.kind==WV_NUM && w.num >= 45000);
        if (!ok) printf("  FAIL NOW: kind=%d num=%g\n", w.kind, w.num);
        fails += !ok; wubuval_free(&w);
    }
    /* PMT: 5%/yr, 10 yrs, $1000 pv -> monthly payment (rate/12, nper*12) */
    fails += !check_num("PMT(0.05/12, 10*12, 1000)", -10.6066, 1e-3);
    fails += !check_num("FV(0.05/12, 10*12, -100)", 15528.2, 1.0);  /* save 100/mo 10y @5% */
    /* CHOOSE */
    fails += !check_num("CHOOSE(2, 10, 20, 30)", 20, 1e-9);
    fails += !check_num("CHOOSE(1, 10, 20, 30)", 10, 1e-9);
    /* INDEX / MATCH need a range resolver; reuse GRID as a table at E1:E3 */
    grid_reset();
    wubuval gi; memset(&gi,0,sizeof gi);
    wubuval_set_num(&gi, 100); grid_set(5,1,&gi);  /* E1 */
    wubuval_set_num(&gi, 200); grid_set(5,2,&gi);  /* E2 */
    wubuval_set_num(&gi, 300); grid_set(5,3,&gi);  /* E3 */
    fails += !check_num("INDEX(E1:E3, 2)", 200, 1e-9);
    fails += !check_num("MATCH(300, E1:E3)", 3, 1e-9);
    fails += !check_num("INDEX(E1:E3, MATCH(200, E1:E3))", 200, 1e-9);
    /* AVERAGEIF over D1:D3 = {10,20,30} */
    grid_reset();
    wubuval gj; memset(&gj,0,sizeof gj);
    wubuval_set_num(&gj, 10); grid_set(4,1,&gj);
    wubuval_set_num(&gj, 20); grid_set(4,2,&gj);
    wubuval_set_num(&gj, 30); grid_set(4,3,&gj);
    fails += !check_num("AVERAGEIF(D1:D3, \">15\")", 25, 1e-9);  /* (20+30)/2 */
    fails += !check_num("AVERAGEIF(D1:D3, 10)", 10, 1e-9);
    /* The newly-split stat / multi-criteria / logical / text-join functions */
    fails += !check_num("MEDIAN(3,1,2)", 2, 1e-9);
    fails += !check_num("MEDIAN(10,20,30,40)", 25, 1e-9);
    fails += !check_num("MODE(1,2,2,3)", 2, 1e-9);
    fails += !check_num("STDEV(2,4,4,4,5,5,7,9)", 2.1381, 1e-3);
    fails += !check_num("VAR(2,4,4,4,5,5,7,9)", 4.5714, 1e-3);
    fails += !check_num("LARGE(D1:D3, 1)", 30, 1e-9);   /* D1:D3={10,20,30} */
    fails += !check_num("SMALL(D1:D3, 2)", 20, 1e-9);
    fails += !check_num("SUMPRODUCT(D1:D3, D1:D3)", 10*10+20*20+30*30, 1e-9);
    fails += !check_num("SUMIFS(D1:D3, D1:D3, \">15\")", 50, 1e-9);  /* 20+30 */
    fails += !check_num("COUNTIFS(D1:D3, \">15\")", 2, 1e-9);
    fails += !check_num("IFS(0, 100, 1, 200)", 200, 1e-9);
    fails += !check_num("SWITCH(2, 1, 10, 2, 20, 99)", 20, 1e-9);
    fails += !check_str("TEXTJOIN(\"-\", 1, \"a\", \"b\", \"c\")", "a-b-c");
    fails += !check_str("CONCAT(\"foo\", \"bar\")", "foobar");
    fails += !check_num("WEEKDAY(DATE(2026,7,13))", 2, 1e-9);   /* 2026-07-13 is a Monday (type1=Sun..Sat -> 2) */
    fails += !check_num("EDATE(DATE(2026,1,15), 1)", 46068, 1e-9); /* 2026-02-15 */
    fails += !check_num("EOMONTH(DATE(2026,2,10), 0)", 46081, 1e-9); /* last day of Feb 2026 = 2026-02-28 */


    /* nested formula referencing cells (rebuild the A1:A3/B1 grid that the
     * earlier cell-reference tests populated, since the lookup block above
     * reset GRID_N) */
    grid_reset();
    wubuval g3; memset(&g3,0,sizeof g3);
    wubuval_set_num(&g3, 10); grid_set(1,1,&g3);   /* A1 */
    wubuval_set_num(&g3, 20); grid_set(1,2,&g3);   /* A2 */
    wubuval_set_num(&g3, 30); grid_set(1,3,&g3);   /* A3 */
    wubuval_set_num(&g3, 5);  grid_set(2,1,&g3);   /* B1 */
    fails += !check_num("SUM(A1:A3)+B1*4", 80, 1e-9);

    /* circular reference: A1 = B1+1, B1 = A1+1 */
    grid_reset();
    fcell fa = { .col=1, .row=1, .state=0 }; wubuval_set_str(&fa.v, "=B1+1"); FGRID[FGRID_N++] = fa;
    fcell fb = { .col=2, .row=1, .state=0 }; wubuval_set_str(&fb.v, "=A1+1"); FGRID[FGRID_N++] = fb;
    {
        wubuval cv; memset(&cv,0,sizeof cv);
        /* evaluate A1; should detect cycle and return #CYCLE! */
        wubucell_ref r = { -1, 1, 1, 0,0,0, "" };
        formula_resolve(NULL, &r, &cv);
        int ok = (cv.kind == WV_ERR && cv.err == WERR_CYCLE);
        if (!ok) printf("  FAIL cycle: kind=%d err=%d (want CYCLE)\n", cv.kind, cv.err);
        fails += !ok;
        wubuval_free(&cv);
    }

    /* chained formula: A1=2, B1=A1*3, C1=B1+1 => 7 */
    grid_reset();
    fcell c1 = { .col=1, .row=1, .state=0 }; wubuval_set_num(&c1.v, 2); FGRID[FGRID_N++] = c1;
    fcell c2 = { .col=2, .row=1, .state=0 }; wubuval_set_str(&c2.v, "=A1*3"); FGRID[FGRID_N++] = c2;
    fcell c3 = { .col=3, .row=1, .state=0 }; wubuval_set_str(&c3.v, "=B1+1"); FGRID[FGRID_N++] = c3;
    {
        wubuval cv; memset(&cv,0,sizeof cv);
        wubucell_ref r = { -1, 3, 1, 0,0,0, "" };
        formula_resolve(NULL, &r, &cv);
        int ok = (cv.kind == WV_NUM && fabs(cv.num - 7) < 1e-9);
        if (!ok) printf("  FAIL chain: kind=%d num=%g (want 7)\n", cv.kind, cv.num);
        fails += !ok;
        wubuval_free(&cv);
    }

    if (fails) { printf("FORMULA TESTS FAILED: %d\n", fails); cleanup_formula_test(); return 1; }
    printf("FORMULA TESTS OK (%d functions registered)\n", wubu_func_count());
    cleanup_formula_test();
    return 0;
}

/* Free the strdup'd value strings held in the test's persistent grids so the
 * suite is clean under LeakSanitizer. The grids themselves are static. */
static void cleanup_formula_test(void) {
    for (int i = 0; i < GRID_N; i++)
        if (GRID[i].v.kind == WV_STR && GRID[i].v.str) { free(GRID[i].v.str); GRID[i].v.str = NULL; }
    for (int i = 0; i < FGRID_N; i++)
        if (FGRID[i].v.kind == WV_STR && FGRID[i].v.str) { free(FGRID[i].v.str); FGRID[i].v.str = NULL; }
}
