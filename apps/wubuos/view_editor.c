/* view_editor.c -- Notepad++-parity editor view (WuBuPad core + lexer).
 *
 * Hosts the REAL WuBuPad document model (piece-table buffer, undo/redo,
 * column selection) and a real lexer for syntax highlighting. Renders the
 * text with a blinking caret, line numbers, and per-token coloring. Typing,
 * arrows, backspace/return, Home/End, PageUp/PageDn all edit the live Doc.
 *
 * This is genuine Notepad++-class editing, not a mockup -- it is the same
 * engine WuBuPad ships, embedded into the unified office shell.
 */
#include <SDL2/SDL.h>

#include "wuos.h"
#include "wuos_font.h"
#include "wuos_file.h"
#include "findbar.h" /* opaque find/replace engine (extracted from this file) */
#include "autocomp.h" /* opaque auto-completion engine (extracted from this file) */
#include "bkmk.h"     /* opaque line-bookmark set (extracted from this file) */
#include "codefold.h"  /* opaque code-folding + function-list (extracted) */
#include "macro.h"     /* opaque macro record/playback (extracted) */
#include "gotoline.h"  /* opaque go-to-line prompt (extracted) */

#include "doc.h"    /* cross-repo: ~/WuBuPad/src */
#include "lex.h"
#include "encode.h" /* WuBuPad encoding detect */
#include "docs.h"   /* WuBuPad multi-document session (DONE engine) */
#include "autosave.h" /* wubuautosave: crash-recovery (INT-2 P0) */
#include "spell.h"    /* wubuspell: live red-squiggle (INT-8 P0) */
#include "settings.h"
#include "view_editor_internal.h"

/* defined in the sibling editor modules */
int  render(WuView *v, int w, int h, int scroll, unsigned char **rgba, int *rw, int *rh);
void on_key(WuView *v, int key, int down);
void on_wheel(WuView *v, int dy); /* wubusettings: live word-wrap + tab-width (UI-26) */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>

/* forward decls: wubuautosave bridge helpers (defined below wuos_editor_create) */
wubumodel_doc *editor_doc_to_model(const void *d);
static char *editor_model_text(const wubumodel_doc *m);

/* macro_play callback: re-dispatch a recorded op through on_key so playback
 * exercises the exact same editing path as a live key press. */
void on_key(WuView *v, int key, int down);  /* defined in view_editor_keys.c */
static void editor_macro_replay_op(int opcode, unsigned char ch, void *ctx){
    WuView *v = ctx;
    if (!v) return;
    if (opcode == MACRO_OP_CHAR) on_key(v, (int)ch, 1);
    else if (opcode == MACRO_OP_RETURN) on_key(v, WUOS_KEY_RETURN, 1);
    else if (opcode == MACRO_OP_BACKSPACE) on_key(v, WUOS_KEY_BACKSPACE, 1);
}
static void tok_color(LexTok k, unsigned char *r, unsigned char *g, unsigned char *b){
    switch (k){
        case TK_KEYWORD:  *r=86;  *g=156; *b=214; break;   /* blue */
        case TK_TYPE:     *r=78;  *g=201; *b=176; break;   /* teal */
        case TK_STRING:   *r=152; *g=195; *b=121; break;   /* green */
        case TK_CHAR:     *r=209; *g=154; *b=102; break;   /* orange */
        case TK_NUMBER:   *r=181; *g=206; *b=168; break;   /* light green */
        case TK_COMMENT:  *r=128; *g=128; *b=128; break;   /* grey */
        case TK_PREPROC:  *r=215; *g=186; *b=125; break;   /* tan */
        case TK_OPERATOR:
        case TK_PUNCT:    *r=120; *g=120; *b=130; break;   /* slate */
        default:          *r=g_def_r; *g=g_def_g; *b=g_def_b; break;   /* theme default */
    }
}

void save(WuView *v);


