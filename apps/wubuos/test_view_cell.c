/* test_view_cell.c -- interactive spreadsheet checks: navigation, editing,
 * CSV/XLSX round-trips, referenced-cell highlighting. Split from test_view.c. */
#include "wuos.h"
#include "settings.h"
#include "toc.h"
#include "palette.h"
#include "model.h"
#include "cell.h"
#include "../../apps/wubucell/cell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int render_check(WuView *v, const char *name);

int testview_cell(void){
    int bad = 0;

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
            /* ---- CELL column widths: model set/get + drag-resize + click
             * geometry must all agree (columns are no longer hardcoded). */
            {
                wubucell_col_width_set(wubucell_view_book(cv), 1, 2, 20.0);  /* col B = 160px */
                if (wubucell_col_width_get(wubucell_view_book(cv),1,2) != 20.0){
                    fprintf(stderr,"[cell] col width set/get FAILED\\n"); bad++;
                } else fprintf(stderr,"[cell] ok (col width model round-trip)\\n");
                /* drag on B's header divider: B spans [128,288); divider at ~x=290 */
                int claimed = cv->drag_start(cv, 289, wuos_font_height()+4);
                if (!claimed){ fprintf(stderr,"[cell] resize drag not claimed at divider\\n"); bad++; }
                else {
                    /* B's new right edge at x=288 => B width = 288-40-88(A) = 160px = 20 units */
                    cv->drag_move(cv, 288, 20);
                    cv->drag_end(cv, 288, 20);
                    double wd = wubucell_col_width_get(wubucell_view_book(cv),1,2);
                    if (wd < 19.0 || wd > 21.0){ fprintf(stderr,"[cell] drag width %.2f want ~20\\n", wd); bad++; }
                    else fprintf(stderr,"[cell] ok (drag resize col B -> %.0f units)\\n", wd);
                }
                /* click inside the widened B column selects B; click in C stays right of it */
                cv->on_click(cv, cellv_test_col_x(cv,3) + 8, 100);
                int cc=0,rr=0; wuos_cell_active(cv,&cc,&rr);
                if (cc!=3){ fprintf(stderr,"[cell] click after widen: col %d want 3\\n",cc); bad++; }
                else fprintf(stderr,"[cell] ok (click geometry follows variable widths)\\n");
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

            /* ---- CELL XLSX round-trip: assemble (write .xlsx) then read back
             * and verify data + formula survive (Excel interchange format). */
            {
                wubucell_book *b = wubucell_create();
                int s = wubucell_sheet(b, "Sheet1");
                wubucell_cell_n(b, s, 1, 1, 10);
                wubucell_cell_n(b, s, 2, 1, 24);
                wubucell_cell_s(b, s, 1, 2, "Total");
                wubucell_cell_f(b, s, 3, 1, "A1+B1", 34);
                const char *xlsx = "/tmp/wubuos_cell_rt.xlsx";
                if (wubucell_assemble(b, xlsx) != 0){ fprintf(stderr,"[cell xlsx] assemble failed\n"); bad++; }
                wubucell_free(b);
                wubucell_book *rb = NULL;
                if (wubucell_read(xlsx, &rb) != 0 || !rb){ fprintf(stderr,"[cell xlsx] read failed\n"); bad++; }
                else {
                    wubucell_ckind k; const char *txt=NULL; double n=0,c=0;
                    int ok = (wubucell_get(rb,1,1,1,&k,&txt,&n,&c)==0 && k==WUBUCELL_NUM && n==10.0);
                    if (!ok){ fprintf(stderr,"[cell xlsx] A1 lost\n"); bad++; }
                    ok = ok && (wubucell_get(rb,1,3,1,&k,&txt,&n,&c)==0 && k==WUBUCELL_FORM && txt && strstr(txt,"A1+B1"));
                    if (!ok){ fprintf(stderr,"[cell xlsx] formula lost\n"); bad++; }
                    else fprintf(stderr,"[cell xlsx] round-trip OK (A1=10, formula='%s')\n", txt);
                    wubucell_free(rb);
                }
            }
        }

        /* ---- CELL referenced-cell highlight: formula 'A1+B2' must yield refs
         * (A1) and (B2) for the active cell. Guard against the Excel parity
         * feature regressing (research: colored boxes on referenced cells). */
        {
            wubucell_book *b = wubucell_create();
            int s = wubucell_sheet(b, "Sheet1");
            wubucell_cell_f(b, s, 3, 4, "A1+B2", 3);   /* C4 = A1+B2 */
            wubucell_cell_f(b, s, 1, 1, "1+1", 2);
            wubucell_cell_f(b, s, 2, 2, "2+2", 4);
            int rc[8], rr[8];
            int n = wuos_cell_test_refs(b, 3, 4, rc, rr, 8);
            int ok = (n == 2);
            if (ok){
                /* expect (1,1) and (2,2) in some order */
                int have11=0, have22=0;
                for (int i=0;i<n;i++){
                    if (rc[i]==1 && rr[i]==1) have11=1;
                    if (rc[i]==2 && rr[i]==2) have22=1;
                }
                ok = have11 && have22;
            }
            if (ok) fprintf(stderr,"[cell ref] highlight refs OK (A1+B2)\n");
            else { fprintf(stderr,"[cell ref] FAILED (n=%d)\n", n); bad++; }
            /* a non-formula active cell yields no refs */
            int n0 = wuos_cell_test_refs(b, 1, 1, rc, rr, 8);
            if (n0 != 0){ fprintf(stderr,"[cell ref] non-formula gave %d refs\n", n0); bad++; }
            else fprintf(stderr,"[cell ref] non-formula no-refs OK\n");
            wubucell_free(b);
        }
    }
    return bad;
}
