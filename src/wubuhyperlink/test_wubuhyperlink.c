#include "wubuhyperlink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubuhyperlink *h = wubuhyperlink_create();
    CK(h != NULL, "create");

    CK(wubuhyperlink_set(h, 1001, "https://example.com", "Example", "sec2") == 0, "set link");
    CK(wubuhyperlink_set(h, 1002, "#heading3", NULL, NULL) == 0, "set anchor link");
    CK(wubuhyperlink_count(h) == 2, "count 2");

    const wubuhyperlink_entry *e = wubuhyperlink_get(h, 1001);
    CK(e && strcmp(e->target, "https://example.com") == 0 && strcmp(e->text, "Example") == 0
       && strcmp(e->anchor, "sec2") == 0, "get link fields");
    CK(wubuhyperlink_get(h, 1002)->text == NULL, "null text ok");
    CK(wubuhyperlink_get(h, 999) == NULL, "absent link");

    /* overwrite */
    CK(wubuhyperlink_set(h, 1001, "https://new.org", NULL, NULL) == 0, "overwrite");
    CK(strcmp(wubuhyperlink_get(h, 1001)->target, "https://new.org") == 0, "overwritten target");
    CK(wubuhyperlink_get(h, 1001)->anchor == NULL, "overwritten anchor cleared");
    CK(wubuhyperlink_count(h) == 2, "count still 2 after overwrite");

    CK(wubuhyperlink_remove(h, 1002) == 1, "remove");
    CK(wubuhyperlink_count(h) == 1, "count 1 after remove");
    CK(wubuhyperlink_remove(h, 1002) == 0, "double remove no-op");

    wubuhyperlink_destroy(h);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuhyperlink (node-id side table, set/get/overwrite/remove)\n");
    return 0;
}
