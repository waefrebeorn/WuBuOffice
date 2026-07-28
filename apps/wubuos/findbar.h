/* findbar.h -- opaque find/replace engine for the editor view.
 *
 * Owns all find/replace state (query, replace string, match span, index,
 * regex handle) so the editor view stays free of that bookkeeping. The engine
 * queries the document via the WuBuPad Doc API but never stores a copy of it.
 *
 * C11, opaque struct, no god-header: clients include only this file.
 */
#ifndef WUBUOS_FINDBAR_H
#define WUBUOS_FINDBAR_H

#include <stddef.h>

typedef struct FindBar FindBar;

/* Create an empty find/replace engine. Caller owns the returned pointer
 * (free with findbar_destroy). */
FindBar *findbar_create(void);

/* Free the engine and any compiled regex. */
void findbar_destroy(FindBar *fb);

/* Configure case-insensitive / regex modes. */
void findbar_set_icase(FindBar *fb, int on);
void findbar_set_regex(FindBar *fb, int on);

/* Set the search / replace strings (copy taken). */
void findbar_set_query(FindBar *fb, const char *q);
void findbar_set_replace(FindBar *fb, const char *r);

/* Find the next match at/after `from` (bytes). Returns 1 on match, 0 if none.
 * Updates the active match span (query with findbar_match()) and the running
 * index/total counts. */
int findbar_next(FindBar *fb, const void *doc, size_t from);
/* Find the previous match (wraps to the first match at the start). */
int findbar_prev(FindBar *fb, const void *doc);

/* Replace the active match with the replace string, then advance to next. */
void findbar_replace_one(FindBar *fb, void *doc);
/* Replace every match in the document. */
void findbar_replace_all(FindBar *fb, void *doc);

/* Reset to an inactive state (no active selection). */
void findbar_clear_active(FindBar *fb);
/* True if a match is currently selected. */
int  findbar_active(const FindBar *fb);
/* Active match span in bytes [start,end). */
void findbar_match(const FindBar *fb, size_t *start, size_t *end);
/* 1-based index of the active match and total match count. */
void findbar_counts(const FindBar *fb, int *idx, int *total);
/* Per-render display accessors (the bar shows the live query/replace text). */
const char *findbar_query(const FindBar *fb);
const char *findbar_replace(const FindBar *fb);
int  findbar_icase(const FindBar *fb);
int  findbar_regex(const FindBar *fb);
/* Per-render transient message (e.g. "bad pattern"); empty string if none. */
const char *findbar_msg(const FindBar *fb);

#endif /* WUBUOS_FINDBAR_H */
