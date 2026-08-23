/* wuos_shell_events.c -- KEYDOWN dispatch for the wubuos shell: modal
 * dialog capture, command-palette capture, the Ctrl/Shift keymap, plugin
 * action and shell-level feature codes. Split from main.c. */
#include "wuos_shell_internal.h"
#include "dialog.h"
#include "settings.h"
#include "pasteplain.h"
#include <stdlib.h>
#include <string.h>

extern int g_dlg_action;
extern WuOSPluginMgr *g_plugins;
extern char *g_plugin_msg;
extern int g_plugin_idx;
extern void add_view(WuView *v);
extern void open_doc_path(const char *path);
extern void apply_zoom(void);

/* Returns 1 when the shell should quit (Escape with nothing capturing). */
int shell_keydown(SDL_Keycode k, SDL_Keymod mod, int *running){
    int code=0;
    /* UI-30: dismiss the first-run splash on any key */
    if (g_first_run){
        g_first_run = 0;
        WubuSettings *sh = wubusettings_shared();
        if (sh){ wubusettings_set_first_run(sh, 0); wubusettings_save(sh, NULL); }
        return 0;
    }
    /* ---- modal dialog owns input while open (DOC-66/EXP-89/UXA-47) ---- */
    if (dialog_active(g_dlg)){
        int nk = 0; const char *ch = NULL;
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) nk = 13;
        else if (k == SDLK_ESCAPE) nk = 27;
        else if (k == SDLK_BACKSPACE) nk = 8;
        else if (k >= 32 && k < 127){ nk = (int)k; ch = (const char*)&k; }
        if (nk){
            int res = dialog_key(g_dlg, nk, ch);
            if (res == 1){           /* confirmed */
                const char *txt = dialog_text(g_dlg);
                WuView *dv = views[active];
                if (g_dlg_action == 1){ if (wuos_doc_insert_link_url(dv, txt)) toast_push(g_toasts, "Hyperlink inserted", 90); }
                else if (g_dlg_action == 2){ if (wuos_doc_insert_qr(dv, txt)) toast_push(g_toasts, "QR inserted", 90); }
                else if (g_dlg_action == 3){ if (wuos_doc_insert_image_alt(dv, txt)) toast_push(g_toasts, "Image inserted", 90); }
                else if (g_dlg_action == 10){   /* Open file (Ctrl+O) */
                    if (txt && *txt){ open_doc_path(txt); toast_push(g_toasts, "Opened", 90); }
                    else toast_push(g_toasts, "Open: empty path", 120);
                }
                else if (g_dlg_action == 11){   /* Save As (Ctrl+Shift+S) */
                    if (dv && dv->set_path && txt && *txt){ dv->set_path(dv, txt); toast_push(g_toasts, "Saved as", 90); }
                    else if (dv && dv->save){ dv->save(dv); toast_push(g_toasts, "Saved", 90); }
                    else toast_push(g_toasts, "Save As: unsupported view", 120);
                }
                g_dlg_action = 0;
            } else if (res == 2){    /* cancelled */
                g_dlg_action = 0;
            }
        }
        return 0;   /* modal: swallow every key while open */
    }
    /* ---- UI-29: command palette captures input while open ---- */
    if (palette_is_open(g_palette)){
        if (k==SDLK_ESCAPE) palette_close(g_palette);
        else if (k==SDLK_BACKSPACE) palette_backspace(g_palette);
        else if (k==SDLK_DOWN) palette_next(g_palette);
        else if (k==SDLK_UP) palette_prev(g_palette);
        else if (k==SDLK_RETURN||k==SDLK_KP_ENTER){
            int cmd = palette_confirm(g_palette);
            switch (cmd){
            case 2: { WuView *nv = wuos_doc_create(NULL);
                      if (nv && nviews<8){ add_view(nv); active=nviews-1; g_scroll=0; }
                      toast_push(g_toasts, "New document", 90); } break;
            case 3: { WubuSettings *sh=wubusettings_shared();
                      if (sh) wubusettings_set_dark(sh, !wubusettings_dark(sh));
                      toast_push(g_toasts, "Theme toggled", 90); } break;
            case 4: g_zoom += 0.1f; apply_zoom();
                    toast_push(g_toasts, "Zoom in", 60); break;
            case 5: g_zoom -= 0.1f; apply_zoom();
                    toast_push(g_toasts, "Zoom out", 60); break;
            case 6: g_zoom = 1.0f; apply_zoom(); toast_push(g_toasts, "Zoom reset", 60); break;
            case 7: for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; g_scroll=0; break; }
                    break;
            case 8: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_EPUB, 1);
                    toast_push(g_toasts, "EPUB export requested", 120); break;
            case 40: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_PDF, 1);
                     toast_push(g_toasts, "PDF export requested", 120); break;
            case 41: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_HTML, 1);
                     toast_push(g_toasts, "HTML export requested", 120); break;
            case 42: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_MARKDOWN, 1);
                     toast_push(g_toasts, "Markdown export requested", 120); break;
            case 43: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_LATEX, 1);
                     toast_push(g_toasts, "LaTeX export requested", 120); break;
            case 44: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_EXPORT_RTF, 1);
                     toast_push(g_toasts, "RTF export requested", 120); break;
            case 9: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_A11Y_CHECK, 1);
                    toast_push(g_toasts, "Accessibility check run", 120); break;
            case 10: { WubuSettings *sh=wubusettings_shared();
                      if (sh) wubusettings_set_high_contrast(sh, !wubusettings_high_contrast(sh));
                      toast_push(g_toasts, "High contrast toggled", 90); } break;
            case 20: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_H1, 1);
                     toast_push(g_toasts, "Style: Heading 1", 90); break;
            case 21: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_H2, 1);
                     toast_push(g_toasts, "Style: Heading 2", 90); break;
            case 22: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_H3, 1);
                     toast_push(g_toasts, "Style: Heading 3", 90); break;
            case 23: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_BODY, 1);
                     toast_push(g_toasts, "Style: Body", 90); break;
            case 24: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_QUOTE, 1);
                     toast_push(g_toasts, "Style: Quote", 90); break;
            case 25: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_STYLE_CODE, 1);
                     toast_push(g_toasts, "Style: Code", 90); break;
            case 26: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_INSERT_SCRIPT, 1);
                     toast_push(g_toasts, "Insert: Script Field", 90); break;
            case 30: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_REC, 1);
                     toast_push(g_toasts, "Macro: record toggled", 90); break;
            case 31: if (views[active]->on_key) views[active]->on_key(views[active], WUOS_KEY_PLAY, 1);
                     toast_push(g_toasts, "Macro: play", 90); break;
            case 32: { const char *mp = getenv("WUBUOS_MACRO_DIR");
                       char buf[512];
                       snprintf(buf,sizeof buf,"%s/wubuos_macro.mac", mp? mp : "/tmp");
                       macro_set_name(macro_create(), "default");
                       if (macro_save(buf)==0) toast_push(g_toasts, "Macro saved", 90);
                       else toast_push(g_toasts, "Macro save failed: check WUBUOS_MACRO_DIR is writable", 180); } break;
            case 33: { const char *mp = getenv("WUBUOS_MACRO_DIR");
                       char buf[512];
                       snprintf(buf,sizeof buf,"%s/wubuos_macro.mac", mp? mp : "/tmp");
                       if (macro_load(buf)==0) toast_push(g_toasts, "Macro loaded", 90);
                       else toast_push(g_toasts, "Macro load failed: no macro at that path yet", 180); } break;
            /* DOC-66 / EXP-89 / UXA-47: open the modal dialog. */
            case 50: g_dlg_action = 1; dialog_open(g_dlg, "Insert Hyperlink", "URL:", "https://"); toast_push(g_toasts, "Hyperlink: type URL, Enter", 120); break;
            case 51: g_dlg_action = 2; dialog_open(g_dlg, "Insert QR Code", "Text:", ""); toast_push(g_toasts, "QR: type text, Enter", 120); break;
            case 52: g_dlg_action = 3; dialog_open(g_dlg, "Insert Image", "Alt text:", ""); toast_push(g_toasts, "Image: type alt text, Enter", 120); break;
            /* UI-39: open a recent document. */
            default:
                if (cmd >= 200){
                    int ri = cmd - 200;
                    WubuSettings *sh = wubusettings_shared();
                    if (sh && ri >= 0 && ri < wubusettings_recents_count(sh))
                        open_doc_path(wubusettings_recent(sh, ri));
                }
                else {
                /* INT-15: font-family commands (id == 100 + index) */
                if (cmd >= 100){
                    int fi = cmd - 100;
                    if (fi >= 0 && fi < wuos_font_family_count()){
                        if (wuos_font_set_family(fi)==0){
                            WubuSettings *sh = wubusettings_shared();
                            if (sh){ wubusettings_set_font_family(sh, wuos_font_family_name(fi));
                                     wubusettings_save(sh, NULL); }
                            toast_push(g_toasts, wuos_font_family_name(fi), 90);
                        } else toast_push(g_toasts, "Font switch failed: glyphs unavailable for that family", 180);
                    }
                }
                }
                break;
            }
        }
        else if (k>=32 && k<128 && !(mod & KMOD_CTRL)) palette_input(g_palette, (char)k);
        return 0;  /* palette swallows the event */
    }
    if (k==SDLK_k && (mod & KMOD_CTRL) && !(mod & KMOD_SHIFT)){
        palette_open(g_palette);   /* UI-29: Ctrl+K */
        return 0;
    }
    if (k==SDLK_ESCAPE){ *running=0; }
    else if (k==SDLK_s && (mod & KMOD_CTRL)) code=WUOS_KEY_SAVE;
    else if (k==SDLK_f && (mod & KMOD_CTRL)) code=WUOS_KEY_FIND;
    else if (k==SDLK_h && (mod & KMOD_CTRL)) code=WUOS_KEY_REPLACE;
    else if (k==SDLK_r && (mod & KMOD_CTRL)) code=WUOS_KEY_REPLACEALL;
    else if (k==SDLK_g && (mod & KMOD_CTRL)) code=WUOS_KEY_GOTO;
    else if (k==SDLK_e && (mod & KMOD_CTRL)) code=WUOS_KEY_EOL;
    else if (k==SDLK_BACKQUOTE && (mod & KMOD_CTRL)) code=WUOS_KEY_THEME;
    else if (k==SDLK_z && (mod & KMOD_CTRL) && !(mod & KMOD_SHIFT)) code=WUOS_KEY_UNDO;
    else if ((k==SDLK_y && (mod & KMOD_CTRL)) || (k==SDLK_z && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT))) code=WUOS_KEY_REDO;
    else if (k==SDLK_t && (mod & KMOD_CTRL)) code=WUOS_KEY_NEWDOC;
    else if (k==SDLK_w && (mod & KMOD_CTRL)) code=WUOS_KEY_CLOSE;
    else if (k==SDLK_TAB && (mod & KMOD_CTRL))
        code = (mod & KMOD_SHIFT)? WUOS_KEY_DOCPREV : WUOS_KEY_DOCNEXT;
    else if (k==SDLK_F2 && (mod & KMOD_CTRL)) code=WUOS_KEY_TOGGLE_BK;
    else if (k==SDLK_F2) code=(mod & KMOD_SHIFT)? WUOS_KEY_PREV_BK : WUOS_KEY_NEXT_BK;
    else if (k==SDLK_c && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_COLMODE;
    else if (k==SDLK_r && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_REC;
    else if (k==SDLK_p && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PLAY;
    else if (k==SDLK_SPACE && (mod & KMOD_CTRL)) code=WUOS_KEY_AC;
    else if (k==SDLK_s && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_SESSION;
    else if (k==SDLK_b && (mod & KMOD_CTRL)) code=WUOS_KEY_SIDEBAR;   /* Ctrl+B: toggle Navigator sidebar */
    else if (k==SDLK_o && (mod & KMOD_CTRL)){   /* Ctrl+O: Open file dialog */
        g_dlg_action = 10;
        const char *cur = (views[active] && views[active]->get_path) ? views[active]->get_path(views[active]) : NULL;
        dialog_open(g_dlg, "Open File", "Path:", cur ? cur : "");
        toast_push(g_toasts, "Open: type path, Enter", 120);
        return 0;
    }
    else if (k==SDLK_a && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)){   /* Ctrl+Shift+A: Save As dialog */
        g_dlg_action = 11;
        const char *cur = (views[active] && views[active]->get_path) ? views[active]->get_path(views[active]) : NULL;
        dialog_open(g_dlg, "Save As", "Path:", cur ? cur : "");
        toast_push(g_toasts, "Save As: type path, Enter", 120);
        return 0;
    }
    else if (k==SDLK_f && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_FOLD;
    else if (k==SDLK_l && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_FUNCLIST;
    else if (k==SDLK_k && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PLUGIN;
    else if (k==SDLK_F10) code=WUOS_KEY_SETTINGS;
    else if ((k==SDLK_EQUALS || k==SDLK_PLUS) && (mod & KMOD_CTRL)) code=WUOS_KEY_ZOOM_IN;
    else if (k==SDLK_MINUS && (mod & KMOD_CTRL)) code=WUOS_KEY_ZOOM_OUT;
    else if (k==SDLK_0 && (mod & KMOD_CTRL)) code=WUOS_KEY_ZOOM_RESET;
    else if (k==SDLK_l && (mod & KMOD_CTRL) && !(mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_LINK; /* DOC-60 */
    else if (k==SDLK_l && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_LIST; /* DOC-59 */
    else if (k==SDLK_t && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_TABLE; /* DOC-62 */
    else if (k==SDLK_i && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_IMAGE; /* DOC-61 */
    else if (k==SDLK_b && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_PAGEBREAK; /* DOC-57 */
    else if (k==SDLK_s && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_SECTIONBREAK; /* DOC-57 */
    else if (k==SDLK_h && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_HEADER; /* DOC-56 */
    else if (k==SDLK_f && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_FOOTER; /* DOC-56 */
    else if (k==SDLK_c && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_COMMENT; /* DOC-63 */
    else if (k==SDLK_t && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT) && (mod & KMOD_ALT)) code=WUOS_KEY_INSERT_TRACKCHANGE; /* DOC-64 */
    else if (k==SDLK_d && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_FIELD; /* DOC-65 */
    else if (k==SDLK_g && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_INSERT_SCRIPT; /* DOC-97 */
    else if (k==SDLK_1 && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_STYLE_H1; /* DOC-58 */
    else if (k==SDLK_2 && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_STYLE_H2; /* DOC-58 */
    else if (k==SDLK_3 && (mod & KMOD_CTRL) && (mod & KMOD_ALT)) code=WUOS_KEY_STYLE_H3; /* DOC-58 */
    else if (k==SDLK_b && (mod & KMOD_CTRL)) code=WUOS_KEY_BOLD;     /* N3 MS convention */
    /* hop 21: spreadsheet keyboard conformance */
    else if (k==SDLK_F2) code=WUOS_KEY_EDIT_CELL;
    else if (k==SDLK_LEFT  && (mod & KMOD_CTRL)) code=WUOS_KEY_EDGE_LEFT;
    else if (k==SDLK_RIGHT && (mod & KMOD_CTRL)) code=WUOS_KEY_EDGE_RIGHT;
    else if (k==SDLK_UP    && (mod & KMOD_CTRL)) code=WUOS_KEY_EDGE_UP;
    else if (k==SDLK_DOWN  && (mod & KMOD_CTRL)) code=WUOS_KEY_EDGE_DOWN;
    else if (k==SDLK_i && (mod & KMOD_CTRL)) code=WUOS_KEY_ITALIC;   /* N3 MS convention */
    else if (k==SDLK_UP && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PARA_PREV; /* DOC-58 */
    else if (k==SDLK_DOWN && (mod & KMOD_CTRL) && (mod & KMOD_SHIFT)) code=WUOS_KEY_PARA_NEXT; /* DOC-58 */
    else if (k==SDLK_F1) code=WUOS_KEY_CHEAT;   /* UI-36 */
    else if (k>=SDLK_1 && k<=SDLK_6 && (mod & KMOD_CTRL))
        code = WUOS_KEY_TOC1 + (k - SDLK_1);   /* DOC-54 jump */
    else if (k==SDLK_F3) code=(mod & KMOD_SHIFT)? WUOS_KEY_FINDPREV : WUOS_KEY_FINDNEXT;
    else if (k==SDLK_UP) code=WUOS_KEY_UP;
    else if (k==SDLK_DOWN) code=WUOS_KEY_DOWN;
    else if (k==SDLK_LEFT) code=WUOS_KEY_LEFT;
    else if (k==SDLK_RIGHT) code=WUOS_KEY_RIGHT;
    else if (k==SDLK_BACKSPACE) code=WUOS_KEY_BACKSPACE;
    else if (k==SDLK_RETURN||k==SDLK_KP_ENTER) code=WUOS_KEY_RETURN;
    else if (k==SDLK_TAB) code=WUOS_KEY_TAB;
    else if (k==SDLK_HOME) code=WUOS_KEY_HOME;
    else if (k==SDLK_END) code=WUOS_KEY_END;
    else if (k==SDLK_PAGEUP) code=WUOS_KEY_PGUP;
    else if (k==SDLK_PAGEDOWN) code=WUOS_KEY_PGDN;
    else if (k==SDLK_DELETE) code=WUOS_KEY_DEL;
    else if (k==SDLK_x && (mod&KMOD_CTRL)) code=WUOS_KEY_CUT;
    else if (k==SDLK_c && (mod&KMOD_CTRL)) code=WUOS_KEY_COPY;
    else if (k==SDLK_v && (mod&KMOD_CTRL) && !(mod&KMOD_SHIFT)) code=WUOS_KEY_PASTE;
    else if (k==SDLK_v && (mod&KMOD_CTRL) && (mod&KMOD_SHIFT)) code=WUOS_KEY_PASTE_PLAIN;
    else if (k==SDLK_a && (mod&KMOD_CTRL)) code=WUOS_KEY_SELECT_ALL;
    else if (k==SDLK_w && (mod&KMOD_CTRL)) code=WUOS_KEY_CLOSE;
    else if (k==SDLK_F5) code=WUOS_KEY_PRESENTATION;
    else if (k>=32 && k<128) code=(int)k;
    if (code && views[active]->on_key) views[active]->on_key(views[active], code, 1);

    /* plugin action: Ctrl+Shift+K runs the next loaded plugin */
    if (code == WUOS_KEY_PLUGIN){
        if (!g_plugins || wuos_plugins_count(g_plugins)==0){
            free(g_plugin_msg);
            g_plugin_msg = strdup("no plugins loaded");
        } else {
            char *r = wuos_plugins_exec(g_plugins, g_plugin_idx, NULL);
            free(g_plugin_msg);
            g_plugin_msg = r;
            g_plugin_idx = (g_plugin_idx + 1) % wuos_plugins_count(g_plugins);
        }
    }

    /* EXP-88 paste-plain: Ctrl+Shift+V strips formatting */
    else if (code == WUOS_KEY_PASTE_PLAIN){
        char *clip = SDL_GetClipboardText();
        if (clip){
            char *plain = pasteplain_strip(clip);
            if (plain && views[active] && views[active]->on_key){
                for (char *p = plain; *p; p++){
                    views[active]->on_key(views[active], (int)*p, 1);
                }
            }
            free(plain);
            SDL_free(clip);
        }
    }

    /* ---- shell-level features (not forwarded to the view) ---- */
    if (code == WUOS_KEY_ZOOM_IN){
        g_zoom += 0.1f; apply_zoom();
    } else if (code == WUOS_KEY_ZOOM_OUT){
        g_zoom -= 0.1f; apply_zoom();
    } else if (code == WUOS_KEY_ZOOM_RESET){
        g_zoom = 1.0f; apply_zoom();
    } else if (code == WUOS_KEY_SETTINGS){
        for (int i=0;i<nviews;i++) if (!strcmp(views[i]->name,"Settings")){ active=i; g_scroll=0; break; }
    } else if (code == WUOS_KEY_SIDEBAR){
        g_sidebar = !g_sidebar;   /* toggle Navigator sidebar */
    } else if (code == WUOS_KEY_CHEAT){
        g_cheat = !g_cheat;   /* UI-36 toggle */
    }
    return 0;
}
