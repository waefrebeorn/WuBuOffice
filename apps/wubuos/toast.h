/* toast.h -- UI-33: toast/notification queue for background ops.
 * Pure logic (no SDL): the shell pushes messages, ticks time, and draws
 * whatever toast_text() returns. Headless-testable. Opaque, C11. */
#ifndef WUOS_TOAST_H
#define WUOS_TOAST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Toasts Toasts;

Toasts *toast_create(void);
void    toast_destroy(Toasts *t);

/* Queue a message (copied). ttl_ticks = how many ticks it stays visible. */
void toast_push(Toasts *t, const char *msg, int ttl_ticks);

/* Advance one tick (call once per frame / timer). Expires the head. */
void toast_tick(Toasts *t);

/* Currently visible message, or NULL when the queue is empty. */
const char *toast_text(const Toasts *t);

/* Number of queued (incl. visible) messages. */
int toast_count(const Toasts *t);

#ifdef __cplusplus
}
#endif
#endif /* WUOS_TOAST_H */
