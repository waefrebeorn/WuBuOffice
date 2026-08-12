/* wubumailexport.h — mail export: render a doc/letter into an RFC-5322 mail
 * message (To/From/Subject/Date headers + plain-text body). */
#ifndef WUBUMAILEXPORT_H
#define WUBUMAILEXPORT_H
#include <stddef.h>

typedef struct {
    char to[256], from[256], subject[256];
    char *body;   /* malloc'd plain-text body; caller frees */
    size_t bodylen;
} wubumailexport;

/* Build an RFC-5322 message from fields. `body` is copied. Returns 0. */
int wubumailexport_build(wubumailexport *m, const char *to, const char *from,
                         const char *subject, const char *body);

/* Render the complete message (headers + blank line + body) as one malloc'd
 * NUL-terminated string. Caller frees. Returns NULL on error. */
char *wubumailexport_render(const wubumailexport *m);

void wubumailexport_free(wubumailexport *m);

#endif
