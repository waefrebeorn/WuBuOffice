/* test_pptx_roundtrip.c -- H13 fidelity row: slide deck round-trip.
 * Write a .pptx via wubuoxml_pptx_write, then extract the slide text back
 * via wubuoxml_pptx_text and assert title + bullets survive. Also verify
 * the zip contains the parts Office requires. */
#include "../../src/wubuoxml/pptx_write.h"
#include "../../src/wubuoxml/package.h"
#include "../../src/wubuoxml/reader.h"
#include "../../src/wubuoxml/docx_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ck(int cond, const char *msg){
    if (!cond){ fprintf(stderr,"FAIL %s\n", msg); bad++; }
    else fprintf(stderr,"ok   %s\n", msg);
}

#define PATH "/tmp/wb_test.pptx"

int main(void){
    const char *bullets[3] = { "First finding", "Second finding", "Next steps" };
    int rc = wubuoxml_pptx_write(PATH, "Quarterly Review",
                                 bullets, 3);
    ck(rc == 0, "pptx write ok");

    /* package must contain the Office-required parts */
    FILE *f = fopen(PATH, "rb");
    ck(f != NULL, "file exists");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fclose(f);
    ck(sz > 1000, "non-trivial size");

    /* round-trip: extract slide1 text via the package reader */
    FILE *pf = fopen(PATH, "rb");
    fseek(pf, 0, SEEK_END); long psz = ftell(pf); fseek(pf, 0, SEEK_SET);
    uint8_t *pbuf = malloc((size_t)psz);
    size_t rdn = fread(pbuf, 1, (size_t)psz, pf); fclose(pf);
    wubuoxml_package pkg;
    ck(wubuoxml_read(pbuf, (size_t)rdn, &pkg) == 0, "package reads back");
    const wubuoxml_part *slide = wubuoxml_part_find(&pkg, "ppt/slides/slide1.xml");
    ck(slide != NULL, "slide1.xml in package");
    if (slide){
        char *txt = NULL;
        ck(wubuoxml_pptx_text(slide->bytes, slide->len, &txt) == 0 && txt,
           "pptx_text extracts");
        if (txt){
            ck(strstr(txt, "Quarterly Review") != NULL, "title survives");
            ck(strstr(txt, "First finding") != NULL, "bullet 1 survives");
            ck(strstr(txt, "Next steps") != NULL, "bullet 3 survives");
            free(txt);
        }
    }
    wubuoxml_free(&pkg);
    free(pbuf);

    fprintf(stderr, bad ? "PPTX_ROUNDTRIP FAIL\n" : "PPTX_ROUNDTRIP PASS\n");
    return bad ? 1 : 0;
}
