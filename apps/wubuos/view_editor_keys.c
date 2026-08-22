/* view_editor_keys.c -- keyboard handling for the editor view, split from
 * view_editor.c. Macro playback re-dispatches through the same on_key path
 * as live typing. */
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wuos.h"
#include "view_editor_internal.h"
#include "doc.h"
#include "lex.h"
#include "findbar.h"
#include "autocomp.h"
#include "macro.h"
#include "gotoline.h"
#include "settings.h"

void on_key(WuView *v, int key, int down);

/* siblings + local helpers */
void editor_caret_vert(Editor *e, int dl);
void save(WuView *v);
void find_open(WuView *v, int mode);
int  editor_line_of(Editor *e);
int  editor_col_of(Editor *e);
void fold_toggle_block(Editor *e);
void sym_toggle(Editor *e);
void bk_jump(Editor *e, int dir);

static void editor_macro_replay_op(int opcode, unsigned char ch, void *ctx){
    WuView *v = (WuView *)ctx;
    if (!v) return;
    if (opcode == MACRO_OP_CHAR) on_key(v, (int)ch, 1);
    else if (opcode == MACRO_OP_RETURN) on_key(v, WUOS_KEY_RETURN, 1);
    else if (opcode == MACRO_OP_BACKSPACE) on_key(v, WUOS_KEY_BACKSPACE, 1);
}

