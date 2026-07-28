/* redact.h -- redaction set + redacted-text export (EXP-90 partner). Mark
 * character ranges [start,end) of a document text as redacted; produce a copy
 * with those ranges replaced by a block character. Opaque. */
#ifndef WUBUREDACT_H
#define WUBUREDACT_H
#include <stddef.h>

typedef struct Redact Redact;

Redact *redact_create(void);
void    redact_destroy(Redact *r);

/* Mark [start,end) (byte offsets) as redacted. Returns 1 on success. */
int     redact_mark(Redact *r, size_t start, size_t end);
/* Number of redaction ranges. */
int     redact_count(const Redact *r);

/* Produce a redacted copy of `text` (malloc'd, caller frees) with marked
 * ranges replaced by '█'. Returns NULL on error (e.g. range out of bounds). */
char *redact_apply(const Redact *r, const char *text);

#endif /* WUBUREDACT_H */
