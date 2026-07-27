/* wuos.h -- unified WuBuOffice GUI shell (the "full suite + Notepad++ parity"
 * front-end). One SDL2 window hosts every engine behind a WuView adapter:
 *   Document (wurender), Spreadsheet (wubucell), Slide, OCR (wubuocr),
 *   Editor (WuBuPad core -- Notepad++ parity).
 *
 * The shell owns the window, tab bar, status bar and scroll; it dispatches
 * events to the active WuView. Each view renders into an RGBA buffer the
 * shell blits to the screen. Clean C11.
 */
#ifndef WUOS_H
#define WUOS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WuView WuView;

struct WuView {
    const char *name;                       /* tab label */
    void      (*destroy)(WuView *);
    /* Render the view into a malloc'd RGBA buffer (caller frees). Advances
     * the per-view scroll (px) the shell supplies. Returns 0 ok. */
    int       (*render)(WuView *, int w, int h, int scroll,
                        unsigned char **rgba, int *rw, int *rh);
    /* Keyboard: SDL_Keycode (>=32 printable) or control (see wuos_key_*). */
    void      (*on_key)(WuView *, int key, int down);
    /* Mouse wheel: dy in "notches" (+down). */
    void      (*on_wheel)(WuView *, int dy);
    /* Mouse left-click: x,y in view-local px (below the tab/status chrome).
     * Optional; used for clickable links/objects. May be NULL. */
    void      (*on_click)(WuView *, int x, int y);
    /* Optional status string builder (caller frees). May be NULL. */
    char     *(*status)(WuView *);
    /* Optional: save the current buffer to its loaded path (Ctrl+S). May be NULL. */
    void      (*save)(WuView *);
    /* Optional: return the loaded file path (for the title/status), or NULL. */
    const char *(*get_path)(WuView *);
    void      *priv;
};

/* Control key sentinels (kept out of the printable range). */
enum { WUOS_KEY_UP=2000, WUOS_KEY_DOWN, WUOS_KEY_LEFT, WUOS_KEY_RIGHT,
       WUOS_KEY_BACKSPACE, WUOS_KEY_RETURN, WUOS_KEY_HOME, WUOS_KEY_END,
       WUOS_KEY_TAB, WUOS_KEY_PGUP, WUOS_KEY_PGDN, WUOS_KEY_DEL, WUOS_KEY_ESC,
       WUOS_KEY_SAVE, WUOS_KEY_OPEN, WUOS_KEY_FIND, WUOS_KEY_REPLACE,
       WUOS_KEY_FINDNEXT, WUOS_KEY_FINDPREV, WUOS_KEY_REPLACEALL, WUOS_KEY_GOTO,
       WUOS_KEY_EOL, WUOS_KEY_THEME, WUOS_KEY_NEWDOC, WUOS_KEY_CLOSE,
       WUOS_KEY_DOCPREV, WUOS_KEY_DOCNEXT, WUOS_KEY_TOGGLE_BK, WUOS_KEY_NEXT_BK,
       WUOS_KEY_PREV_BK, WUOS_KEY_COLMODE, WUOS_KEY_REC, WUOS_KEY_PLAY, WUOS_KEY_AC,
 WUOS_KEY_SESSION, WUOS_KEY_FOLD, WUOS_KEY_FUNCLIST, WUOS_KEY_PLUGIN,
 WUOS_KEY_INSERT_CHART, WUOS_KEY_INSERT_DRAW, WUOS_KEY_INSERT_MATH,
 WUOS_KEY_EXPORT_EPUB, WUOS_KEY_A11Y_CHECK,
 WUOS_KEY_ZOOM_IN, WUOS_KEY_ZOOM_OUT, WUOS_KEY_ZOOM_RESET,
 WUOS_KEY_SETTINGS,
 WUOS_KEY_TOC1, WUOS_KEY_TOC2, WUOS_KEY_TOC3,
 WUOS_KEY_TOC4, WUOS_KEY_TOC5, WUOS_KEY_TOC6,
 WUOS_KEY_CHEAT,
 WUOS_KEY_INSERT_LINK,
 WUOS_KEY_INSERT_LIST,
 WUOS_KEY_INSERT_TABLE };

