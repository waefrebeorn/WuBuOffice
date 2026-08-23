/* test_pptx_multi.c -- hop 17: multi-slide pptx assembly.
 * A 3-slide deck must produce sldIdLst with 3 entries, three slide parts,
 * and each slide's text must round-trip through wubuoxml_pptx_text. */
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

#define PATH "/tmp/wb_multi.pptx"

int main(void){
    const char *b1[2] = { "intro point", "second point" };
    const char *b2[1] = { "data slide" };
    PptxSlide slides[3] = {
        { "Deck Title",   b1, 2 },
        { "Results",      b2, 1 },
        { "Conclusion",   NULL, 0 },
    };
    ck(wubuoxml_pptx_write_multi(PATH, slides, 3) == 0, "multi-slide write ok");

    /* read the package back */
    FILE *pf = fopen(PATH, "rb");
    ck(pf != NULL, "file exists");
    fseek(pf, 0, SEEK_END); long sz = ftell(pf); fseek(pf, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz);
    size_t rd = fread(buf, 1, (size_t)sz, pf); fclose(pf);
    (void)rd;

    wubuoxml_package pkg;
    ck(wubuoxml_read(buf, (size_t)sz, &pkg) == 0, "package reads");

    /* all three slide parts present */
    for (int i = 1; i <= 3; i++){
        char part[48];
        snprintf(part, sizeof part, "ppt/slides/slide%d.xml", i);
        const wubuoxml_part *sl = wubuoxml_part_find(&pkg, part);
        ck(sl != NULL, part);
        if (!sl) continue;
        char *txt = NULL;
        ck(wubuoxml_pptx_text(sl->bytes, sl->len, &txt) == 0 && txt,
           "text extracts");
        if (txt){
            const char *expect[3] = { "Deck Title", "Results", "Conclusion" };
            ck(strstr(txt, expect[i-1]) != NULL, "title survives");
            free(txt);
        }
    }
    /* presentation.xml lists 3 slides */
    const wubuoxml_part *pres = wubuoxml_part_find(&pkg, "ppt/presentation.xml");
    if (pres){
        uint8_t *tmp = malloc(pres->len + 1);
        memcpy(tmp, pres->bytes, pres->len); tmp[pres->len] = 0;
        ck(strstr((char*)tmp, "<p:sldId id=\"258\"") != NULL,
           "sldIdLst has third entry");
        free(tmp);
    }

    free(buf);
    wubuoxml_free(&pkg);
    fprintf(stderr, bad ? "PPTX_MULTI FAIL\n" : "PPTX_MULTI PASS\n");
    return bad ? 1 : 0;
}
