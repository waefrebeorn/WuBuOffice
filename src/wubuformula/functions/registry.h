/* WuBuOffice -- wubuformula/functions/registry
 * Public face of the function registry. Category modules (math, logic, text,
 * lookup, datefin, stat) publish their functions through a registrar callback;
 * the evaluator looks names up here. Single source of truth for the function
 * set so there is exactly one table and no per-file copies.
 *
 * Clean-room, from-scratch (SLERM): no third-party spreadsheet code. */

#ifndef WUBUFORMULA_FUNCS_REGISTRY_H
#define WUBUFORMULA_FUNCS_REGISTRY_H

#include "funcs.h"

/* A registrar callback used by category modules to publish functions into the
 * shared table without each module knowing about the table's storage. */
typedef void (*wubu_func_registrar)(const char *name, wubu_func_impl fn);

/* Publish one function (called by the category registrars). */
void wubu_func_register(const char *name, wubu_func_impl fn);

/* Register every built-in function once. Idempotent. Called automatically by
 * wubu_func_lookup / wubu_func_count on first use, so callers need not call
 * it explicitly. */
void wubu_formula_register_all(void);

/* Category registrars (one per source module). Declared here so registry.c can
 * invoke them; each is defined in its own translation unit. */
void wubu_register_logic(wubu_func_registrar reg);
void wubu_register_math(wubu_func_registrar reg);
void wubu_register_text(wubu_func_registrar reg);
void wubu_register_lookup(wubu_func_registrar reg);
void wubu_register_datefin(wubu_func_registrar reg);
void wubu_register_stat(wubu_func_registrar reg);

#endif /* WUBUFORMULA_FUNCS_REGISTRY_H */
