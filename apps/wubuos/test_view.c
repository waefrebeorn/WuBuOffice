/* Headless smoke test: create + render every view (real engines, no GUI).
 * Also exercises file open (doc markdown + editor code) and Ctrl+S save. */
#include "wuos.h"
#include "wuos_font.h"
#include "settings.h"
#include "toc.h"
#include "palette.h"     /* DOC-42: command-palette keyboard-nav test */
#include "wuos_file.h"
#include "autosave.h"   /* wubuautosave: editor crash-recovery test */
#include "model.h"      /* wubumodel_doc: build snapshot in autosave test */
#include "cell.h"       /* wubucell: spreadsheet round-trip save check */
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
        /* Navigator sidebar content callback must be safe to call for every
         * view (may return NULL when the view has no structure). */
        if (v->sidebar){
            char *sb = v->sidebar(v);
            if (sb && !sb[0]){ fprintf(stderr,"[%s] sidebar empty string\n", tbl[i].name); bad++; }
            free(sb);
        }
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

    WuView *sv = wuos_settings_create();
    if (sv){ bad += render_check(sv, "settings");
             /* UI-24/25: settings view edits the shared config; verify a render
              * produced a buffer and status reports a theme. */
             char *sts = sv->status(sv);
             if (!sts || !strstr(sts, "Settings")){ fprintf(stderr,"[settings] status missing\n"); bad++; }
             else fprintf(stderr,"[settings] ok (%s)\n", sts);
             free(sts);
             sv->destroy(sv); }
    else { fprintf(stderr,"[settings] create FAILED\n"); bad++; }

    WuView *dv = wuos_doc_create(mdp);
    if (dv){ bad += render_check(dv, "doc(file)");
             /* Navigator sidebar: a markdown file with "# Title" must yield a
              * TOC heading in the docked sidebar content. */
             if (dv->sidebar){
                 char *sb = dv->sidebar(dv);
                 if (!sb || !strstr(sb, "Title")){ fprintf(stderr,"[doc] sidebar missing TOC heading\n"); bad++; }
                 else fprintf(stderr,"[doc] sidebar ok (TOC heading present)\n");
                 free(sb);
             }
             if (dv->get_path && strcmp(dv->get_path(dv), mdp)!=0){ fprintf(stderr,"doc get_path mismatch\n"); bad++; }
             /* INT-1/3: chart/draw/math engines are now wired into the Document
              * tab (rasterized via the new wubusvg rasterizer). Assert the
              * seeded chart overlay exists and Insert adds draw/math. */
             int oc0 = wuos_doc_obj_count(dv);
             if (oc0 < 1){ fprintf(stderr,"[doc] no inserted chart overlay\n"); bad++; }
             dv->on_key(dv, WUOS_KEY_INSERT_DRAW, 1);
             dv->on_key(dv, WUOS_KEY_INSERT_MATH, 1);
             int oc1 = wuos_doc_obj_count(dv);
             if (oc1 < oc0 + 2){ fprintf(stderr,"[doc] insert draw/math failed (oc %d->%d)\n", oc0, oc1); bad++; }
             else fprintf(stderr,"[doc] insert ok (%d overlays)\n", oc1);
             /* INT-4: EPUB export writes a real file */
             dv->on_key(dv, WUOS_KEY_EXPORT_EPUB, 1);
             const char *em = wuos_doc_epub_msg(dv);
             if (!em || !strstr(em, "EPUB written")){ fprintf(stderr,"[doc] epub export failed ('%s')\n", em?em:"(null)"); bad++; }
             else fprintf(stderr,"[doc] epub ok\n");
             /* INT-5: a11y check runs and reports a count */
             dv->on_key(dv, WUOS_KEY_A11Y_CHECK, 1);
             int ai = wuos_doc_a11y_issues(dv);
             if (ai < 0){ fprintf(stderr,"[doc] a11y check not run\n"); bad++; }
             else {
                 fprintf(stderr,"[doc] a11y ok (%d issues)\n", ai);
                 /* DOC-46: issues are reported inline with real text */
                 int have_text=0;
                 for (int i=0;i<ai;i++){ const char *s=wuos_doc_a11y_item(dv,i); if (s && *s) have_text=1; }
                 if (ai>0 && !have_text){ fprintf(stderr,"[doc] a11y issues missing text\n"); bad++; }
                 else fprintf(stderr,"[doc] a11y inline report ok\n");
             }
             /* DOC-42: command palette is fully keyboard-navigable (no mouse).
              * Exercises open -> type-to-filter -> arrow move -> confirm. */
             {
                 Palette *pp = palette_create();
                 palette_add(pp, "Open File", 1);
                 palette_add(pp, "Save Document", 2);
                 palette_add(pp, "Style: Heading 1", 3);
                 palette_add(pp, "Insert: Script Field", 4);
                 palette_open(pp);
                 if (!palette_is_open(pp)){ fprintf(stderr,"[kbd] palette did not open\n"); bad++; }
                 /* type "styl" to filter down to the Heading command */
                 const char *q = "styl";
                 for (int i=0;q[i];i++) palette_input(pp, q[i]);
                 int rc = palette_result_count(pp);
                 int found_h1 = 0;
                 for (int i=0;i<rc;i++)
                     if (strcmp(palette_result_label(pp,i),"Style: Heading 1")==0) found_h1=1;
                 if (!found_h1){ fprintf(stderr,"[kbd] type-to-filter failed (count=%d)\n", rc); bad++; }
                 else {
                     /* move selection to the Heading entry and confirm via keyboard */
                     int sel = -1;
                     for (int i=0;i<rc;i++) if (strcmp(palette_result_label(pp,i),"Style: Heading 1")==0) sel=i;
                     while (palette_selected(pp) != sel) palette_next(pp); /* arrow-down nav */
                     int cid = palette_confirm(pp);
                     if (cid != 3){ fprintf(stderr,"[kbd] keyboard confirm wrong id (%d)\n", cid); bad++; }
                     else fprintf(stderr,"[kbd] palette keyboard-nav ok (id=%d)\n", cid);
                     if (palette_is_open(pp)){ fprintf(stderr,"[kbd] palette still open after confirm\n"); bad++; }
                     palette_destroy(pp);
                 }
             }
             /* DOC-97: wubuscript exposed as a computed field. Use a doc view
              * with a model (NULL path -> sample doc) so the insert lands. */
             {
                 WuView *sv2 = wuos_doc_create(NULL);
                 if (!sv2){ fprintf(stderr,"[doc] script view create FAILED\n"); bad++; }
                 else {
                     sv2->on_key(sv2, WUOS_KEY_INSERT_SCRIPT, 1);
                     int found=0;
                     wubumodel_doc *mdl = wuos_doc_model(sv2);
                     wubumodel_node *root = mdl ? wubumodel_doc_root(mdl) : NULL;
                     for (wubumodel_node *sec=root; sec; sec=wubumodel_node_next_sibling(sec)){
                         for (wubumodel_node *n=wubumodel_node_first_child(sec); n; n=wubumodel_node_next_sibling(n)){
                             if (wubumodel_node_kind(n)==WUBUMODEL_FIELD && strcmp(wubumodel_node_field(n),"script")==0) found=1;
                             /* fields may also sit inside a paragraph */
                             for (wubumodel_node *g=wubumodel_node_first_child(n); g; g=wubumodel_node_next_sibling(g))
                                 if (wubumodel_node_kind(g)==WUBUMODEL_FIELD && strcmp(wubumodel_node_field(g),"script")==0) found=1;
                         }
                     }
                     if (!found){ fprintf(stderr,"[doc] script field not inserted\n"); bad++; }
                     else fprintf(stderr,"[doc] wubuscript field ok\n");
                     sv2->destroy(sv2);
                 }
             }
             dv->destroy(dv); }
             else { fprintf(stderr,"[doc(file)] create FAILED\n"); bad++; }

    WuView *ev = wuos_editor_create(codep);
    if (ev){ bad += render_check(ev, "editor(file)");
             /* status must report live word/char counts (Notepad++ parity) */
             if (ev->status){
                 char *st = ev->status(ev);
                 if (!st || !strstr(st, "words ")){ fprintf(stderr,"[editor] status missing word count\n"); bad++; }
                 else fprintf(stderr,"[editor] status ok (has word count)\n");
                 free(st);
             }
             /* type + save */
             ev->on_key(ev, 'X', 1);
             ev->save(ev);
             /* re-read saved file, ensure it grew by the typed 'X' */
             size_t sz=0; char *saved = wuos_read_file(codep, &sz);
             if (!saved || sz <= strlen(code)){ fprintf(stderr,"editor save FAILED (sz=%zu)\n", sz); bad++; }
             free(saved);
             ev->destroy(ev); }
    else { fprintf(stderr,"[editor(file)] create FAILED\n"); bad++; }

    /* UI-41: active-line highlight renders as a subtle full-row tint behind
     * the caret line (modern editor affordance). Headless pixel check. */
    {
        WuView *al = wuos_editor_create(NULL);
        if (!al){ fprintf(stderr,"[active-line] create FAILED\n"); bad++; }
        else {
            int w=0,h=0; unsigned char *rgba=NULL;
            int rc = al->render(al, 960, 664, 0, &rgba, &w, &h);
            if (rc!=0 || !rgba){ fprintf(stderr,"[active-line] render FAILED\n"); bad++; }
            else {
                /* find a contiguous band of rows (in the text area x>60) whose
                 * average brightness sits between pure-bg and pure-white (i.e.
                 * the tinted active line), excluding status/tab chrome. */
                int band=0;
                for (int y=30; y<h-26 && !band; y++){
                    long sr=0,sg=0,sb=0,n=0;
                    for (int x=60; x<400; x+=5){ int i=(y*w+x)*4; sr+=rgba[i];sg+=rgba[i+1];sb+=rgba[i+2]; n++; }
                    int mr=sr/n, mg=sg/n, mb=sb/n;
                    if (mr>=236 && mr<=252 && mg>=236 && mg<=252 && mb>=236 && mb<=252
                        && (mr-mg)<18 && (mg-mb)<18){
                        /* measure run length */
                        int run=0;
                        for (int yy=y; yy<h-26; yy++){
                            long r2=0,g2=0,b2=0,n2=0;
                            for (int x=60; x<400; x+=5){ int j=(yy*w+x)*4; r2+=rgba[j];g2+=rgba[j+1];b2+=rgba[j+2]; n2++; }
                            int m2r=r2/n2,m2g=g2/n2,m2b=b2/n2;
                            if (m2r>=236&&m2r<=252&&m2g>=236&&m2g<=252&&m2b>=236&&m2b<=252&&(m2r-m2g)<18&&(m2g-m2b)<18) run++;
                            else break;
                        }
                        if (run>=10) band=1;   /* a real line band is ~24px */
                    }
                }
                if (!band){ fprintf(stderr,"[active-line] no highlight band found\n"); bad++; }
                else fprintf(stderr,"[active-line] ok (highlight band rendered)\n");
            }
            free(rgba);
            al->destroy(al);
        }
    }

    /* UI-42: Save-As re-points the editor and persists (Ctrl+Shift+S path). */
    {
        WuView *sa = wuos_editor_create(NULL);
        if (!sa){ fprintf(stderr,"[save-as] create FAILED\n"); bad++; }
        else {
            const char *p = "/tmp/wubuos_saveas_test.txt";
            wuos_write_file(p, "", 0);   /* ensure exists/scope */
            if (!sa->set_path){ fprintf(stderr,"[save-as] set_path NULL\n"); bad++; }
            else {
                sa->set_path(sa, p);
                const char *got = sa->get_path ? sa->get_path(sa) : NULL;
                if (!got || strcmp(got, p)!=0){ fprintf(stderr,"[save-as] path not applied\n"); bad++; }
                else {
                    /* type content + save-as should write the file */
                    sa->on_key(sa, 'H', 1); sa->on_key(sa, 'i', 1);
                    if (sa->save) sa->save(sa);
                    size_t sz=0; char *out = wuos_read_file(p, &sz);
                    if (!out || sz < 2){ fprintf(stderr,"[save-as] file not written (sz=%zu)\n", sz); bad++; }
                    else fprintf(stderr,"[save-as] ok (path=%s, %zu bytes)\n", got, sz);
                    free(out);
                }
            }
            sa->destroy(sa);
        }
    }

    {
        WuView *sv = wuos_editor_create(NULL);
        int good = sv ? wuos_editor_spell(sv, "cat") : -1;
        int badw = sv ? wuos_editor_spell(sv, "zzqfoo") : -1;
        if (good != 1){ fprintf(stderr,"[spell] known word 'cat' not flagged known (=%d)\n", good); bad++; }
        else if (badw != 0){ fprintf(stderr,"[spell] misspelled 'zzqfoo' not flagged (=%d)\n", badw); bad++; }
        else fprintf(stderr,"[spell] ok (cat=known, zzqfoo=misspelled)\n");
        if (sv) sv->destroy(sv);
    }

    /* INT-2 P0: crash-recovery autosave is actually wired into the editor.
     * Simulate a crash (write a snapshot, drop the live lock), then reopen
     * the editor and confirm the recovered text is spliced into the buffer. */
    {
        const char *recp = "/tmp/wubuos_as_rec.txt";
        wuos_write_file(recp, "RECOVER_ME_LINE\n", 16);
        /* build a snapshot via the same engine the editor uses */
        Autosave *a = wubuautosave_create(recp, 0);
        wubumodel_doc *m = wubumodel_doc_create();
        wubumodel_node *sec = wubumodel_node_create(m, WUBUMODEL_SECTION);
        wubumodel_node *para= wubumodel_node_create(m, WUBUMODEL_PARAGRAPH);
        wubumodel_node *run = wubumodel_node_create(m, WUBUMODEL_RUN);
        wubumodel_run_set_text(run, "CRASH_RECOVERED_TEXT");
        wubumodel_node_append(m, para, run);
        wubumodel_node_append(m, sec, para);
        wubuautosave_flush(a, m);
        wubumodel_doc_destroy(m);
        wubuautosave_destroy(a);
        unlink("/tmp/wubuos_as_rec.txt.lock");  /* simulate dead PID */
        /* reopen: editor should detect + recover */
        WuView *rv = wuos_editor_create(recp);
        if (!rv){ fprintf(stderr,"[autosave] reopen FAILED\n"); bad++; }
        else {
            char *t = wuos_editor_text(rv);
            if (!t || !strstr(t, "CRASH_RECOVERED_TEXT")){
                fprintf(stderr,"[autosave] recovery NOT spliced (text='%s')\n", t?t:"(null)");
                bad++;
            } else {
                fprintf(stderr,"[autosave] ok (recovered text present)\n");
            }
            free(t);
            rv->destroy(rv);
        }
        unlink(recp); unlink("/tmp/wubuos_as_rec.txt.asd"); unlink("/tmp/wubuos_as_rec.txt.lock");
    }

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

    /* column/block selection: Ctrl+Alt+C on, Shift+Right/Down extends block */
    {
        WuView *cv2 = wuos_editor_create(NULL);
        if (!cv2){ fprintf(stderr,"[col] create FAILED\n"); bad++; }
        else {
            cv2->on_key(cv2, WUOS_KEY_GOTO, 1);          /* go to line 1 (top) */
            cv2->on_key(cv2, (unsigned char)'1', 1);
            cv2->on_key(cv2, WUOS_KEY_RETURN, 1);
            cv2->on_key(cv2, WUOS_KEY_COLMODE, 1);       /* enter block mode */
            cv2->on_key(cv2, WUOS_KEY_RIGHT, 1);
            cv2->on_key(cv2, WUOS_KEY_RIGHT, 1);
            cv2->on_key(cv2, WUOS_KEY_DOWN, 1);
            cv2->on_key(cv2, WUOS_KEY_DOWN, 1);
            int l0,c0,l1,c1;
            int mode = wuos_editor_col(cv2, &l0,&c0,&l1,&c1);
            if (!mode){ fprintf(stderr,"[col] mode not on\n"); bad++; }
            else if (l1<=l0 || c1<=c0){ fprintf(stderr,"[col] block not extended l%d-%d c%d-%d\n",l0,l1,c0,c1); bad++; }
            else fprintf(stderr,"[col] ok (block L%d-%d C%d-%d)\n", l0,l1,c0,c1);
            cv2->destroy(cv2);
        }
    }

    /* macro record/play: Ctrl+Shift+R record "Hi<ret>", play into new doc */
    {
        WuView *m1 = wuos_editor_create(NULL);
        if (!m1){ fprintf(stderr,"[macro] create FAILED\n"); bad++; }
        else {
            m1->on_key(m1, WUOS_KEY_REC, 1);              /* start recording */
            if (!wuos_editor_macro(m1, NULL)){ fprintf(stderr,"[macro] rec flag off\n"); bad++; }
            m1->on_key(m1, (unsigned char)'H', 1);
            m1->on_key(m1, (unsigned char)'i', 1);
            m1->on_key(m1, WUOS_KEY_RETURN, 1);
            m1->on_key(m1, WUOS_KEY_REC, 1);              /* stop */
            int ops=0; wuos_editor_macro(m1, &ops);
            if (ops != 3){ fprintf(stderr,"[macro] wrong op count %d\n", ops); bad++; }
            else {
                WuView *m2 = wuos_editor_create(NULL);
                m2->on_key(m2, WUOS_KEY_PLAY, 1);          /* replay */
                char *t = wuos_editor_text(m2);
                int ok = (strstr(t, "Hi\n") != NULL);
                if (!ok){ fprintf(stderr,"[macro] replay='%s'\n", t); bad++; }
                else fprintf(stderr,"[macro] ok (replayed 'Hi\\n', ops=%d)\n", ops);
                free(t); m2->destroy(m2);
            }
            m1->destroy(m1);
        }
    }

    /* auto-completion: type 'pri', Ctrl+Space, Tab -> 'printf' */
    {
        WuView *av = wuos_editor_create(NULL);
        if (!av){ fprintf(stderr,"[ac] create FAILED\n"); bad++; }
        else {
            av->on_key(av, (unsigned char)'p', 1);
            av->on_key(av, (unsigned char)'r', 1);
            av->on_key(av, (unsigned char)'i', 1);
            av->on_key(av, WUOS_KEY_AC, 1);          /* open popup */
            int n=0, sel=0; int opened = wuos_editor_ac(av, &n, &sel);
            if (!opened || n==0){ fprintf(stderr,"[ac] popup not opened (n=%d)\n", n); bad++; }
            else {
                char *before = wuos_editor_text(av);
                int has_pri = (strstr(before, "pri") != NULL);
                free(before);
                av->on_key(av, WUOS_KEY_TAB, 1);      /* accept top candidate */
                char *after = wuos_editor_text(av);
                int has_printf = (strstr(after, "printf") != NULL);
                if (!has_pri || !has_printf){ fprintf(stderr,"[ac] fail pri=%d printf=%d\n", has_pri, has_printf); bad++; }
                else fprintf(stderr,"[ac] ok (popup n=%d, accepted 'printf')\n", n);
                free(after);
            }
            av->destroy(av);
        }
    }

    /* session save/restore: save 2 docs, restore on new editor */
    {
        char sp[256]; sprintf(sp, "/tmp/wuos_sess_%d.txt", (int)getpid());
        setenv("WUBUOS_SESSION", sp, 1);
        WuView *sv = wuos_editor_create(NULL);
        if (!sv){ fprintf(stderr,"[sess] create FAILED\n"); bad++; }
        else {
            sv->on_key(sv, WUOS_KEY_NEWDOC, 1);     /* 2 docs (seed + new) */
            sv->on_key(sv, WUOS_KEY_SESSION, 1);     /* save session */
            size_t saved = wuos_editor_doc_count(sv);
            setenv("WUBUOS_RESTORE", "1", 1);
            WuView *sv2 = wuos_editor_create(NULL);  /* restore */
            size_t restored = wuos_editor_doc_count(sv2);
            unsetenv("WUBUOS_RESTORE");
            if (restored < saved || saved < 2){ fprintf(stderr,"[sess] save=%zu restore=%zu\n", saved, restored); bad++; }
            else fprintf(stderr,"[sess] ok (saved %zu, restored %zu)\n", saved, restored);
            sv2->destroy(sv2);
            sv->destroy(sv);
            remove(sp);
        }
        unsetenv("WUBUOS_SESSION");
    }

    /* code folding: Ctrl+Shift+F folds the block at the cursor line */
    {
        WuView *fv = wuos_editor_create(NULL);
        if (!fv){ fprintf(stderr,"[fold] create FAILED\n"); bad++; }
        else {
            fv->on_key(fv, WUOS_KEY_GOTO, 1);       /* go to line */
            fv->on_key(fv, (unsigned char)'4', 1);
            fv->on_key(fv, WUOS_KEY_RETURN, 1);
            fv->on_key(fv, WUOS_KEY_FOLD, 1);      /* fold main() block (cursor inside it) */
            int fc=0; int folded = wuos_editor_fold(fv, &fc);
            if (!folded || fc < 1){ fprintf(stderr,"[fold] no lines folded (count=%d)\n", fc); bad++; }
            else fprintf(stderr,"[fold] ok (%d lines collapsed)\n", fc);
            /* function list: Ctrl+Shift+L shows panel + symbols */
            fv->on_key(fv, WUOS_KEY_FUNCLIST, 1);
            int ns=0; int on = wuos_editor_sym(fv, &ns);
            if (!on || ns < 1){ fprintf(stderr,"[funclist] panel=%d syms=%d\n", on, ns); bad++; }
            else fprintf(stderr,"[funclist] ok (panel on, %d symbols)\n", ns);
            fv->destroy(fv);
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

    /* ---- CELL interactive: navigate + edit a cell, engine recomputes ---- */
    {
        WuView *cv = wuos_cell_create(NULL);
        if (!cv){ fprintf(stderr,"[cell] create FAILED\n"); bad++; }
        else {
            bad += render_check(cv, "cell");
            /* Navigator sidebar: active cell + row values must be present. */
            if (cv->sidebar){
                char *sb = cv->sidebar(cv);
                if (!sb || !strstr(sb, "Active:")){ fprintf(stderr,"[cell] sidebar missing active cell\n"); bad++; }
                else fprintf(stderr,"[cell] sidebar ok (active cell present)\n");
                free(sb);
            }
            /* active cell starts at A1 (1,1) showing 10 */
            int c=0,r=0; wuos_cell_active(cv,&c,&r);
            char v[64]; wuos_cell_value(cv,v,sizeof v);
            if (c!=1||r!=1){ fprintf(stderr,"[cell] active %d,%d want 1,1\n",c,r); bad++; }
            else if (atoi(v)!=10){ fprintf(stderr,"[cell] A1='%s' want 10\n",v); bad++; }
            /* move to E1 (the SUM formula cell) */
            cv->on_key(cv, WUOS_KEY_RIGHT, 1); /* B1 */
            cv->on_key(cv, WUOS_KEY_RIGHT, 1); /* C1 */
            cv->on_key(cv, WUOS_KEY_RIGHT, 1); /* D1 */
            cv->on_key(cv, WUOS_KEY_RIGHT, 1); /* E1 */
            wuos_cell_active(cv,&c,&r);
            wuos_cell_value(cv,v,sizeof v);
            if (c!=5||r!=1){ fprintf(stderr,"[cell] active %d,%d want 5,1\n",c,r); bad++; }
            else if (atoi(v)!=79){ fprintf(stderr,"[cell] E1(SUM)='%s' want 79\n",v); bad++; }
            else fprintf(stderr,"[cell] ok (nav + SUM=%s)\n", v);
            /* live edit: move DOWN to row 2, type a formula, Enter -> engine stores it */
            cv->on_key(cv, WUOS_KEY_DOWN, 1);  /* row 2, same column (E2) */
            wuos_cell_active(cv,&c,&r);
            if (r!=2){ fprintf(stderr,"[cell] active %d,%d want col5,row2\n",c,r); bad++; }
            else {
                cv->on_key(cv, '=', 1);            /* start editing */
                if (!wuos_cell_editing(cv)){ fprintf(stderr,"[cell] not editing after '='\n"); bad++; }
                for (const char *p="A1+A1"; *p; p++) cv->on_key(cv,(unsigned char)*p,1);
                cv->on_key(cv, WUOS_KEY_RETURN, 1);
                if (wuos_cell_editing(cv)){ fprintf(stderr,"[cell] still editing after Enter\n"); bad++; }
                char f[64]; wuos_cell_formula(cv,f,sizeof f);
                if (strcmp(f,"A1+A1")!=0){ fprintf(stderr,"[cell] formula stored='%s' want 'A1+A1'\n",f); bad++; }
                else fprintf(stderr,"[cell] ok (live edit at %c%d formula='%s')\n", 'A'+c-1, r, f);
            }
            cv->destroy(cv);

            /* ---- CELL round-trip: save the edited book to CSV and reload it
             * to confirm the spreadsheet can persist (was: no save hook). */
            {
                WuView *cv2 = wuos_cell_create("/tmp/wubuos_cell_rt.csv");
                if (!cv2){ fprintf(stderr,"[cell rt] create FAILED\n"); bad++; }
                else {
                    /* EDIT A1 then save+reload: set_cell must REPLACE so the
                     * edited value is what comes back (round-trip correctness). */
                    cv2->on_key(cv2,'h',1);            /* start editing */
                    for (const char*p="i";*p;p++) cv2->on_key(cv2,(unsigned char)*p,1);
                    cv2->on_key(cv2,WUOS_KEY_RETURN,1);/* commit A1="hi" */
                    if (cv2->save) cv2->save(cv2);
                    else { fprintf(stderr,"[cell rt] NO save hook\n"); bad++; }
                    cv2->destroy(cv2);
                    /* reload and confirm "hi" survived (edit replaced A1) */
                    wubucell_book *rb=NULL;
                    if (wubucell_read_csv("/tmp/wubuos_cell_rt.csv", ',', &rb)==0 && rb){
                        wubucell_ckind k; const char *txt=NULL; double n=0,c=0;
                        if (wubucell_get(rb,1,1,1,&k,&txt,&n,&c)==0 && k==WUBUCELL_STR && txt && strstr(txt,"hi"))
                            fprintf(stderr,"[cell rt] CSV edit round-trip OK (A1='%s')\n",txt);
                        else { fprintf(stderr,"[cell rt] edit A1 lost\n"); bad++; }
                        wubucell_free(rb);
                    } else { fprintf(stderr,"[cell rt] reload FAILED\n"); bad++; }
                }
            }
        }
    }

    /* ---- OCR interactive: real recognized text + selection navigation ---- */
    {
        WuView *ov = wuos_ocr_create(NULL);
        if (!ov){ fprintf(stderr,"[ocr] create FAILED\n"); bad++; }
        else {
            bad += render_check(ov, "ocr");
            int n = wuos_ocr_blocks(ov);
            if (n < 1){ fprintf(stderr,"[ocr] no blocks detected\n"); bad++; }
            else {
                char *txt = wuos_ocr_text(ov);
                /* The sample is synthesized from DejaVuSans and recognized by
                 * the DejaVu-backed fontbank; expect non-empty real text. */
                if (!txt || !*txt){
                    fprintf(stderr,"[ocr] no recognized text (fontbank missing?)\n"); bad++;
                } else {
                    fprintf(stderr,"[ocr] ok (%d blocks; recognized '%s')\n", n, txt);
                }
                free(txt);
                /* navigation must be safe + change selection without crashing */
                ov->on_key(ov, WUOS_KEY_DOWN, 1);
                ov->on_key(ov, WUOS_KEY_UP, 1);
                ov->on_key(ov, WUOS_KEY_RETURN, 1);
                char *sel = wuos_ocr_selected(ov);
                fprintf(stderr,"[ocr] selected '%s'\n", sel?sel:"(empty)");
                free(sel);
            }
            ov->destroy(ov);
        }
    }

    /* ---- DOC interactive: office-format open + find-in-doc ---- */
    {
        /* synthesize a tiny docx via the facade's create path is heavy; instead
         * verify find works on the markdown sample and on a loaded text file. */
        WuView *dv = wuos_doc_create(NULL);
        if (!dv){ fprintf(stderr,"[doc-i] create FAILED\n"); bad++; }
        else {
            bad += render_check(dv, "doc");
            int rendered = wuos_doc_is_rendered(dv);
            if (!rendered){ fprintf(stderr,"[doc-i] sample not rendered\n"); bad++; }
            else fprintf(stderr,"[doc-i] ok (sample rendered page)\n");
            dv->destroy(dv);
        }
        /* Document view + DOC-54 TOC + UXA-41 high-contrast: render the
         * sample, expect a TOC (headings present) and a toggleable HC flag. */
        {
            WuView *dv = wuos_doc_create(NULL);
            if (!dv){ fprintf(stderr,"[doc-toc] create FAILED\n"); bad++; }
            else {
                bad += render_check(dv, "doc-toc");
                int tc = wuos_doc_toc_count(dv);
                if (tc < 0){ fprintf(stderr,"[doc-toc] no TOC\n"); bad++; }
                else fprintf(stderr,"[doc-toc] ok (toc=%d entries)\n", tc);
                int hc0 = wuos_doc_high_contrast(dv);
                WubuSettings *sh = wubusettings_shared();
                if (sh) wubusettings_set_high_contrast(sh, !hc0);
                int hc1 = wuos_doc_high_contrast(dv);
                if (hc1 == hc0){ fprintf(stderr,"[doc-hc] toggle no-op\n"); bad++; }
                else fprintf(stderr,"[doc-hc] ok (toggled %d->%d)\n", hc0, hc1);
                if (sh) wubusettings_set_high_contrast(sh, hc0);
                dv->destroy(dv);
            }
        }
        /* real text doc: write one, open, find a known token */
        char dp[256]; sprintf(dp,"/tmp/wuos_doc_%d.txt",(int)getpid());
        wuos_write_file(dp, "alpha bravo charlie delta\nsecond line here\n", 41);
        WuView *dv2 = wuos_doc_create(dp);
        if (!dv2){ fprintf(stderr,"[doc-i] open FAILED\n"); bad++; }
        else {
            int has_txt = wuos_doc_has_text(dv2);
            int hit = wuos_doc_find(dv2, "bravo");
            if (!has_txt){ fprintf(stderr,"[doc-i] no text model\n"); bad++; }
            else if (!hit){ fprintf(stderr,"[doc-i] find 'bravo' failed\n"); bad++; }
            else fprintf(stderr,"[doc-i] ok (text opened, find 'bravo' hit)\n");
            dv2->destroy(dv2);
        }
        remove(dp);
    }


    /* plugin ABI: load sample .so via explicit path, exec, verify string */
    {
        char sop[512];
        snprintf(sop, sizeof sop, "%s/plugins/sample_plugin.so",
                 getenv("WUBUOS_PLUGIN_DIR") ? getenv("WUBUOS_PLUGIN_DIR")
                                             : "/tmp");
        if (wuos_plugin_load_path(sop) != 0){
            fprintf(stderr, "[plugin] load %s FAILED (build step missing?)\n", sop);
            bad++;
        } else {
            int n = wuos_plugin_count();
            if (n != 1){ fprintf(stderr, "[plugin] count=%d want 1\n", n); bad++; }
            else if (strcmp(wuos_plugin_name(0), "hello") != 0){
                fprintf(stderr, "[plugin] name='%s' want 'hello'\n", wuos_plugin_name(0)); bad++;
            } else {
                char *r = wuos_plugin_run(0, NULL);
                int ok = r && strstr(r, "hello from hello v1.0.0 (host-ok)");
                if (!ok){ fprintf(stderr, "[plugin] exec='%s' WRONG\n", r?r:"(null)"); bad++; }
                else fprintf(stderr, "[plugin] ok (loaded '%s', exec ok)\n", wuos_plugin_name(0));
                free(r);
            }
        }
    }

    /* cleanup temp files */
    remove(mdp); remove(codep);

    wuos_font_quit();
    fprintf(stderr,"done bad=%d\n", bad);
    return bad?1:0;
}
