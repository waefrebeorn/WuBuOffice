/* test_settings.c -- wubusettings module check. */
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void){
    int fails = 0;
    WubuSettings *s = wubusettings_create();
    if (!s){ printf("FAIL create\n"); return 1; }

    /* factory defaults */
    if (wubusettings_zoom(s) != 1.0){ printf("FAIL default zoom\n"); fails++; }
    if (!wubusettings_dark(s)){ printf("FAIL default dark\n"); fails++; }
    if (wubusettings_first_run(s) != 1){ printf("FAIL default first_run\n"); fails++; }  /* UI-30 */
    if (wubusettings_autosave_ms(s) != 5000){ printf("FAIL default autosave\n"); fails++; }
    if (strcmp(wubusettings_language(s), "en")){ printf("FAIL default lang\n"); fails++; }

    /* setters + clamping */
    wubusettings_set_zoom(s, 99.0);   /* clamp to 3.0 */
    if (wubusettings_zoom(s) != 3.0){ printf("FAIL zoom clamp hi (%f)\n", wubusettings_zoom(s)); fails++; }
    wubusettings_set_zoom(s, 0.1);    /* clamp to 0.5 */
    if (wubusettings_zoom(s) != 0.5){ printf("FAIL zoom clamp lo\n"); fails++; }
    wubusettings_set_zoom(s, 1.5);
    wubusettings_set_dark(s, 0);
    wubusettings_set_autosave_ms(s, 2000);
    wubusettings_set_language(s, "ar");
    wubusettings_set_font_size(s, 20);
    wubusettings_set_high_contrast(s, 1);
    wubusettings_set_reduced_motion(s, 1);   /* DOC-43 */
    wubusettings_set_ui_scale(s, 1.5);       /* DOC-45 */
    wubusettings_set_first_run(s, 0);        /* UI-30: dismiss */

    /* round-trip through a temp file */
    const char *path = "/tmp/wubuos_settings_test.json";
    if (wubusettings_save(s, path) != 0){ printf("FAIL save\n"); fails++; }
    wubusettings_destroy(s);

    WubuSettings *s2 = wubusettings_create();
    if (wubusettings_load(s2, path) != 0){ printf("FAIL load\n"); fails++; }
    if (wubusettings_zoom(s2) != 1.5){ printf("FAIL reload zoom\n"); fails++; }
    if (wubusettings_dark(s2)){ printf("FAIL reload dark\n"); fails++; }
    if (wubusettings_autosave_ms(s2) != 2000){ printf("FAIL reload autosave\n"); fails++; }
    if (strcmp(wubusettings_language(s2), "ar")){ printf("FAIL reload lang\n"); fails++; }
    if (wubusettings_font_size(s2) != 20){ printf("FAIL reload fontsize\n"); fails++; }
    if (!wubusettings_high_contrast(s2)){ printf("FAIL reload high-contrast\n"); fails++; }
    if (!wubusettings_reduced_motion(s2)){ printf("FAIL reload reduced-motion\n"); fails++; }
    if (wubusettings_ui_scale(s2) != 1.5){ printf("FAIL reload ui-scale\n"); fails++; }
    if (wubusettings_first_run(s2) != 0){ printf("FAIL reload first_run\n"); fails++; }  /* UI-30 */
    wubusettings_destroy(s2);

    /* shared singleton */
    WubuSettings *sh = wubusettings_shared();
    if (!sh){ printf("FAIL shared\n"); fails++; }

    if (fails){ printf("SETTINGS TESTS FAILED (%d)\n", fails); return 1; }
    printf("SETTINGS TESTS PASSED\n");
    return 0;
}
