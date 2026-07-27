/* Headless smoke test: create + render every view (real engines, no GUI).
 * Also exercises file open (doc markdown + editor code) and Ctrl+S save. */
#include "wuos.h"
#include "wuos_font.h"
#include "wuos_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int render_check(WuView *v, const char *name){
    unsigned char *rgba=NULL; int w=0,h=0;
    int rc = v->render(v, 960, 664, 0, &rgba, &w, &h);
    if (rc!=0 || !rgba){ fprintf(stderr,"[%s] render FAILED rc=%d\n", name, rc); free(rgba); return 1; }
    fprintf(stderr,"[%s] ok %dx%d\n", name, w, h);
    free(rgba);
    /* status bar text must be produced (the live shell paints it) */
    char *st = v->status? v->status(v): NULL;
    if (st){
        if (st[0]=='\0'){ fprintf(stderr,"[%s] status empty\n", name); free(st); return 1; }
        free(st);
    }
    /* tab label must be measurable text (the live shell paints it) */
    static unsigned char scratch[8*64*4];
    int adv = wuos_font_draw(name, 0, 0, 0, 0,0,0, scratch, 8, 64);
    if (adv <= 0){ fprintf(stderr,"[%s] tab label unmeasurable\n", name); return 1; }
    return 0;
}

