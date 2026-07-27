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
       WUOS_KEY_PREV_BK, WUOS_KEY_COLMODE, WUOS_KEY_REC, WUOS_KEY_PLAY };

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
/* Test accessor: column/block selection state (mode + block bounds). */
int wuos_editor_col(WuView *v, int *l0, int *c0, int *l1, int *c1);
/* Test accessor: macro record state (recording flag + recorded op count). */
int wuos_editor_macro(WuView *v, int *ops);
WuView *wuos_cell_create(const char *path);         /* wubucell grid */
WuView *wuos_ocr_create(const char *path);          /* wubuocr page */
WuView *wuos_slide_create(const char *path);        /* simple slide */
WuView *wuos_compare_create(const char *left, const char *right); /* diff view */

#ifdef __cplusplus
}
#endif
#endif /* WUOS_H */
