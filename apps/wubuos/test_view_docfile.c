/* test_view_docfile.c -- markdown -> Document view round-trip: sidebar
 * structure, wubuscript fields, status. Split from test_view.c battery. */
#include "wuos.h"
#include "settings.h"
#include "toc.h"
#include "palette.h"
#include "model.h"
#include "wuos_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int render_check(WuView *v, const char *name);

int testview_docfile(void){
    int bad = 0;
    const char *md = "# Title\n\nHello **world** paragraph.\n\n## Section\n";
    char mdp[256];
    sprintf(mdp, "/tmp/wuos_df_%d.md", (int)getpid());
    wuos_write_file(mdp, md, strlen(md));


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

    remove(mdp);
    return bad;
}
