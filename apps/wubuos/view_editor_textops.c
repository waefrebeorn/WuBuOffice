/* view_editor_textops.c -- editor text/session operations split from
 * view_editor.c (no monoliths): EOL handling, caret movement, session
 * persistence, bookmarks/fold/symbol navigation. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wuos.h"
#include "view_editor_internal.h"
#include "doc.h"
#include "lex.h"
#include "encode.h"
#include "docs.h"
#include "codefold.h"
#include "bkmk.h"
size_t doc_offset_of_line(Doc *d, int lineN){
    char *t = doc_text(d);
    size_t n = doc_length(d);
    int cur = 1;
    if (lineN <= 1){ free(t); return 0; }
    for (size_t q=0; q<n; q++){
        if (t[q]=='\n'){ cur++; if (cur == lineN){ free(t); return q+1; } }
    }
    free(t);
    /* lineN beyond end: return end of doc */
    return n;
}

/* Detect current EOL style from text (1 if any CRLF present, else 0=LF). */
int detect_eol(const char *t){
    if (!t) return 0;
    for (size_t q=0; t[q]; q++) if (t[q]=='\r' && t[q+1]=='\n') return 1;
    return 0;
}

/* Convert the whole document between LF and CRLF in place. */
void convert_eol(Editor *e, int to_crlf){
    if (e->eol_crlf == to_crlf) return;
    char *t = doc_text(e->doc);
    size_t n = doc_length(e->doc);
    /* worst case: every char is '\n' -> doubles (+1 for NUL) */
    char *out = malloc(n*2 + 1);
    if (!out){ free(t); return; }
    size_t o=0;
    for (size_t q=0; q<n; q++){
        if (to_crlf && t[q]=='\n' && (q==0 || t[q-1]!='\r')) out[o++]='\r';
        else if (!to_crlf && t[q]=='\r' && t[q+1]=='\n') continue; /* drop \r */
        out[o++] = t[q];
    }
    out[o]=0;
    doc_replace(e->doc, 0, n, out);
    free(out); free(t);
    e->eol_crlf = to_crlf;
}

/* Point e->doc at the active document in the session and refresh EOL/encoding
 * from its current text. Called after any doc switch. */
void editor_sync_active(Editor *e){
    if (!e->docs) return;
    size_t a = docs_active(e->docs);
    e->doc = docs_doc(e->docs, a);
    if (e->doc){
        char *t = doc_text(e->doc);
        e->eol_crlf = detect_eol(t);
        free(t);
    }
    const char *p = docs_path(e->docs, a);
    free(e->path);
    e->path = (p && *p)? strdup(p) : NULL;   /* owned copy */
    e->enc_label = "UTF-8";
}

/* ---- session save/restore (Notepad++ session model) ---- */
const char *session_path(void){
    const char *e = getenv("WUBUOS_SESSION");
    return e && *e ? e : "~/.wubuos_session";
}
/* write the open document list (one path per line, "(untitled)" for none) */
void session_save(Editor *e){
    if (!e->docs) return;
    char *fn = strdup(session_path());
    /* expand leading ~ */
    if (fn[0]=='~'){ const char *home=getenv("HOME"); size_t hl=home?strlen(home):0;
        char *exp=malloc(hl+strlen(fn)+2); if(home) strcpy(exp,home); strcat(exp, fn+1);
        free(fn); fn=exp; }
    FILE *f = fopen(fn, "w");
    if (f){
        size_t n = docs_count(e->docs);
        for (size_t i=0;i<n;i++){
            const char *p = docs_path(e->docs, i);
            fprintf(f, "%s\n", (p && *p)? p : "(untitled)");
        }
        fclose(f);
    }
    free(fn);
}
/* reopen the saved document list (called when launching with no file arg) */
void session_restore(Editor *e){
    char *fn = strdup(session_path());
    if (fn[0]=='~'){ const char *home=getenv("HOME"); size_t hl=home?strlen(home):0;
        char *exp=malloc(hl+strlen(fn)+2); if(home) strcpy(exp,home); strcat(exp, fn+1);
        free(fn); fn=exp; }
    FILE *f = fopen(fn, "r");
    free(fn);
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof line, f)){
        size_t L = strlen(line); while (L>0 && (line[L-1]=='\n'||line[L-1]=='\r')) line[--L]=0;
        if (!*line) continue;
        if (!strcmp(line, "(untitled)")) docs_open(e->docs, NULL, "", "c");
        else docs_open(e->docs, line, "", "c");
    }
    fclose(f);
}

/* current caret line (0-based) */
int editor_line_of(Editor *e){
    char *t = doc_text(e->doc);
    size_t cur = doc_cursor(e->doc), ln = 0;
    for (size_t q=0; q<cur; q++) if (t[q]=='\n') ln++;
    free(t);
    return (int)ln;
}
/* current caret column (0-based, in chars) */
int editor_col_of(Editor *e){
    char *t = doc_text(e->doc);
    size_t cur = doc_cursor(e->doc), c = 0;
    while (cur>0 && t[cur-1]!='\n'){ cur--; c++; }
    free(t);
    return (int)c;
}
/* move caret up/down `dl` lines, keeping the same char column */
void editor_caret_vert(Editor *e, int dl){
    char *t = doc_text(e->doc);
    size_t n = doc_length(e->doc);
    int cl = editor_line_of(e), cc = editor_col_of(e);
    cl += dl; if (cl<0) cl=0;
    /* find start of line cl */
    size_t p=0; int ln=0;
    while (ln<cl && p<n){ if (t[p]=='\n') ln++; p++; }
    /* advance cc chars (or to EOL) */
    size_t q=p; int c=0;
    while (q<n && t[q]!='\n' && c<cc){ q++; c++; }
    doc_set_cursor(e->doc, q);
    free(t);
}

/* ---- auto-completion (collect identifiers from doc + builtins) ---- */
/* ---- code-folding + function-list now delegate to opaque CodeFold (codefold.c) ---- */
void fold_toggle_block(Editor *e){
    if (e->cf) codefold_toggle_block(e->cf, e->doc, editor_line_of(e));
}
void sym_toggle(Editor *e){ if (e->cf) codefold_sym_toggle(e->cf); }

void bk_jump(Editor *e, int dir){   /* dir +1 next, -1 prev */
    if (!e->bk) return;
    int cl = editor_line_of(e);
    int best = bkmk_jump(e->bk, cl, dir);
    if (best < 0) return;
    size_t off = doc_offset_of_line(e->doc, best+1);
    doc_set_cursor(e->doc, off);
}

