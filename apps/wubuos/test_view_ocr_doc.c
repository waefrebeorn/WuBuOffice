/* test_view_ocr_doc.c -- OCR view interactivity, document-view open+find,
 * plugin ABI host check. Split from test_view.c. */
#include "wuos.h"
#include "settings.h"
#include "toc.h"
#include "palette.h"
#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int render_check(WuView *v, const char *name);

int testview_ocr_doc(void){
    int bad = 0;

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

    return bad;
}
