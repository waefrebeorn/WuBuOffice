/* test_xlsx.c -- unit test for docfmt_to_xlsx (Excel OOXML export).
 * Builds a small docmodel JSON (a paragraph + a 2x2 table), serializes to
 * .xlsx, and verifies the result is a valid zip containing the six required
 * OOXML parts. Depends only on wubuocr (docfmt + wubuzip + wubujson). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "docfmt.h"
#include "reader.h"   /* wubuzip from-scratch ZIP reader */

static const char *sample_json =
    "{\"blocks\":["
    "  {\"kind\":\"paragraph\",\"text\":\"Hello OCR\"},"
    "  {\"kind\":\"table\",\"rows\":2,\"cols\":2,\"cells\":[[\"a\",\"b\"],[\"c\",\"d\"]]}"
    "]}";

int main(void){
    char *buf=NULL; size_t len=0;
    if (docfmt_to_xlsx(sample_json, &buf, &len) != 0 || !buf){
        printf("FAIL: docfmt_to_xlsx returned error\n"); return 1;
    }
    if (len < 4 || memcmp(buf, "PK\x03\x04", 4) != 0){
        printf("FAIL: output is not a zip (bad magic)\n"); free(buf); return 1;
    }
    wubuzip_archive ar;
    if (wubuzip_open((const uint8_t*)buf, len, &ar) != 0){
        printf("FAIL: wubuzip_open failed\n"); free(buf); return 1;
    }
    const char *req[] = {
        "[Content_Types].xml", "_rels/.rels", "xl/workbook.xml",
        "xl/_rels/workbook.xml.rels", "xl/worksheets/sheet1.xml",
        "xl/sharedStrings.xml"
    };
    int have[6] = {0,0,0,0,0,0};
    for (int i=0;i<6;i++){
        size_t idx = wubuzip_find(&ar, req[i]);
        if (idx != (size_t)-1){
            uint8_t *d=NULL; size_t dl=0;
            if (wubuzip_extract(&ar, idx, &d, &dl) == 0 && d && dl>0){ have[i]=1; free(d); }
        }
    }
    wubuzip_close(&ar);
    free(buf);
    int missing=0;
    for (int i=0;i<6;i++) if(!have[i]){ printf("FAIL: missing part %s\n", req[i]); missing++; }
    if (missing) return 1;
    printf("OK: xlsx has all 6 OOXML parts\n");
    printf("PASS: test_xlsx\n");
    return 0;
}
