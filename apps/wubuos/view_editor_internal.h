/* view_editor_internal.h -- shared layout of the editor view's private
 * state. Internal to apps/wubuos editor modules; callers keep the opaque
 * WuView handle. */
#ifndef VIEW_EDITOR_INTERNAL_H
#define VIEW_EDITOR_INTERNAL_H

#include "wuos.h"
#include "docs.h"
#include "lex.h"
#include "findbar.h"
#include "gotoline.h"
#include "bkmk.h"
#include "macro.h"
#include "autocomp.h"
#include "codefold.h"
#include "autosave.h"
#include "spell.h"
#include <stddef.h>


typedef struct {
    Doc  *doc;
    Lex  *lex;
    char *path;       /* loaded file, or NULL */
    int   top;        /* first visible line */
    int   caret_line, caret_col;
    int   blink;      /* caret phase */
    int   frames;     /* for blink timing */

    /* ---- find / replace ----
     * Search engine state (query/replace/matches/regex) lives in the opaque
     * FindBar (findbar.h); the editor keeps only its own panel UI state:
     * which panel is open (find_mode), which field is focused (find_focus),
     * and the transient message timer (find_msg_t). */
    FindBar *fb;       /* opaque search engine (see findbar.h) */
    int   find_mode;  /* 0 none, 1 find, 2 replace */
    int   find_focus; /* in replace mode: 0=find field, 1=replace field */
    int   find_msg_t; /* frames remaining to show msg */

    /* ---- go to line (Ctrl+G): delegated to opaque GotoLine engine ---- */
    GotoLine *gto;

    /* ---- EOL + encoding (Notepad++ parity) ---- */
    int   eol_crlf;        /* 0 = LF, 1 = CRLF */
    const char *enc_label; /* detected encoding label, or NULL */
    int   dark;            /* 0 = light, 1 = dark theme */

    /* multi-document session (DONE engine: src/docs) */
    Docs *docs;

    /* bookmarks (Notepad++ line ops): delegated to opaque BkMk engine */
    BkMk *bk;

    /* column / block selection mode */
    int   col_mode;          /* 0 = stream, 1 = column */
    int   sel_l0, sel_c0;    /* anchor line/col */
    int   sel_l1, sel_c1;    /* active line/col */

    /* macro record/play (Notepad++-style): delegated to opaque Macro engine */
    Macro *macro;

    /* auto-completion popup: delegated to the opaque AutoComp engine */
    AutoComp *ac;           /* owns candidate list + selection (see autocomp.h) */

    /* code folding + function-list panel: delegated to opaque CodeFold engine */
    CodeFold *cf;

    /* crash-recovery autosave (INT-2 P0: was never attached to any app) */
    Autosave *asv;
    int   asv_tick;          /* frame counter for periodic snapshot */
    size_t asv_len;          /* last seen doc length (detect edits) */
    /* live spell-check (INT-8 P0: was never linked by any app) */
    SpellDict *spd;
} Editor;

extern unsigned char g_def_r, g_def_g, g_def_b;

/* Shared across the editor modules (defined in view_editor_textops.c etc). */
void editor_sync_active(Editor *e);
void session_save(Editor *e);
void find_open(WuView *v, int mode);
void editor_caret_vert(Editor *e, int dl);
void fold_toggle_block(Editor *e);
void sym_toggle(Editor *e);
void bk_jump(Editor *e, int dir);
int  editor_line_of(Editor *e);
int  editor_col_of(Editor *e);
size_t doc_offset_of_line(Doc *d, int lineN);
void convert_eol(Editor *e, int to_crlf);
void session_restore(Editor *e);
void find_close(WuView *v);

#endif /* VIEW_EDITOR_INTERNAL_H */
