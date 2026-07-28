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

#ifdef __cplusplus
}
#endif
#endif /* WUBUOS_MACRO_H */