/* ---- find / replace now delegate to the opaque FindBar engine (findbar.c) ----
 * The editor keeps only the UI-mode booleans; all search state lives in e->fb. */

void find_close(WuView *v){
    Editor *e = v->priv;
    e->find_mode = 0;
    if (e->fb) findbar_clear_active(e->fb);
    doc_set_selection(e->doc, doc_cursor(e->doc), doc_cursor(e->doc)); /* clear */
}

/* Open find (mode=1) or replace (mode=2). */
void find_open(WuView *v, int mode){
    Editor *e = v->priv;
    if (e->find_mode != mode){ e->find_mode = mode; e->find_focus = 0; }
    e->frames = 0;
}

/* Byte offset of the start of 1-based line `lineN` (clamped to last line). */
void on_wheel(WuView *v, int dy){ (void)v; (void)dy; /* line scroll handled in render via caret */ }

/* Navigator sidebar content: the source function/symbol list from the lexer.
 * Real structure; NULL if the editor has no symbols. Caller frees. */
static char *sidebar(WuView *v){
    Editor *e = v->priv;
    if (!e->doc || !e->cf) return NULL;
    char *st = doc_text(e->doc);
    if (!st) return NULL;
    size_t sl = doc_length(e->doc);
    LexSym syms[256];
    size_t ns = lex_symbols(st, sl, syms, 256);
    free(st);
    if (ns == 0){
        /* Guided empty state — a code editor with no detected symbols still
         * tells the user what this panel is for. */
        return strdup(
            "Navigator — Symbols\n"
            "\n"
            "No symbols detected.\n"
            "Functions/structs you define\n"
            "appear here as a jump list.");
    }
    size_t cap = 64, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    for (size_t k=0; k<ns; k++){
        char nm[64]; size_t L = syms[k].name_len; if (L>63) L=63;
        char *dt = doc_text(e->doc); memcpy(nm, dt+syms[k].name_off, L); nm[L]=0; free(dt);
        char line[96];
        snprintf(line, sizeof line, "%s : L%zu\n", nm, syms[k].line+1);
        size_t add = strlen(line);
        if (len+add+1 > cap){ cap=(len+add+1)*2; char *nb=realloc(out,cap); if(!nb){ free(out); return NULL; } out=nb; }
        memcpy(out+len, line, add); len+=add; out[len]=0;
    }
    return out;
}

static char *status(WuView *v){
    Editor *e = v->priv;
    char *t = doc_text(e->doc);
    size_t cur = doc_cursor(e->doc);
    size_t line=1,col=1; for (size_t q=0;q<cur && t && t[q];q++){ if(t[q]=='\n'){line++;col=1;}else col++; }
    /* live word + character counts (Notepad++ / LibreOffice status parity) */
    size_t words=0, chars=0, inword=0;
    if (t) for (const char *p=t; *p; p++){
        if (*p != ' ' && *p != '\n' && *p != '\t'){ chars++; if(!inword){ inword=1; words++; } }
        else inword = 0;
    }
    free(t);
    const char *lang = e->lex? lex_lang(e->lex) : "none";
    const char *fn = e->path? e->path : "(unsaved)";
    const char *eol = e->eol_crlf? "CRLF" : "LF";
    const char *enc = e->enc_label? e->enc_label : "UTF-8";
    char docn[32]; docn[0]=0;
    if (e->docs && docs_count(e->docs) > 1)
        snprintf(docn,sizeof docn,"[doc %zu/%zu] ", docs_active(e->docs)+1, docs_count(e->docs));
    char buf[256];
    snprintf(buf,sizeof buf,"%s%s  Ln %zu  Col %zu  words %zu  chars %zu  %s  %s  %s  %s  [%s]",
             docn, fn, line, col, words, chars,
             doc_has_selection(e->doc)?"SEL":"   ",
             doc_can_undo(e->doc)?"*":" ", eol, enc, lang);
    return strdup(buf);
}