/* ---- view factories ----
 * `path` is an optional file to load (NULL = use the bundled sample). */
WuView *wuos_doc_create(const char *path);    /* wurender document */
WuView *wuos_editor_create(const char *path); /* WuBuPad core (Notepad++ parity) */
/* Inspection accessor for tests: report find state without exposing the
 * private Editor struct. Returns 0 ok, -1 on bad view. */
int wuos_editor_find_stats(WuView *v, int *active, int *total);
/* Test accessor: returns the editor's current document text (caller frees). */
char *wuos_editor_text(WuView *v);
/* Test accessor: current caret byte offset (for go-to-line assertions). */
size_t wuos_editor_cursor(WuView *v);
/* Test accessor: current dark-theme state (for theme-toggle assertion). */
int wuos_editor_dark(WuView *v);
/* Test accessor: multi-doc session size + active index (for doc-tab tests). */
size_t wuos_editor_doc_count(WuView *v);
size_t wuos_editor_doc_active(WuView *v);
/* Test accessor: number of active bookmarks (line-ops). */
int wuos_editor_bookmarks(WuView *v);
/* Test accessor: spell-check a word via the editor's attached dict (INT-8).
 * 1 known, 0 misspelled, -1 no spell engine. */
int wuos_editor_spell(WuView *v, const char *word);
/* Test accessor: column/block selection state (mode + block bounds). */
int wuos_editor_col(WuView *v, int *l0, int *c0, int *l1, int *c1);
/* Test accessor: macro record state (recording flag + recorded op count). */
int wuos_editor_macro(WuView *v, int *ops);
/* Test accessor: auto-completion popup state (open + candidate count + sel). */
int wuos_editor_ac(WuView *v, int *n, int *sel);
/* Test accessor: folded-line count + function-list panel state. */
int wuos_editor_fold(WuView *v, int *count);
int wuos_editor_sym(WuView *v, int *n);
/* Document view inspection: 1 if rendered page (md/model), else 0; text model. */
int  wuos_doc_is_rendered(WuView *v);
int  wuos_doc_has_text(WuView *v);
int  wuos_doc_find(WuView *v, const char *q);   /* returns 1 if match found */
/* Document view: count of inserted chart/draw/math overlay objects (INT-1,3). */
int  wuos_doc_obj_count(WuView *v);
/* Document view: last EPUB export message (view-owned, do not free), or NULL. */
const char *wuos_doc_epub_msg(WuView *v);
/* Document view: a11y issue count from last check, or -1 if not run (INT-5). */
int  wuos_doc_a11y_issues(WuView *v);
/* Document view: TOC entry count for the current render (DOC-54), or -1. */
int  wuos_doc_toc_count(WuView *v);
/* Document view: high-contrast setting (UXA-41). */
int  wuos_doc_high_contrast(WuView *v);
/* Cell view inspection: active cell + editing state + cell value as string. */
int  wuos_cell_active(WuView *v, int *col, int *row);
int  wuos_cell_editing(WuView *v);
void wuos_cell_value(WuView *v, char *out, int outn);   /* current active cell text */
int  wuos_cell_kind(WuView *v);                         /* -1 empty, 0 str,1 num,2 form */
void wuos_cell_formula(WuView *v, char *out, int outn); /* stored formula (if FORM) */
/* OCR view inspection: block count + selected block text (caller frees). */
int  wuos_ocr_blocks(WuView *v);
char *wuos_ocr_selected(WuView *v);
/* OCR: concatenated recognized text across all blocks (caller frees); NULL if none. */
char *wuos_ocr_text(WuView *v);
/* Plugin manager inspection (host side): count + name of loaded plugin i. */
int  wuos_plugin_count(void);
const char *wuos_plugin_name(int i);
int  wuos_plugin_load_path(const char *so);
char *wuos_plugin_run(int i, const char *args);
WuView *wuos_cell_create(const char *path);         /* wubucell grid */
WuView *wuos_ocr_create(const char *path);          /* wubuocr page */
WuView *wuos_slide_create(const char *path);        /* simple slide */
WuView *wuos_compare_create(const char *left, const char *right); /* diff view */
WuView *wuos_settings_create(void);                        /* preferences view (UI-25) */

#ifdef __cplusplus
}
#endif
#endif /* WUOS_H */
