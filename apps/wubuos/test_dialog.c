/* test_dialog.c -- headless unit test for the opaque Dialog module. */
#include "dialog.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c,msg) do{ if(!(c)){ fprintf(stderr,"[fail] %s\n", msg); fails++; } }while(0)

int main(void){
    Dialog *d = dialog_create();
    CHECK(d != NULL, "create");

    /* not active until opened */
    CHECK(!dialog_active(d), "inactive before open");
    CHECK(!dialog_confirmed(d), "not confirmed before open");

    /* open + type */
    dialog_open(d, "Insert Link", "URL:", "https://");
    CHECK(dialog_active(d), "active after open");
    const char *p = "example.com";
    for (const char *q=p; *q; q++) dialog_key(d, (unsigned char)*q, q);
    CHECK(!strcmp(dialog_text(d), "https://example.com"), "typed text");
    CHECK(dialog_active(d), "still active while typing");

    /* backspace removes a char */
    dialog_key(d, 8, NULL);
    CHECK(!strcmp(dialog_text(d), "https://example.co"), "backspace");

    /* Enter confirms, buffers the text, deactivates */
    int r = dialog_key(d, 13, NULL);
    CHECK(r == 1, "enter returns 1 (confirm)");
    CHECK(!dialog_active(d), "inactive after confirm");
    CHECK(dialog_confirmed(d), "confirmed flag set");
    CHECK(!strcmp(dialog_text(d), "https://example.co"), "text retained after confirm");

    /* cancel path */
    dialog_open(d, "X", "Y:", "default");
    CHECK(dialog_active(d), "active again");
    r = dialog_key(d, 27, NULL);
    CHECK(r == 2, "esc returns 2 (cancel)");
    CHECK(!dialog_active(d), "inactive after cancel");
    CHECK(!dialog_confirmed(d), "not confirmed after cancel");

    /* default value present and editable */
    dialog_open(d, "QR", "Text:", NULL);
    CHECK(!strcmp(dialog_text(d), ""), "empty default");
    dialog_key(d, 'A', "A");
    CHECK(!strcmp(dialog_text(d), "A"), "typed after empty default");

    dialog_destroy(d);
    if (fails){ printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: dialog (open/active, type, backspace, confirm, cancel, default)\n");
    return 0;
}
