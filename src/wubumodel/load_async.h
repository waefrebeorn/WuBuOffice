#ifndef WUBUMODEL_LOAD_ASYNC_H
#define WUBUMODEL_LOAD_ASYNC_H

/* Background document loading (ws07#1334: never block the UI thread).
 * The heavy load function runs on a worker thread; completion callbacks
 * are fired on the JOINING thread (the caller / UI thread), so they may
 * safely touch UI state. Native pthreads only — no Electron, no JVM. */

#include "model.h"
#include <stddef.h>

typedef struct load_async load_async;

/* A load function: read+parse `path` into *out. Returns 0 on success
 * (ownership of *out passes to the caller) or -1 on failure (*out NULL). */
typedef int (*wubumodel_load_fn)(const char *path, wubumodel_doc **out);

/* Completion callback: fired per queued path on the joining thread after
 * load_async_join(). `doc` is NULL on load failure. */
typedef void (*wubumodel_load_done)(const char *path,
                                   wubumodel_doc *doc, void *user);

/* Create a loader (owns a worker thread + queue mutex). */
load_async *load_async_create(void);

/* Destroy a loader. Implicitly joins the worker (waits for queued
 * loads to finish); any un-delivered callbacks are fired first. */
void load_async_destroy(load_async *la);

/* Queue `path` for background load via `fn`; `done` fires on the
 * joining thread with the result. Returns 0 if queued, -1 on OOM. */
int load_async_queue(load_async *la, const char *path,
                     wubumodel_load_fn fn, wubumodel_load_done done,
                     void *user);

/* Block until every queued load has finished, then fire each `done`
 * callback (on THIS thread) with its result. Safe to call from the
 * UI thread. After this returns, the queue is empty. */
void load_async_join(load_async *la);

#endif /* WUBUMODEL_LOAD_ASYNC_H */
