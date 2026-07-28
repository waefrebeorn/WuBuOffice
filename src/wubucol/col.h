/* col.h -- opaque comment-thread store (COL-95): per-anchor threads with
 * replies and a resolved flag. An anchor is an opaque string (e.g. a node id or
 * a "line:col" range); the store never interprets it. */
#ifndef WUBUCOL_H
#define WUBUCOL_H

typedef struct Col Col;

typedef struct {
    char *author;
    char *text;
} Reply;

Col *col_create(void);
void  col_destroy(Col *c);

/* Add a top-level comment anchored at `anchor` (copied). Returns thread id. */
int   col_add(Col *c, const char *anchor, const char *author, const char *text);
/* Add a reply to thread `tid`. Returns 1 on success. */
int   col_reply(Col *c, int tid, const char *author, const char *text);
/* Mark/unmark a thread resolved. */
int   col_resolve(Col *c, int tid, int resolved);
/* Remove a thread. Returns 1 if removed. */
int   col_remove(Col *c, int tid);

int   col_thread_count(const Col *c);
int   col_id_at(const Col *c, int i);          /* list index -> thread id */
const char *col_anchor(const Col *c, int tid);
const char *col_author(const Col *c, int tid);
const char *col_text(const Col *c, int tid);   /* root comment text */
int   col_resolved(const Col *c, int tid);
int   col_reply_count(const Col *c, int tid);
const Reply *col_reply_at(const Col *c, int tid, int i);

#endif /* WUBUCOL_H */
