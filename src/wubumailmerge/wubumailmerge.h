/* wubumailmerge.h — mail merge: fill a template's ${field} placeholders from
 * a set of data records. Each record produces one merged document. */
#ifndef WUBUMAILMERGE_H
#define WUBUMAILMERGE_H
#include <stddef.h>

typedef struct wubumailmerge wubumailmerge;

/* A single field value: field name -> value. */
typedef struct {
    const char *field;
    const char *value;
} wubumailmerge_field;

/* One data record = array of fields (terminated by field==NULL). */
typedef const wubumailmerge_field *wubumailmerge_record;

wubumailmerge *wubumailmerge_create(void);
void wubumailmerge_destroy(wubumailmerge *m);

/* Add a record. `fields` is NULL-terminated. Copies are made. Returns 0. */
int wubumailmerge_add_record(wubumailmerge *m, const wubumailmerge_field *fields);

size_t wubumailmerge_record_count(const wubumailmerge *m);

/* Merge the template for the i-th record into `out` (caller-freed).
 * Recognizes ${field} and also bare {field} placeholders. Unknown fields are
 * left as-is. Returns the new string or NULL on OOM. */
char *wubumailmerge_merge(const wubumailmerge *m, size_t i, const char *template);

#endif
