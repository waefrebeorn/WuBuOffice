/* wubuindex.h — back-of-book index generator: given document text and a set
 * of index terms, produce alphabetized index entries with page/occurrence
 * references. */
#ifndef WUBUINDEX_H
#define WUBUINDEX_H
#include <stddef.h>

typedef struct {
    char *term;     /* owned, the normalized index term */
    int *pages;     /* sorted ascending list of occurrence page numbers */
    size_t npages;
    size_t cap;
} wubuindex_entry;

typedef struct {
    wubuindex_entry *entries;
    size_t n, cap;
} wubuindex;

/* Add a term to the index vocabulary. Returns 0 on success. */
int wubuindex_add_term(wubuindex *ix, const char *term);

/* Feed a page of text: any vocabulary term found (case-insensitive) gets
 * `page` recorded. Returns 0. */
int wubuindex_feed_page(wubuindex *ix, const char *page_text, int page);

wubuindex *wubuindex_create(void);
void wubuindex_destroy(wubuindex *ix);

/* Number of entries + accessor (entries kept sorted by term). */
size_t wubuindex_count(const wubuindex *ix);
const wubuindex_entry *wubuindex_get(const wubuindex *ix, size_t i);

#endif
