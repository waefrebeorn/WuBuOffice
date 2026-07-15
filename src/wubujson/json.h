/* json.h -- minimal JSON value model: parse + emit. Self-contained C11.
 *
 * Enough for the agent protocol (objects, arrays, strings, numbers, bool,
 * null). No float formatting beyond what printf gives; emits紧凑 (no
 * insignificant whitespace). Reused by wubuOS later. Opaque node type. */
#ifndef WUBUPAD_JSON_H
#define WUBUPAD_JSON_H

#include <stddef.h>

typedef enum {
    J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ
} JType;

typedef struct JVal JVal;

JVal *j_null(void);
JVal *j_bool(int v);
JVal *j_num(double v);
JVal *j_str(const char *s);          /* copies s */
JVal *j_arr(void);                    /* empty array */
JVal *j_obj(void);                    /* empty object */

void  j_free(JVal *v);

/* Build helpers (take ownership of child pointers where noted). */
void  j_arr_push(JVal *arr, JVal *child);          /* owns child */
void  j_obj_put(JVal *obj, const char *key, JVal *val); /* copies key; owns val */

JType j_type(const JVal *v);
int        j_as_bool(const JVal *v);
double     j_as_num(const JVal *v);
const char *j_as_str(const JVal *v);                /* valid for J_STR */
size_t     j_len(const JVal *v);                    /* arr/obj length */
const JVal *j_arr_at(const JVal *arr, size_t i);
const JVal *j_obj_get(const JVal *obj, const char *key);  /* NULL if absent */
const char *j_obj_key_at(const JVal *obj, size_t i);     /* for iteration */

/* Serialize to a malloc'd NUL-terminated string (caller frees). NULL on OOM. */
char *j_emit(const JVal *v);

/* Parse a NUL-terminated JSON string. Returns NULL on syntax error.
 * `out_end` (if non-NULL) receives a pointer just past the parsed value. */
JVal *j_parse(const char *text, const char **out_end);

#endif /* WUBUPAD_JSON_H */