int main(void){
    if (wuos_font_init()!=0){ fprintf(stderr,"font init FAILED\n"); return 2; }

    /* no-path views */
    struct { const char *name; WuView *(*mk)(const char*); } tbl[] = {
        {"doc", wuos_doc_create},
        {"cell", wuos_cell_create},
        {"slide", wuos_slide_create},
        {"ocr",  wuos_ocr_create},
        {"editor", wuos_editor_create},
        {NULL,NULL}
    };
    int bad=0;
    for (int i=0; tbl[i].name; i++){
        WuView *v = tbl[i].mk(NULL);
        if (!v){ fprintf(stderr,"[%s] create FAILED\n", tbl[i].name); bad++; continue; }
        bad += render_check(v, tbl[i].name);
        v->destroy(v);
    }

    /* file open: markdown -> Document, code -> Editor */
    const char *md = "# Title\n\nHello **world** paragraph.\n\n## Section\n\nAnother line.\n";
    const char *code = "int main(){\n  return 0; // done\n}\n";
    char mdp[256], codep[256];
    sprintf(mdp,  "/tmp/wuos_test_%d.md",  (int)getpid());
    sprintf(codep,"/tmp/wuos_test_%d.c",  (int)getpid());
    wuos_write_file(mdp, md, strlen(md));
    wuos_write_file(codep, code, strlen(code));

    WuView *dv = wuos_doc_create(mdp);
    if (dv){ bad += render_check(dv, "doc(file)");
             if (dv->get_path && strcmp(dv->get_path(dv), mdp)!=0){ fprintf(stderr,"doc get_path mismatch\n"); bad++; }
             dv->destroy(dv); }
    else { fprintf(stderr,"[doc(file)] create FAILED\n"); bad++; }

    WuView *ev = wuos_editor_create(codep);
    if (ev){ bad += render_check(ev, "editor(file)");
             /* type + save */
             ev->on_key(ev, 'X', 1);
             ev->save(ev);
             /* re-read saved file, ensure it grew by the typed 'X' */
             size_t sz=0; char *saved = wuos_read_file(codep, &sz);
             if (!saved || sz <= strlen(code)){ fprintf(stderr,"editor save FAILED (sz=%zu)\n", sz); bad++; }
             free(saved);
             ev->destroy(ev); }
    else { fprintf(stderr,"[editor(file)] create FAILED\n"); bad++; }

    /* find/replace logic: build a doc, find all 'int', replace-all -> 'INT' */
    {
        WuView *fv = wuos_editor_create(NULL);
        if (!fv){ fprintf(stderr,"[find] create FAILED\n"); bad++; }
        else {
            /* type a known text (appended after the sample) */
            const char *src = "int a; int b; int c;";
            for (const char *p=src; *p; p++) fv->on_key(fv, (unsigned char)*p, 1);
            /* open find + set query 'int' + activate via F3 */
            fv->on_key(fv, WUOS_KEY_FIND, 1);
            for (const char *p="int"; *p; p++) fv->on_key(fv, (unsigned char)*p, 1);
            fv->on_key(fv, WUOS_KEY_FINDNEXT, 1);
            int active=0, total=0;
            wuos_editor_find_stats(fv, &active, &total);
            if (!active){ fprintf(stderr,"[find] no match found\n"); bad++; }
            else if (total < 3){ fprintf(stderr,"[find] total=%d want >=3\n", total); bad++; }
            /* replace-all: open replace mode, Tab to replace field, set 'INT', Ctrl+R */
            fv->on_key(fv, WUOS_KEY_REPLACE, 1);
            fv->on_key(fv, WUOS_KEY_TAB, 1);          /* focus replace field */
            for (const char *p="INT"; *p; p++) fv->on_key(fv, (unsigned char)*p, 1);
            fv->on_key(fv, WUOS_KEY_REPLACEALL, 1);
            char *ft = wuos_editor_text(fv);
            int cnt = 0; const char *q = ft;
            while ((q = strstr(q, "INT"))){ cnt++; q++; }
            if (cnt < 3){ fprintf(stderr,"[find] replace-all INT count=%d want >=3\n", cnt); bad++; }
            if (strstr(ft, "int ")){ fprintf(stderr,"[find] lowercase 'int ' still present\n"); bad++; }
            free(ft);
            fv->destroy(fv);
            fprintf(stderr,"[find] ok (%d matches, %d replaced)\n", total, cnt);
        }
    }

    /* go-to-line: Ctrl+G, type "3", Enter -> caret at start of line 3 */
    {
        WuView *gv = wuos_editor_create(NULL);
        if (!gv){ fprintf(stderr,"[goto] create FAILED\n"); bad++; }
        else {
            gv->on_key(gv, WUOS_KEY_GOTO, 1);
            for (const char *p="3"; *p; p++) gv->on_key(gv, (unsigned char)*p, 1);
            gv->on_key(gv, WUOS_KEY_RETURN, 1);
            size_t c = wuos_editor_cursor(gv);
            char *t = wuos_editor_text(gv);
            /* count newlines before c; must be exactly 2 (start of line 3) */
            int nl=0; for (size_t q=0;q<c && t && t[q];q++) if (t[q]=='\n') nl++;
            if (nl != 2){ fprintf(stderr,"[goto] line=%d want 3 (cursor %zu)\n", nl+1, c); bad++; }
            else fprintf(stderr,"[goto] ok (line 3, cursor %zu)\n", c);
            free(t);
            gv->destroy(gv);
        }
    }

    /* EOL convert: Ctrl+E toggles LF <-> CRLF */
    {
        WuView *ev2 = wuos_editor_create(NULL);
        if (!ev2){ fprintf(stderr,"[eol] create FAILED\n"); bad++; }
        else {
            /* seed is LF; convert to CRLF */
            ev2->on_key(ev2, WUOS_KEY_EOL, 1);
            char *t1 = wuos_editor_text(ev2);
            int has_crlf = (strstr(t1, "\r\n") != NULL);
            if (!has_crlf){ fprintf(stderr,"[eol] CRLF convert failed\n"); bad++; }
            free(t1);
            /* back to LF */
            ev2->on_key(ev2, WUOS_KEY_EOL, 1);
            char *t2 = wuos_editor_text(ev2);
            int still_crlf = (strstr(t2, "\r\n") != NULL);
            if (still_crlf){ fprintf(stderr,"[eol] LF convert failed\n"); bad++; }
            free(t2);
            if (!has_crlf || still_crlf) { /* counted above */ }
            else fprintf(stderr,"[eol] ok (LF<->CRLF)\n");
            ev2->destroy(ev2);
        }
    }

    /* dark theme: Ctrl+` toggles; dark render has dark background pixels */
    {
        WuView *tv = wuos_editor_create(NULL);
        if (!tv){ fprintf(stderr,"[theme] create FAILED\n"); bad++; }
        else {
            if (wuos_editor_dark(tv) != 0){ fprintf(stderr,"[theme] default not light\n"); bad++; }
            tv->on_key(tv, WUOS_KEY_THEME, 1);
            if (wuos_editor_dark(tv) != 1){ fprintf(stderr,"[theme] toggle failed\n"); bad++; }
            unsigned char *rgba=NULL; int w=0,h=0;
            int rc = tv->render(tv, 960, 664, 0, &rgba, &w, &h);
            if (rc!=0 || !rgba){ fprintf(stderr,"[theme] render FAILED\n"); bad++; }
            else {
                /* top-left pixel should be dark (~30,33,40) */
                int dr=(int)rgba[0], dg=(int)rgba[1], db=(int)rgba[2];
                if (!(dr<80 && dg<80 && db<80)){ fprintf(stderr,"[theme] bg not dark (%d,%d,%d)\n",dr,dg,db); bad++; }
                else fprintf(stderr,"[theme] ok (dark bg %d,%d,%d)\n", dr,dg,db);
            }
            free(rgba);
            tv->destroy(tv);
        }
    }

    /* multi-doc: Ctrl+T new, Ctrl+Tab cycle, Ctrl+W close */
    {
        WuView *mv = wuos_editor_create(NULL);
        if (!mv){ fprintf(stderr,"[multidoc] create FAILED\n"); bad++; }
        else {
            size_t c0 = wuos_editor_doc_count(mv);
            mv->on_key(mv, WUOS_KEY_NEWDOC, 1);
            size_t c1 = wuos_editor_doc_count(mv);
            if (c1 != c0+1){ fprintf(stderr,"[multidoc] new failed (%zu->%zu)\n", c0, c1); bad++; }
            else {
                /* type into the new (active) doc */
                for (const char *p="NEWDOC_MARKER"; *p; p++) mv->on_key(mv, (unsigned char)*p, 1);
                /* cycle to previous doc (seed) */
                mv->on_key(mv, WUOS_KEY_DOCPREV, 1);
                size_t a2 = wuos_editor_doc_active(mv);
                char *t0 = wuos_editor_text(mv);
                int has_seed = (strstr(t0, "WuBuPad -- Notepad++") != NULL);
                if (a2 != 0 || !has_seed){ fprintf(stderr,"[multidoc] cycle fail a=%zu seed=%d\n", a2, has_seed); bad++; }
                free(t0);
                /* close the seed doc -> new doc (with marker) remains */
                mv->on_key(mv, WUOS_KEY_CLOSE, 1);
                size_t c2 = wuos_editor_doc_count(mv);
                char *t1 = wuos_editor_text(mv);
                int has_marker = (strstr(t1, "NEWDOC_MARKER") != NULL);
                if (c2 != c1-1 || !has_marker){ fprintf(stderr,"[multidoc] close fail c=%zu marker=%d\n", c2, has_marker); bad++; }
                else fprintf(stderr,"[multidoc] ok (count %zu->%zu, cycle+close)\n", c0, c2);
                free(t1);
            }
            mv->destroy(mv);
        }
    }

    /* bookmarks: Ctrl+F2 toggle, F2 next/prev */
    {
        WuView *bv = wuos_editor_create(NULL);
        if (!bv){ fprintf(stderr,"[bk] create FAILED\n"); bad++; }
        else {
            bv->on_key(bv, WUOS_KEY_TOGGLE_BK, 1);      /* mark line 0 */
            /* move to line 3 then mark */
            bv->on_key(bv, WUOS_KEY_GOTO, 1);
            for (const char *p="3"; *p; p++) bv->on_key(bv, (unsigned char)*p, 1);
            bv->on_key(bv, WUOS_KEY_RETURN, 1);
            bv->on_key(bv, WUOS_KEY_TOGGLE_BK, 1);      /* mark line 2 */
            int n = wuos_editor_bookmarks(bv);
            if (n != 2){ fprintf(stderr,"[bk] count!=2 (%d)\n", n); bad++; }
            else {
                /* go to line 1, next bookmark should jump toward line 2 */
                bv->on_key(bv, WUOS_KEY_GOTO, 1);
                for (const char *p="1"; *p; p++) bv->on_key(bv, (unsigned char)*p, 1);
                bv->on_key(bv, WUOS_KEY_RETURN, 1);
                bv->on_key(bv, WUOS_KEY_NEXT_BK, 1);
                size_t cur = wuos_editor_cursor(bv);
                /* line 2 offset = position of 3rd newline+1 */
                if (cur < 3){ fprintf(stderr,"[bk] next failed cur=%zu\n", cur); bad++; }
                else {
                    /* toggle off line 2 by being on it again */
                    bv->on_key(bv, WUOS_KEY_TOGGLE_BK, 1);
                    int n2 = wuos_editor_bookmarks(bv);
                    if (n2 != 1){ fprintf(stderr,"[bk] unmark failed (%d)\n", n2); bad++; }
                    else fprintf(stderr,"[bk] ok (2 -> 1, jump works)\n");
                }
            }
            bv->destroy(bv);
        }
    }

    /* cell view: load a real CSV via wubucell reader */
    {
        char csvp[256]; sprintf(csvp, "/tmp/wuos_cell_%d.csv", (int)getpid());
        const char *csv = "alpha,beta,gamma\n10,20,30\n4,5,6\n";
        wuos_write_file(csvp, csv, strlen(csv));
        WuView *cv = wuos_cell_create(csvp);
        if (!cv){ fprintf(stderr,"[cell(csv)] create FAILED\n"); bad++; }
        else {
            bad += render_check(cv, "cell(csv)");
            cv->destroy(cv);
        }
        remove(csvp);
    }

    /* compare view: diff two temp files via WuBuPad Myers engine */
    {
        char ap[256], bp[256];
        sprintf(ap,"/tmp/wuos_cmp_a_%d.txt",(int)getpid());
        sprintf(bp,"/tmp/wuos_cmp_b_%d.txt",(int)getpid());
        wuos_write_file(ap, "line one\nline two\nline three\n", 29);
        wuos_write_file(bp, "line one\nline TWO\nline three\n", 29);
        WuView *cmp = wuos_compare_create(ap, bp);
        if (!cmp){ fprintf(stderr,"[compare] create FAILED\n"); bad++; }
        else {
            bad += render_check(cmp, "compare");
            cmp->destroy(cmp);
        }
        remove(ap); remove(bp);
    }

    /* cleanup temp files */
    remove(mdp); remove(codep);

    wuos_font_quit();
    fprintf(stderr,"done bad=%d\n", bad);
    return bad?1:0;
}
