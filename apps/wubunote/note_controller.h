/* note_controller.h -- pure wubunote interaction logic (tabs + editing + prompt).
 *
 * main.c owns TTY I/O only. ALL state transitions live here as PURE
 * functions with NO TTY and NO global state, so the real editor path is
 * unit-testable: feed events, assert the resulting buffer/scroll/quit.
 *
 * The "command palette" (Ctrl+K) is just the prompt pre-seeded with
 * a ':' and a list of commands -- discoverability without a ribbon.
 */
#ifndef WUBUNOTE_CONTROLLER_H
#define WUBUNOTE_CONTROLLER_H

#include <stddef.h>
#include <stdbool.h>

#include "input.h"
#include "edit.h"      /* EditBuf */

#ifdef __cplusplus
extern "C" {
#endif

#define NCTRL_MAX_TABS 8
#define NCTRL_PATH_MAX  256
#define NCTRL_PROMPT_MAX 256

/* what the prompt is currently collecting */
typedef enum {
    NPMT_NONE = 0,   /* normal editing */
    NPMT_FIND,        /* find-next query */
    NPMT_GOTO,        /* goto line number */
    NPMT_SAVEAS,      /* save to a path */
    NPMT_CMD          /* command palette (Ctrl+K) */
} NPrompt;

/* a single editor tab */
typedef struct {
    EditBuf *buf;
    char     path[NCTRL_PATH_MAX];
    int      dirty;     /* mirrors buf->dirty, cached for status bar */
} NTab;

typedef struct {
    size_t screen_w, screen_h;
    size_t body_y;     /* first row of the editing area (after header+tabbar) */
    size_t body_h;     /* rows available for text */
    size_t gutter;     /* line-number column width (0 = off) */

    NTab   tabs[NCTRL_MAX_TABS];
    size_t tab_n;
    size_t tab_active;

    int     wrap;       /* 1 = word-wrap long lines */
    int     running;

    /* prompt / palette modal state */
    NPrompt prompt;
    char    prompt_buf[NCTRL_PROMPT_MAX];
    size_t  prompt_len;
    const char *prompt_label;   /* "Find:" / "Goto:" / "Save:" / ":" */

    /* command list for the palette (static, see controller.c) */
    int     cmd_sel;    /* selected index while in NPMT_CMD */
} NState;

void nctrl_init(NState *st, size_t w, size_t h);
void nctrl_resize(NState *st, size_t w, size_t h);

/* open a file into a new tab (reads path). returns tab idx or -1. */
int nctrl_open(NState *st, const char *path);
/* new empty tab. returns idx or -1. */
int nctrl_new(NState *st);
/* close active tab; quits when last closes. */
void nctrl_close_active(NState *st);
/* switch tab (dir +1/-1, wraps). */
void nctrl_switch(NState *st, int dir);

/* begin a prompt mode */
void nctrl_prompt_find(NState *st);
void nctrl_prompt_goto(NState *st);
void nctrl_prompt_saveas(NState *st);
void nctrl_prompt_cmd(NState *st);

/* commit the current prompt (Enter). returns 1 if it did something, 0 if
 * it stayed in normal mode. `out_path` (if non-NULL and SAVEAS/CMD save)
 * receives a malloc'd path to write (caller frees). */
int nctrl_prompt_commit(NState *st, char **out_path);

/* command palette entries (Ctrl+K); shared by controller + UI */
extern const char *NCMD[];
extern const char *NCMD_HELP[];

/* the active tab's editing buffer (read-only access for callers/UI) */
EditBuf *nctrl_active_buf(const NState *st);

/* feed one decoded key/mouse event; mutates st + the active buffer.
 * If a SAVEAS/CMD-save commits, *out_path (if non-NULL) is set to a
 * malloc'd path the caller must persist + free. */
void nctrl_handle(NState *st, const TuiKey *k, char **out_path);

#ifdef __cplusplus
}
#endif

#endif /* WUBUNOTE_CONTROLLER_H */
