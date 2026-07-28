/* macro.h -- opaque Notepad++-style macro record/playback engine.
 *
 * Owns the recorded edit-op buffer (process-global semantics: one active
 * macro) and the recording flag. The editor delegates capture/playback here;
 * playback invokes a caller-supplied op callback so the module stays decoupled
 * from the editor's key dispatch.
 */
#ifndef WUBUOS_MACRO_H
#define WUBUOS_MACRO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Macro Macro;

/* opcodes recorded per edit action */
enum { MACRO_OP_CHAR = 1, MACRO_OP_RETURN = 2, MACRO_OP_BACKSPACE = 3 };

Macro *macro_create(void);
void   macro_destroy(Macro *m);

/* Toggle recording; when starting, the buffer is cleared. Returns new state. */
int  macro_toggle_rec(Macro *m);
int  macro_recording(const Macro *m);

/* Record one edit op (MACRO_OP_*). Ignored unless recording. `ch` is only
 * meaningful for MACRO_OP_CHAR. */
void macro_record(Macro *m, int opcode, unsigned char ch);

/* Number of recorded ops. */
int  macro_count(const Macro *m);

/* Playback: invoke `cb(opcode, ch, ctx)` for each recorded op, in order. */
typedef void (*macro_op_cb)(int opcode, unsigned char ch, void *ctx);
void macro_play(const Macro *m, macro_op_cb cb, void *ctx);

/* ---- persistence (SCR-98: macros survive the session) ----
 * A macro library file holds one macro per line:
 *     <name>\t<hex-byte>...
 * The shared engine can save its current buffer (naming it) and load a named
 * macro back. macro_save_all/macro_load_named manage the whole file; the
 * simple macro_save/macro_load helpers persist just the single shared buffer
 * under a fixed slot. */
void macro_set_name(Macro *m, const char *name);
const char *macro_name(const Macro *m);

/* Save the current shared buffer to `path` (overwrites the file with this one
 * named macro). Returns 0 on success. */
int macro_save(const char *path);
/* Load the first macro from `path` into the shared buffer. Returns 0 on
 * success, -1 if the file is missing/empty. */
int macro_load(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* WUBUOS_MACRO_H */
