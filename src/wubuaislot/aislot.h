/* aislot.h -- offline AI assist hook (SCR-99). An inference-slot interface:
 * a provider (local model, remote bridge, or the built-in rule-based fallback)
 * registers a callback; the app submits text requests and reads responses.
 * The slot itself never does I/O -- providers own that. Opaque. */
#ifndef WUBUAISLOT_H
#define WUBUAISLOT_H
#include <stddef.h>

typedef struct AiSlot AiSlot;

/* Provider callback: fill `out` (cap `outcap`, NUL-terminated) from `prompt`.
 * Return 0 on success, nonzero on failure. `user` is the provider's state. */
typedef int (*AiProviderFn)(void *user, const char *task, const char *prompt,
                            char *out, size_t outcap);

AiSlot *aislot_create(void);
void    aislot_destroy(AiSlot *s);

/* Register a provider (replaces any previous). NULL fn restores the built-in
 * rule-based fallback (summarize/complete heuristics, offline, no deps). */
void    aislot_set_provider(AiSlot *s, AiProviderFn fn, void *user);
int     aislot_has_custom_provider(const AiSlot *s);

/* Run a task ("summarize", "complete", ...) over `prompt`. Returns a malloc'd
 * response (caller frees) or NULL on provider failure. */
char   *aislot_run(AiSlot *s, const char *task, const char *prompt);

#endif /* WUBUAISLOT_H */