void on_key(WuView *v, int key, int down){
    Editor *e = v->priv;
    if (!down) return;
    e->frames = 0;  /* reset blink on activity */

    /* ---- undo / redo (Ctrl+Z / Ctrl+Y, also wired from the menu bar) ---- */
    if (key == WUOS_KEY_UNDO){ if (doc_can_undo(e->doc)) doc_undo(e->doc); return; }
    if (key == WUOS_KEY_REDO){ if (doc_can_redo(e->doc)) doc_redo(e->doc); return; }

    /* ---- auto-completion popup intercepts keys ---- */
    if (e->ac && autocomp_opened(e->ac)){
        switch (key){
            case WUOS_KEY_ESC: autocomp_close(e->ac); return;
            case WUOS_KEY_UP:   autocomp_move(e->ac, -1); return;
            case WUOS_KEY_DOWN: autocomp_move(e->ac, +1); return;
            case WUOS_KEY_TAB:
            case WUOS_KEY_RETURN: autocomp_accept(e->ac, e->doc); return;
            default: break;  /* typing closes the popup and falls through */
        }
        autocomp_close(e->ac);
    }

    /* ---- go-to-line mode intercepts keys ---- */
    if (e->gto && gotoline_active(e->gto)){
        /* normalize WuosKeys to the plain codes gotoline understands */
        int k = key;
        if (key == WUOS_KEY_RETURN)   k = 13;
        else if (key == WUOS_KEY_ESC) k = 27;
        else if (key == WUOS_KEY_BACKSPACE) k = 8;
        int r = gotoline_key(e->gto, k);
        if (r == 1){                       /* committed */
            int ln = gotoline_commit(e->gto);
            if (ln >= 1){
                size_t off = doc_offset_of_line(e->doc, ln);
                doc_set_cursor(e->doc, off);
                doc_set_selection(e->doc, off, off);
            }
        }
        if (r != 0) return;                /* 1 or 2: prompt consumed the key */
        return;
    }

    /* ---- find / replace mode intercepts keys ---- */
    if (e->find_mode && e->fb){
        switch (key){
            case WUOS_KEY_ESC:    find_close(v); return;
            case WUOS_KEY_FIND:   e->find_mode = 1; e->find_focus = 0; return;
            case WUOS_KEY_REPLACE: e->find_mode = 2; e->find_focus = 0; return;
            case WUOS_KEY_FINDNEXT:
                if (!findbar_query(e->fb)[0]) return;
                if (findbar_active(e->fb)){ size_t m=0,x=0; findbar_match(e->fb,&m,&x); findbar_next(e->fb, e->doc, x); }
                else findbar_next(e->fb, e->doc, 0);
                return;
            case WUOS_KEY_FINDPREV:
                if (!findbar_query(e->fb)[0]) return;
                findbar_prev(e->fb, e->doc);
                return;
            case WUOS_KEY_REPLACEALL:
                if (e->find_mode==2) findbar_replace_all(e->fb, e->doc);
                return;
            case WUOS_KEY_TAB:
                if (e->find_mode==2) e->find_focus ^= 1;  /* toggle field */
                return;
            case WUOS_KEY_RETURN:
                if (e->find_mode==2 && e->find_focus==1){
                    findbar_replace_one(e->fb, e->doc);              /* replace current */
                } else {
                    if (findbar_query(e->fb)[0]){
                        if (findbar_active(e->fb)){ size_t m=0,x=0; findbar_match(e->fb,&m,&x); findbar_next(e->fb, e->doc, x); }
                        else findbar_next(e->fb, e->doc, 0);
                    }
                }
                return;
            case WUOS_KEY_BACKSPACE: {
                char buf[512];
                const char *cur = (e->find_mode==2 && e->find_focus==1)
                    ? findbar_replace(e->fb) : findbar_query(e->fb);
                snprintf(buf, sizeof buf, "%s", cur);
                size_t l = strlen(buf);
                if (l) buf[l-1]=0;
                if (e->find_mode==2 && e->find_focus==1) findbar_set_replace(e->fb, buf);
                else findbar_set_query(e->fb, buf);
                return;
            }
            default:
                if (key>=32 && key<128){
                    char buf[512];
                    const char *cur = (e->find_mode==2 && e->find_focus==1)
                        ? findbar_replace(e->fb) : findbar_query(e->fb);
                    snprintf(buf, sizeof buf, "%s", cur);
                    size_t l = strlen(buf);
                    if (l < sizeof buf - 1){ buf[l]=(char)key; buf[l+1]=0; }
                    if (e->find_mode==2 && e->find_focus==1) findbar_set_replace(e->fb, buf);
                    else findbar_set_query(e->fb, buf);
                    if (!findbar_active(e->fb)) findbar_next(e->fb, e->doc, 0);
                    return;
                }
                return; /* swallow other keys (arrows etc.) while in find bar */
        }
    }

    /* ---- normal editing ---- */
    size_t cur = doc_cursor(e->doc);
    /* macro capture: while recording, log edit ops (skip meta keys) */
    if (e->macro && macro_recording(e->macro)){
        if (key >= 32 && key < 127) macro_record(e->macro, MACRO_OP_CHAR, (unsigned char)key);
        else if (key==WUOS_KEY_RETURN) macro_record(e->macro, MACRO_OP_RETURN, 0);
        else if (key==WUOS_KEY_BACKSPACE) macro_record(e->macro, MACRO_OP_BACKSPACE, 0);
    }
    switch (key){
        case WUOS_KEY_FIND:    find_open(v, 1); return;
        case WUOS_KEY_REPLACE: find_open(v, 2); return;
        case WUOS_KEY_GOTO:    if (e->gto) gotoline_open(e->gto); return;
        case WUOS_KEY_EOL:     convert_eol(e, e->eol_crlf? 0 : 1); return;
        case WUOS_KEY_THEME:  e->dark ^= 1; return;
        case WUOS_KEY_NEWDOC: {
            size_t i = docs_open(e->docs, NULL, "", "c");
            if (i != SIZE_MAX){ docs_set_active(e->docs, i); editor_sync_active(e); }
            return;
        }
        case WUOS_KEY_CLOSE: {
            if (docs_count(e->docs) > 1){
                size_t a = docs_active(e->docs);
                docs_close(e->docs, a);
                if (a >= docs_count(e->docs)) a = docs_count(e->docs)-1;
                docs_set_active(e->docs, a);
                editor_sync_active(e);
            }
            return;
        }
        case WUOS_KEY_DOCPREV:
        case WUOS_KEY_DOCNEXT: {
            size_t n = docs_count(e->docs);
            if (n > 1){
                size_t a = docs_active(e->docs);
                a = (key==WUOS_KEY_DOCNEXT)? (a+1)%n : (a+n-1)%n;
                docs_set_active(e->docs, a);
                editor_sync_active(e);
            }
            return;
        }
        case WUOS_KEY_TOGGLE_BK: bkmk_toggle(e->bk, editor_line_of(e)); return;
        case WUOS_KEY_NEXT_BK:   bk_jump(e, +1); return;
        case WUOS_KEY_PREV_BK:   bk_jump(e, -1); return;
        case WUOS_KEY_COLMODE:
            e->col_mode ^= 1;
            if (e->col_mode){
                e->sel_l0 = e->sel_l1 = editor_line_of(e);
                e->sel_c0 = e->sel_c1 = editor_col_of(e);
            }
            return;
        case WUOS_KEY_REC:
            if (e->macro) macro_toggle_rec(e->macro);
            return;
        case WUOS_KEY_PLAY: {
            if (!e->macro || !macro_count(e->macro)) return;
            int was = macro_recording(e->macro);
            /* don't re-record the playback */
            if (was) macro_toggle_rec(e->macro);
            macro_play(e->macro, editor_macro_replay_op, v);
            if (was) macro_toggle_rec(e->macro);   /* restore */
            return;
        }
        case WUOS_KEY_AC: if (e->ac) autocomp_open(e->ac, e->doc); return;
        case WUOS_KEY_SESSION: session_save(e); return;
        case WUOS_KEY_FOLD: fold_toggle_block(e); return;
        case WUOS_KEY_FUNCLIST: sym_toggle(e); return;
        case WUOS_KEY_BACKSPACE:
            if (cur>0) doc_delete(e->doc, cur-1, 1);
            break;
        case WUOS_KEY_RETURN:
            doc_type(e->doc, "\n", 1);
            break;
        case WUOS_KEY_TAB:
            doc_type(e->doc, "    ", 4);
            break;
        case WUOS_KEY_LEFT:
            if (e->col_mode){
                int c = editor_col_of(e);
                if (c>0){ e->sel_c1 = c-1; e->sel_l1 = editor_line_of(e); doc_set_cursor(e->doc, doc_offset_of_line(e->doc, e->sel_l1+1)+e->sel_c1); }
            } else if (cur>0) doc_set_cursor(e->doc, cur-1);
            break;
        case WUOS_KEY_RIGHT:
            if (e->col_mode){
                int c = editor_col_of(e);
                e->sel_c1 = c+1; e->sel_l1 = editor_line_of(e); doc_set_cursor(e->doc, doc_offset_of_line(e->doc, e->sel_l1+1)+e->sel_c1);
            } else doc_set_cursor(e->doc, cur+1);
            break;
        case WUOS_KEY_HOME: {
            char *t = doc_text(e->doc); size_t p=doc_cursor(e->doc);
            while (p>0 && t[p-1]!='\n') p--;
            doc_set_cursor(e->doc, p); free(t);
            break; }
        case WUOS_KEY_END: {
            char *t = doc_text(e->doc); size_t p=doc_cursor(e->doc), n=doc_length(e->doc);
            while (p<n && t[p]!='\n') p++;
            doc_set_cursor(e->doc, p); free(t);
            break; }
        case WUOS_KEY_UP: case WUOS_KEY_DOWN: {
            int dl = (key==WUOS_KEY_UP)? -1 : 1;
            if (e->col_mode){
                editor_caret_vert(e, dl);
                e->sel_l1 = editor_line_of(e);
            } else {
                char *t = doc_text(e->doc); size_t p=doc_cursor(e->doc), n=doc_length(e->doc);
                size_t line=0,col=0; for (size_t q=0;q<p;q++){ if(t[q]=='\n'){line++;col=0;}else col++; }
                size_t target = (key==WUOS_KEY_UP)? (line>0?line-1:0) : line+1;
                size_t lstart=0, curline=0;
                for (size_t q=0;q<n;q++){ if (curline==target){lstart=q;break;} if(t[q]=='\n')curline++; }
                size_t lend=lstart; while (lend<n && t[lend]!='\n') lend++;
                size_t newp = lstart + (col < (lend-lstart)? col : (lend-lstart));
                doc_set_cursor(e->doc, newp); free(t);
            }
            break; }
        case WUOS_KEY_PGUP: for(int i=0;i<20;i++) on_key(v,WUOS_KEY_UP,1); break;
        case WUOS_KEY_PGDN: for(int i=0;i<20;i++) on_key(v,WUOS_KEY_DOWN,1); break;
        case WUOS_KEY_SAVE:
                    save(v);
                    break;
                case WUOS_KEY_CUT:
                    if (doc_has_selection(e->doc)){
                        char *t = doc_text(e->doc);
                        size_t s = doc_sel_start(e->doc), en = doc_sel_end(e->doc);
                        char *clip = malloc(en - s + 1);
                        if (clip){ memcpy(clip, t + s, en - s); clip[en - s] = 0; }
                        free(t);
                        doc_delete(e->doc, s, en - s);
                        doc_set_cursor(e->doc, s);
                        doc_clear_selection(e->doc);
                        if (clip){
                            SDL_SetClipboardText(clip);
                            free(clip);
                        }
                    }
                    break;
                case WUOS_KEY_COPY:
                    if (doc_has_selection(e->doc)){
                        char *t = doc_text(e->doc);
                        size_t s = doc_sel_start(e->doc), en = doc_sel_end(e->doc);
                        char *clip = malloc(en - s + 1);
                        if (clip){ memcpy(clip, t + s, en - s); clip[en - s] = 0; SDL_SetClipboardText(clip); free(clip); }
                        free(t);
                    }
                    break;
                case WUOS_KEY_PASTE: {
                    const char *clip = SDL_GetClipboardText();
                    if (clip && clip[0]) doc_type(e->doc, clip, strlen(clip));
                    break;
                }
                case WUOS_KEY_PASTE_PLAIN: {
                    const char *clip = SDL_GetClipboardText();
                    if (clip && clip[0]) doc_type(e->doc, clip, strlen(clip));
                    break;
                }
                case WUOS_KEY_SELECT_ALL:
                    doc_set_selection(e->doc, 0, doc_length(e->doc));
                    break;
                default:
                    if (key>=32 && key<127){ char c=(char)key; doc_type(e->doc,&c,1); }
                    break;
            }
}

