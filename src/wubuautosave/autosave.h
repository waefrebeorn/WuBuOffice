/* autosave.h -- dependency-free C11 autosave + crash-recovery (wubuautosave).
 *
 * Periodically snapshots a wubumodel_doc to an atomic ".asd" sidecar and
 * tracks a PID ".lock" so a crash leaves a recoverable snapshot behind. On the
 * next open, callers detect+recover the snapshot rather than losing work.
 *
 * Crash model: a live session owns <doc>.lock containing its PID. If the
 * process dies, the lock becomes stale (PID no longer running) while the
 * <doc>.asd snapshot remains -- that is the "recover me" signal.
 *
 * All writes are atomic: temp file -> fsync -> rename over the target, so a
 * crash mid-write never corrupts the existing snapshot. */
#ifndef WUBUOFFICE_AUTOSAVE_H
#define WUBUOFFICE_AUTOSAVE_H

#include <stddef.h>
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Autosave Autosave;

/* Create an autosave session for the document at `doc_path` (the real file the
 * user saves to, e.g. "report.docx"). The sidecar paths are derived from it
 * (<doc>.asd and <doc>.lock). `interval_ms` is the minimum gap between
 * background snapshots once the doc is dirty; 0 = only explicit flushes. */
Autosave *wubuautosave_create(const char *doc_path, int interval_ms);
void      wubuautosave_destroy(Autosave *a);

/* Set/clear the dirty flag. Only dirty docs get snapshotted on tick(). */
void wubuautosave_mark_dirty(Autosave *a);
void wubuautosave_clear_dirty(Autosave *a);

/* Change the interval (ms). */
void wubuautosave_set_interval(Autosave *a, int interval_ms);

/* Call periodically (e.g. on a timer / idle). If the doc is dirty and the
 * interval has elapsed since the last snapshot, serializes atomically to
 * <doc>.asd. Returns 1 if it wrote a snapshot, 0 if it skipped, -1 on error. */
int wubuautosave_tick(Autosave *a, const wubumodel_doc *doc);

/* Force a snapshot now (e.g. on quit / before a risky op). Returns 0 ok. */
int wubuautosave_flush(Autosave *a, const wubumodel_doc *doc);

/* Normal close: remove the snapshot + lock (work was saved to the real doc).
 * Returns 0 ok. */
int wubuautosave_clear(Autosave *a);

/* ---- recovery (free functions, no live session needed) ---- */

/* Non-zero if a recoverable snapshot exists for `doc_path`: a ".asd" file is
 * present AND there is no live owner (no lock, or a stale lock from a dead
 * PID). This is the "offer recovery" signal. */
int wubuautosave_has_recovery(const char *doc_path);

/* Recover the snapshot into a fresh wubumodel_doc. On success returns 1 and
 * sets *out (caller frees with wubumodel_doc_destroy). Returns 0 if no
 * recovery was available, -1 on error (and *out is NULL). The snapshot file is
 * left in place so the user can discard it explicitly; call
 * wubuautosave_discard_recovery() when they decline/accept. */
int wubuautosave_recover(const char *doc_path, wubumodel_doc **out);

/* Delete the snapshot sidecar (and any lock). Call after recovery is accepted
 * or declined. Returns 0 ok. */
int wubuautosave_discard_recovery(const char *doc_path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOFFICE_AUTOSAVE_H */