static void destroy(WuView *v){
    Editor *e = v->priv;
    if (e->lex) lex_free(e->lex);
    if (e->docs) docs_free(e->docs);
    if (e->fb) findbar_destroy(e->fb);
    if (e->ac) autocomp_destroy(e->ac);
    if (e->bk) bkmk_destroy(e->bk);
    if (e->cf) codefold_destroy(e->cf);
    if (e->macro) macro_destroy(e->macro);
    if (e->gto) gotoline_destroy(e->gto);
    if (e->asv){ wubuautosave_clear(e->asv); wubuautosave_destroy(e->asv); }
    if (e->spd) spell_free(e->spd);
    free(e->path);
    free(e);
    free(v);
}

void save(WuView *v){
    Editor *e = v->priv;
    if (!e->docs) return;
    if (!e->path) return;
    char *t = doc_text(e->doc);
    if (t){
        wuos_write_file(e->path, t, strlen(t));
        docs_set_dirty(e->docs, docs_active(e->docs), 0);
        free(t);
    }
    /* INT-2 P0: work was committed to the real file — drop the crash snapshot. */
    if (e->asv){
        wubumodel_doc *m = editor_doc_to_model(e->doc);
        if (m){ wubuautosave_flush(e->asv, m); wubumodel_doc_destroy(m); }
        wubuautosave_clear(e->asv);
    }
}

static const char *get_path(WuView *v){ return ((Editor*)v->priv)->path; }

/* Save-As: point the editor at a new path, then persist (also re-bases the
 * crash-recovery autosave snapshot onto the new file). */
static void set_path(WuView *v, const char *p){
    Editor *e = v->priv;
    if (!e || !p || !*p) return;
    free(e->path);
    e->path = strdup(p);
    if (e->asv){ wubuautosave_destroy(e->asv); e->asv = wubuautosave_create(e->path, 5000); }
    save(v);
}

/* Test/inspection accessor: report find state without exposing the struct. */
int wuos_editor_find_stats(WuView *v, int *active, int *total){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->fb) return -1;
    if (active) *active = findbar_active(e->fb);
    if (total)  *total  = 0;
    { int t=0; findbar_counts(e->fb, NULL, &t); if (total) *total = t; }
    return 0;
}

/* Test accessor: returns the editor's current document text (caller frees). */
char *wuos_editor_text(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e) return NULL;
    return doc_text(e->doc);
}

/* Test accessor: current caret byte offset (for go-to-line assertions). */
size_t wuos_editor_cursor(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    return doc_cursor(e->doc);
}

/* Test accessor: current dark-theme state (for theme-toggle assertion). */
int wuos_editor_dark(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    return e->dark;
}

/* Test accessor: multi-doc session size + active index (for doc-tab tests). */
size_t wuos_editor_doc_count(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->docs) return 0;
    return docs_count(e->docs);
}
size_t wuos_editor_doc_active(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->docs) return 0;
    return docs_active(e->docs);
}
/* Test accessor: number of active bookmarks (line-ops). */
int wuos_editor_bookmarks(WuView *v){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->bk) return 0;
    return bkmk_count(e->bk);
}
/* Test accessor: spell-check a word through the editor's attached dictionary
 * (INT-8). Returns 1 if known, 0 if misspelled, -1 if no spell engine. */
