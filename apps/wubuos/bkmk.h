/* bkmk.h -- opaque line-bookmark set for the editor view.
 *
 * Owns a sorted set of 0-based line numbers (Notepad++ line bookmarks) with
 * toggle and next/prev jump. The editor renders a gutter marker by querying
 * bkmk_has(); it never touches the underlying array.
 *
 * C11, opaque struct, no god-header: include only this file.
 */
#ifndef WUBUOS_BKMK_H
#define WUBUOS_BKMK_H

typedef struct BkMk BkMk;

/* Create an empty bookmark set (capacity 256 lines). */
BkMk *bkmk_create(void);
void  bkmk_destroy(BkMk *b);

/* Toggle the bookmark on `line` (0-based). Add if absent, remove if present. */
void bkmk_toggle(BkMk *b, int line);

/* Return the next bookmark line at/after `from` (dir=+1) or the previous
 * bookmark line at/before `from` (dir=-1). Returns -1 if none. */
int  bkmk_jump(BkMk *b, int from, int dir);

/* True if `line` carries a bookmark. */
int  bkmk_has(const BkMk *b, int line);

/* Number of bookmarks currently set. */
int  bkmk_count(const BkMk *b);

#endif /* WUBUOS_BKMK_H */
