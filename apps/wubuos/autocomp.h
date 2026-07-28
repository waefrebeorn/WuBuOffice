/* autocomp.h -- opaque auto-completion engine for the editor view.
 *
 * Owns the candidate word-list, prefix, selection state and builtin keyword
 * table. The editor feeds it the document text; it returns the current
 * candidate list + selected index for rendering and applies the chosen
 * completion back to the document.
 *
 * C11, opaque struct, no god-header: include only this file.
 */
#ifndef WUBUOS_AUTOCOMP_H
#define WUBUOS_AUTOCOMP_H

#include <stddef.h>

typedef struct AutoComp AutoComp;

/* Create an empty completion engine. Caller owns the result (free with
 * autocomp_destroy). */
AutoComp *autocomp_create(void);
void autocomp_destroy(AutoComp *ac);

/* Open the popup for the word under/before the caret in `doc` (WuBuPad Doc).
 * Collects identifiers from the document plus the builtin keyword table.
 * Returns 1 if candidates exist (popup open), 0 if empty (popup stays closed). */
int  autocomp_open(AutoComp *ac, const void *doc);

/* True if the popup is currently open. */
int  autocomp_opened(const AutoComp *ac);
/* Close the popup. */
void autocomp_close(AutoComp *ac);

/* Number of candidates and the selected index (0-based). */
int  autocomp_count(const AutoComp *ac);
int  autocomp_selected(const AutoComp *ac);
/* The i-th candidate string (caller must not free; valid until next call). */
const char *autocomp_candidate(const AutoComp *ac, int i);

/* Move selection up/down (clamped). */
void autocomp_move(AutoComp *ac, int dir);

/* Accept the selected candidate: delete the typed prefix in `doc` and insert
 * the full word. Closes the popup. Returns 1 on success. */
int  autocomp_accept(AutoComp *ac, void *doc);

#endif /* WUBUOS_AUTOCOMP_H */