int wuos_editor_spell(WuView *v, const char *word){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->spd) return -1;
    return spell_check(e->spd, word);
}
/* Test accessor: column/block selection state (mode + block bounds). */
int wuos_editor_col(WuView *v, int *l0, int *c0, int *l1, int *c1){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    if (l0) *l0 = e->sel_l0;
    if (c0) *c0 = e->sel_c0;
    if (l1) *l1 = e->sel_l1;
    if (c1) *c1 = e->sel_c1;
    return e->col_mode;
}
/* Test accessor: macro record state (recording flag + recorded op count). */
int wuos_editor_macro(WuView *v, int *ops){
    Editor *e = v ? v->priv : NULL;
    if (!e) return 0;
    if (ops) *ops = e->macro ? macro_count(e->macro) : 0;
    return e->macro ? macro_recording(e->macro) : 0;
}
/* Test accessor: auto-completion popup state (open + candidate count + sel). */
int wuos_editor_ac(WuView *v, int *n, int *sel){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->ac) return 0;
    if (n) *n = autocomp_count(e->ac);
    if (sel) *sel = autocomp_selected(e->ac);
    return autocomp_opened(e->ac);
}
/* Test accessor: folded-line count + function-list panel state. */
int wuos_editor_fold(WuView *v, int *count){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->cf) return 0;
    int c = codefold_folded_count(e->cf);
    if (count) *count = c;
    return c;   /* nonzero if anything folded */
}
int wuos_editor_sym(WuView *v, int *n){
    Editor *e = v ? v->priv : NULL;
    if (!e || !e->cf) return 0;
    if (n){ char *t=doc_text(e->doc); size_t len=doc_length(e->doc); LexSym s[256];
            size_t k=lex_symbols(t,len,s,256); free(t); *n=(int)k; }
    return codefold_symmode(e->cf);
}

/* ---- wubuautosave bridge (INT-2 P0) ----
 * The Editor's live buffer is a WuBuPad Doc (piece table); wubuautosave
 * snapshots a wubumodel_doc. These two helpers cross the model boundary so
 * the real crash-recovery engine actually protects the edited text. */

/* Build a single-section wubumodel_doc holding the Doc's current text. */
wubumodel_doc *editor_doc_to_model(const void *d){
    wubumodel_doc *m = wubumodel_doc_create();
    if (!m) return NULL;
    wubumodel_node *sec  = wubumodel_node_create(m, WUBUMODEL_SECTION);
    wubumodel_node *para = wubumodel_node_create(m, WUBUMODEL_PARAGRAPH);
    wubumodel_node *run  = wubumodel_node_create(m, WUBUMODEL_RUN);
    char *t = doc_text((void*)d);
    if (t){ wubumodel_run_set_text(run, t); free(t); }
    wubumodel_node_append(m, para, run);
    wubumodel_node_append(m, sec, para);
    return m;
}

/* Walk a wubumodel_doc's runs, returning a malloc'd concatenation (caller frees). */
char *editor_model_text(const wubumodel_doc *m){
    wubumodel_node *sec = wubumodel_doc_root(m);
    if (!sec) return NULL;
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = 0;
    for (wubumodel_node *p = wubumodel_node_first_child(sec); p;
         p = wubumodel_node_next_sibling(p)){
        for (wubumodel_node *r = wubumodel_node_first_child(p); r;
             r = wubumodel_node_next_sibling(r)){
            const char *rt = wubumodel_run_text(r);
            if (!rt) continue;
            size_t rl = strlen(rt);
            if (len + rl + 1 >= cap){ cap = len + rl + 256; char *n = realloc(out, cap); if(!n){ free(out); return NULL; } out = n; }
            memcpy(out + len, rt, rl + 1);
            len += rl;
        }
    }
    return out;
}

