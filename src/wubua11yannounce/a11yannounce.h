/* a11yannounce.h -- screen-reader announcement queue (UXA-53). Structural
 * changes (insert/delete/heading/table) are pushed as announcement strings the
 * platform SR bridge drains. Opaque. */
#ifndef WUBUA11YANNOUNCE_H
#define WUBUA11YANNOUNCE_H

typedef struct A11yAnnounce A11yAnnounce;

A11yAnnounce *a11y_announce_create(void);
void a11y_announce_destroy(A11yAnnounce *a);

/* Queue a structural-change announcement (copied). */
void a11y_announce_push(A11yAnnounce *a, const char *msg);
int  a11y_announce_pending(const A11yAnnounce *a);
/* Drain the oldest announcement (malloc'd, caller frees), or NULL if empty. */
char *a11y_announce_pop(A11yAnnounce *a);

#endif /* WUBUA11YANNOUNCE_H */
