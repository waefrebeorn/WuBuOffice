/* cite.h -- bibliography / citation store (DOC-68). Holds citation entries
 * (key, type, title, authors, year) and renders an ordered bibliography list
 * plus inline citations "(Author, Year)". Opaque. */
#ifndef WUBUCITE_H
#define WUBUCITE_H

typedef struct Cite Cite;

Cite *cite_create(void);
void  cite_destroy(Cite *c);

/* Add an entry. `key` is the citation key (e.g. "smith2020"). Returns 1 ok. */
int   cite_add(Cite *c, const char *key, const char *type,
               const char *title, const char *authors, int year);

/* Inline citation text for `key`, e.g. "(Smith, 2020)". Returns malloc'd
 * string (caller frees), or NULL if unknown. */
char *cite_inline(Cite *c, const char *key);

/* Render the full bibliography as text (one entry per line, numbered).
 * Returns malloc'd string (caller frees). */
char *cite_bibliography(Cite *c);

int   cite_count(const Cite *c);

#endif /* WUBUCITE_H */
