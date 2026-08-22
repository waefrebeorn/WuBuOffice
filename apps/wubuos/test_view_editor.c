/* test_view_editor.c -- file-open round-trips (markdown -> Document,
 * code -> Editor), settings view, and the editor interaction battery.
 * Split from test_view.c; returns number of failed checks. */
#include "wuos.h"
#include "settings.h"
#include "toc.h"
#include "palette.h"
#include "model.h"
#include "cell.h"
#include "autosave.h"
#include "wuos_file.h"
#include "wuos_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int render_check(WuView *v, const char *name);   /* from test_view_main.c */

int testview_editor(void){
    int bad = 0;

    /* file open: markdown -> Document, code -> Editor */
    const char *md = "# Title\n\nHello **world** paragraph.\n\n## Section\n\nAnother line.\n";
    const char *code = "int main(){\n  return 0; // done\n}\n";
    char mdp[256], codep[256];
    sprintf(mdp,  "/tmp/wuos_test_%d.md",  (int)getpid());
    sprintf(codep,"/tmp/wuos_test_%d.c",  (int)getpid());
    wuos_write_file(mdp, md, strlen(md));
    wuos_write_file(codep, code, strlen(code));

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
                /* Find a contiguous band of rows (in the text area x>60) whose
                 * brightness is subtly offset from the background (the active-line
                 * tint). Theme-independent: compute the dominant bg first, then
                 * look for a run of ~one line height that deviates from it by a
                 * small, consistent delta. */
                /* dominant bg color in the text area (skip gutter) */
                long br=0,bg_=0,bb=0,bn=0;
                for (int y=40; y<h-26; y+=4)
                    for (int x=200; x<700; x+=4){ int i=(y*w+x)*4; br+=rgba[i];bg_+=rgba[i+1];bb+=rgba[i+2];bn++; }
                int mbr=(int)(br/bn), mbg=(int)(bg_/bn), mbb=(int)(bb/bn);
                int band=0;
                for (int y=40; y<h-26 && !band; y++){
                    long sr=0,sg=0,sb=0,n=0;
                    for (int x=200; x<700; x+=5){ int i=(y*w+x)*4; sr+=rgba[i];sg+=rgba[i+1];sb+=rgba[i+2]; n++; }
                    int mr=sr/n, mg=sg/n, mb=sb/n;
                    /* a tinted row: within ~40 brightness of bg but clearly distinct */
                    int d = abs(mr-mbr)+abs(mg-mbg)+abs(mb-mbb);
                    if (d>=12 && d<=120){
                        int run=0;
                        for (int yy=y; yy<h-26; yy++){
                            long r2=0,g2=0,b2=0,n2=0;
                            for (int x=200; x<700; x+=5){ int j=(yy*w+x)*4; r2+=rgba[j];g2+=rgba[j+1];b2+=rgba[j+2]; n2++; }
                            int m2r=r2/n2,m2g=g2/n2,m2b=b2/n2;
                            int dd=abs(m2r-mbr)+abs(m2g-mbg)+abs(m2b-mbb);
                            if (dd>=12 && dd<=120) run++; else break;
                        }
                        if (run>=6) band=1;   /* a line band is ~18-24px */
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
            /* editor now follows the global shell theme (may be light or dark),
             * so assert the TOGGLE flips state, then force DARK for the render
             * pixel check (independent of the starting theme). */
            int start = wuos_editor_dark(tv);
            tv->on_key(tv, WUOS_KEY_THEME, 1);
            if (wuos_editor_dark(tv) == start){ fprintf(stderr,"[theme] toggle failed\n"); bad++; }
            if (!wuos_editor_dark(tv)) tv->on_key(tv, WUOS_KEY_THEME, 1);  /* ensure dark */
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

    /* cleanup temp files owned by this section */
    remove(mdp); remove(codep);
    return bad;
}
