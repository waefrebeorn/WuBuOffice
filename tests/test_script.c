/* test_script.c -- wubuscript acceptance test (sandboxed formula host). */
#include "script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,m) do { if(!(c)){ printf("FAIL: %s\n", m); fails++; } } while(0)

/* resolver: x=10, y=5, pi=3.14 */
static int resolve(const char *name, double *out, void *ctx){
    (void)ctx;
    if (strcmp(name,"x")==0){ *out=10; return 0; }
    if (strcmp(name,"y")==0){ *out=5; return 0; }
    if (strcmp(name,"pi")==0){ *out=3.14; return 0; }
    return -1;
}

int main(void){
    double v;
    CHECK(script_eval("2 + 3 * 4", resolve, NULL, &v) == 0, "arith parses");
    CHECK(v == 14, "2+3*4 == 14");

    CHECK(script_eval("SUM(1, 2, 3, 4)", resolve, NULL, &v) == 0, "SUM ok");
    CHECK(v == 10, "SUM(1..4) == 10");

    CHECK(script_eval("x * y", resolve, NULL, &v) == 0, "variable resolves");
    CHECK(v == 50, "x*y == 50");

    CHECK(script_eval("x + missing", resolve, NULL, &v) == -1, "unknown var -> fail");

    /* string result */
    char *s = script_eval_str("2 * 21", resolve, NULL);
    CHECK(s != NULL, "eval_str ok");
    if (s){ CHECK(strcmp(s, "42")==0 || atof(s)==42.0, "eval_str '42'"); free(s); }

    /* NULL guard */
    CHECK(script_eval(NULL, resolve, NULL, &v) == -1, "NULL expr -> fail");

    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuscript (arith, SUM, vars, unknown-var fail, str)\n");
    return 0;
}
