/* WuBuOffice -- wubuformula/functions/registry
 * Central function table. Category modules register into it; the evaluator
 * looks names up here. Single source of truth for the function set.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#include "registry.h"
#include "value_util.h"

#include <string.h>
#include <stdlib.h>

#define WUBU_FUNC_MAX 256

static wubu_func_impl table[WUBU_FUNC_MAX];
static char names[WUBU_FUNC_MAX][32];
static int nfuncs = 0;
static int registered = 0;

void wubu_func_register(const char *name, wubu_func_impl fn) {
    if (nfuncs >= WUBU_FUNC_MAX) return;
    strncpy(names[nfuncs], name, 31);
    names[nfuncs][31] = '\0';
    table[nfuncs] = fn;
    nfuncs++;
}

void wubu_formula_register_all(void) {
    if (registered) return;
    registered = 1;
    wubu_register_logic(wubu_func_register);
    wubu_register_math(wubu_func_register);
    wubu_register_text(wubu_func_register);
    wubu_register_lookup(wubu_func_register);
    wubu_register_datefin(wubu_func_register);
    wubu_register_stat(wubu_func_register);
}

wubu_func_impl wubu_func_lookup(const char *name) {
    wubu_formula_register_all();
    for (int i = 0; i < nfuncs; i++)
        if (wubu_strcasecmp(names[i], name) == 0) return table[i];
    return NULL;
}

int wubu_func_count(void) {
    wubu_formula_register_all();
    return nfuncs;
}
