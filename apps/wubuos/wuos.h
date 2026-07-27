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
    void      *priv;
};

/* Control key sentinels (kept out of the printable range). */
enum { WUOS_KEY_UP=2000, WUOS_KEY_DOWN, WUOS_KEY_LEFT, WUOS_KEY_RIGHT,
       WUOS_KEY_BACKSPACE, WUOS_KEY_RETURN, WUOS_KEY_HOME, WUOS_KEY_END,
       WUOS_KEY_TAB, WUOS_KEY_PGUP, WUOS_KEY_PGDN, WUOS_KEY_DEL, WUOS_KEY_ESC };

/* ---- view factories ---- */
WuView *wuos_doc_create(void);       /* wurender document */
WuView *wuos_editor_create(void);    /* WuBuPad core (Notepad++ parity) */
WuView *wuos_cell_create(void);      /* wubucell grid */
WuView *wuos_ocr_create(void);       /* wubuocr page */
WuView *wuos_slide_create(void);     /* simple slide */

#ifdef __cplusplus
}
#endif
#endif /* WUOS_H */
