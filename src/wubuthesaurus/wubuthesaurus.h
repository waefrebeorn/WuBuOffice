/* wubuthesaurus.h — thesaurus: word -> list of synonyms (a small built-in
 * dictionary + an extensible store). */
#ifndef WUBUTHESAURUS_H
#define WUBUTHESAURUS_H
#include <stddef.h>

typedef struct wubuthesaurus wubuthesaurus;

wubuthesaurus *wubuthesaurus_create(void);
void wubuthesaurus_destroy(wubuthesaurus *t);

/* Look up synonyms for `word` (case-insensitive). Returns a NULL-terminated
 * array of strings (owned by t), or NULL if the word has no entry. */
const char **wubuthesaurus_lookup(const wubuthesaurus *t, const char *word);

/* Add an entry. `words` is NULL-terminated. Returns 0 on success. */
int wubuthesaurus_add(wubuthesaurus *t, const char *word, const char **words);

size_t wubuthesaurus_count(const wubuthesaurus *t);

#endif