WuView *wuos_editor_create(const char *path){
    Editor *e = calloc(1, sizeof *e);
    if (!e) return NULL;
    /* follow the shell's theme so a dark app doesn't show a light editor
     * (was hardcoded light; the syntax palette IS theme-aware, only the bg
     * flag was wrong) */
    e->dark = wubusettings_dark(wubusettings_shared());

    const char *seed =
        "/* WuBuPad -- Notepad++ parity, embedded in WuBuOffice */\n"
        "#include <stdio.h>\n"
        "\n"
        "int main(void) {\n"
        "    int total = 0;\n"
        "    for (int i = 1; i <= 10; i++) {\n"
        "        total += i;            /* sum 1..10 */\n"
        "        if (total > 50) break; // early out\n"
        "    }\n"
        "    printf(\"sum=%d\\n\", total);\n"
        "    return 0;\n"
        "}\n";

    e->docs = docs_create();
    if (!e->docs){ free(e); return NULL; }
    e->fb = findbar_create();
    if (!e->fb){ docs_free(e->docs); free(e); return NULL; }
    e->ac = autocomp_create();
    if (!e->ac){ findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->bk = bkmk_create();
    if (!e->bk){ autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->cf = codefold_create();
    if (!e->cf){ bkmk_destroy(e->bk); autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->macro = macro_create();
    if (!e->macro){ codefold_destroy(e->cf); bkmk_destroy(e->bk); autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }
    e->gto = gotoline_create();
    if (!e->gto){ macro_destroy(e->macro); codefold_destroy(e->cf); bkmk_destroy(e->bk); autocomp_destroy(e->ac); findbar_destroy(e->fb); docs_free(e->docs); free(e); return NULL; }

    if (path){
        /* load via docs (detects encoding, seeds Doc); sets active doc */
        size_t idx = docs_load_file(e->docs, path, NULL);
        if (idx == SIZE_MAX){
            /* read failed: open blank with the requested name */
            docs_open(e->docs, path, "", "c");
        }
    } else if (getenv("WUBUOS_RESTORE")){
        /* no file arg + restore requested: reopen the saved session */
        session_restore(e);
    }
    if (docs_count(e->docs) == 0){
        docs_open(e->docs, NULL, seed, "c");
    }
    editor_sync_active(e);

    /* INT-2 P0: attach real crash-recovery autosave to the live editor.
     * Only meaningful for a real on-disk file; the seed/blank doc has no
     * path to snapshot against. Offer to recover a previous crash snapshot. */
    if (e->path){
        if (wubuautosave_has_recovery(e->path)){
            wubumodel_doc *rec = NULL;
            if (wubuautosave_recover(e->path, &rec) > 0 && rec){
                /* splice recovered text over the active doc */
                char *rt = editor_model_text(rec);
                if (rt){ doc_replace(e->doc, 0, doc_length(e->doc), rt); free(rt); }
                wubumodel_doc_destroy(rec);
                wubuautosave_discard_recovery(e->path);
            }
        }
        e->asv = wubuautosave_create(e->path, 5000);  /* 5s min gap */
        e->asv_len = doc_length(e->doc);
    }

    /* INT-8 P0: wire the spell engine so the editor shows live red squiggles
     * under misspelled words. Seed with built-in English + load any user dict. */
    {
        e->spd = spell_create();
        if (e->spd){
            spell_seed_english(e->spd);
            const char *ud = getenv("WUBUOS_DICT");
            if (ud) spell_load(e->spd, ud);
        }
    }

    /* pick lexer by active doc extension (default c) */
    const char *lang = "c";
    const char *ap = e->path;
    if (ap){
        const char *dot = strrchr(ap, '.');
        if (dot){
            if      (!strcasecmp(dot, ".json")) lang = "json";
            else if (!strcasecmp(dot, ".h") || !strcasecmp(dot, ".cxx") ||
                     !strcasecmp(dot, ".cpp") || !strcasecmp(dot, ".cc")) lang = "c";
            else if (!strcasecmp(dot, ".py")) lang = "c"; /* lexer has no python; reuse c */
        }
    }
    e->lex = lex_create(lang);
    e->frames = 0;
    WuView *v = calloc(1, sizeof *v);
    v->name = "Editor";
    v->priv = e;
    v->destroy  = destroy;
    v->render   = render;
    v->on_key   = on_key;
    v->on_wheel = on_wheel;
    v->status   = status;
    v->sidebar  = sidebar;
    v->save     = save;
    v->get_path = get_path;
    v->set_path = set_path;
    return v;
}
