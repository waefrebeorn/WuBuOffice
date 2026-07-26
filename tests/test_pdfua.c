/* test_pdfua.c -- unit test for PDF/UA structure tree (#93) + language tag
 * (#46/#94) plumbing through the searchable-PDF writer.
 * Builds a 2x2 blank OcrImage, writes a searchable PDF from a docmodel that
 * carries lang="ru", and checks:
 *   - the PDF has a /StructTreeRoot (PDF/UA requirement),
 *   - /MarkInfo << /Marked true >> is present,
 *   - /Lang reflects the docmodel language ("ru"). */
#include <stdio.h>
#include <string.h>
#include "image.h"
#include "pdfsearch.h"

int main(void){
    /* 2x2 all-white page */
    OcrImage *img = ocr_image_create(2, 2);
    if (!img){ printf("FAIL: ocr_image_create\n"); return 1; }
    for (int y=0;y<2;y++) for (int x=0;x<2;x++) ocr_image_set(img,(size_t)x,(size_t)y,255);

    const char *json =
        "{\"blocks\":[{\"kind\":\"paragraph\",\"text\":\"privet\",\"conf\":90,"
         "\"bbox\":[0,0,2,2],\"lang\":\"ru\"}],\"lang\":\"ru\"}";

    char path[256];
    snprintf(path, sizeof path, "/tmp/test_pdfua_%d.pdf", (int)getpid());
    FILE *o = fopen(path, "wb");
    if (!o){ printf("FAIL: fopen\n"); ocr_image_free(img); return 1; }
    int rc = wubuocr_write_searchable_pdf(img, json, o);
    fclose(o);
    ocr_image_free(img);
    if (rc != 0){ printf("FAIL: write_searchable_pdf rc=%d\n", rc); return 1; }

    FILE *rf = fopen(path, "rb");
    if (!rf){ printf("FAIL: reopen\n"); return 1; }
    fseek(rf,0,SEEK_END); long sz=ftell(rf); fseek(rf,0,SEEK_SET);
    char *buf = malloc(sz? (size_t)sz : 1);
    if (fread(buf,1,(size_t)sz,rf)!=(size_t)sz){ printf("FAIL: read\n"); free(buf); fclose(rf); return 1; }
    fclose(rf);

    int ok = 1;
    if (!strstr(buf, "StructTreeRoot")) { printf("FAIL: no StructTreeRoot\n"); ok=0; }
    if (!strstr(buf, "MarkInfo") || !strstr(buf, "Marked")) { printf("FAIL: no MarkInfo/Marked\n"); ok=0; }
    if (!strstr(buf, "/Lang (ru)")) { printf("FAIL: /Lang not 'ru'\n"); ok=0; }
    free(buf);
    if (!ok) return 1;

    printf("PASS: test_pdfua (StructTreeRoot + MarkInfo + /Lang=ru)\n");
    return 0;
}
