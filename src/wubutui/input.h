/* input.h -- wubutui key decoding: raw input bytes -> logical key events.
 *
 * The decoder is a pure function over a byte buffer: it recognizes ASCII keys
 * and the common ANSI/VT escape sequences (arrows, page up/down, home/end,
 * insert/delete) and reports how many bytes it consumed. Because it never reads
 * from a fd, the entire key-handling path is unit-testable by feeding it byte
 * strings -- no TTY required (soul.md: pure, self-contained, testable core).
 *
 * Terminals deliver escape sequences as multiple bytes that may arrive split
 * across reads; the caller accumulates bytes and calls tui_key_decode() with
 * whatever it has. If the buffer holds an incomplete sequence, the decoder
 * reports TUI_KEY_INCOMPLETE and consumes nothing, so the caller reads more.
 */
#ifndef WUBUTUI_INPUT_H
#define WUBUTUI_INPUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TUI_KEY_NONE = 0,     /* empty buffer */
    TUI_KEY_INCOMPLETE,   /* partial escape sequence: read more, consume nothing */
    TUI_KEY_CHAR,         /* a printable/control char in .ch */
    TUI_KEY_ENTER,
    TUI_KEY_ESC,          /* a lone ESC (not a recognized sequence) */
    TUI_KEY_TAB,
    TUI_KEY_BACKSPACE,
    TUI_KEY_UP,
    TUI_KEY_DOWN,
    TUI_KEY_LEFT,
    TUI_KEY_RIGHT,
    TUI_KEY_PAGE_UP,
    TUI_KEY_PAGE_DOWN,
    TUI_KEY_HOME,
    TUI_KEY_END,
    TUI_KEY_INSERT,
    TUI_KEY_DELETE,
    TUI_KEY_MOUSE         /* a mouse event: see the .mouse fields */
} TuiKeyType;

/* Mouse actions (old-school xterm reporting, decoded here). */
typedef enum {
    TUI_MOUSE_PRESS = 0,   /* a button went down */
    TUI_MOUSE_RELEASE,     /* a button came up (SGR reports which; X10 does not) */
    TUI_MOUSE_DRAG,        /* motion with a button held */
    TUI_MOUSE_MOVE,        /* motion with no button (only if motion tracking on) */
    TUI_MOUSE_WHEEL_UP,    /* wheel scrolled up */
    TUI_MOUSE_WHEEL_DOWN   /* wheel scrolled down */
} TuiMouseAction;

/* Which button (for press/release/drag). Wheel events use WHEEL_* action. */
typedef enum {
    TUI_MBTN_NONE = 0,
    TUI_MBTN_LEFT,
    TUI_MBTN_MIDDLE,
    TUI_MBTN_RIGHT
} TuiMouseButton;

typedef struct {
    TuiKeyType type;
    char       ch;   /* valid when type == TUI_KEY_CHAR (the raw byte) */
    /* valid when type == TUI_KEY_MOUSE: */
    TuiMouseAction mouse_action;
    TuiMouseButton mouse_button;
    size_t         mouse_x;   /* 0-based column */
    size_t         mouse_y;   /* 0-based row    */
    int            mouse_shift, mouse_alt, mouse_ctrl;  /* modifier flags */
} TuiKey;

/* Decode the next key from `buf` (len bytes). Writes the event to *out and
 * returns the number of bytes consumed:
 *   - 0 with out->type == TUI_KEY_NONE       : len == 0
 *   - 0 with out->type == TUI_KEY_INCOMPLETE : a partial escape seq (read more)
 *   - >0                                     : bytes consumed for this event
 * Unknown escape sequences that are fully terminated are consumed and reported
 * as TUI_KEY_ESC so the stream never deadlocks. */
size_t tui_key_decode(const char *buf, size_t len, TuiKey *out);

/* Human-readable name for a key type (for tests / debugging). */
const char *tui_key_name(TuiKeyType t);

#ifdef __cplusplus
}
#endif

#endif /* WUBUTUI_INPUT_H */
