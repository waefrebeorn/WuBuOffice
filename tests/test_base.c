/* test_base.c -- unit tests for the shared wububase utilities. */
#include "wububase.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){ fprintf(stderr,"FAIL: %s\n", msg); failures++; } } while(0)

static void test_utf8(void){
    /* decode: byte count vs codepoint count */
    const char *ascii = "cafe";
    CHECK(wububase_utf8_len(ascii) == 4, "ascii len");
    const char *acute = "caf\xc3\xa9";           /* cafe + e-acute */
    CHECK(wububase_utf8_len(acute) == 4, "accented len (codepoints, not bytes)");
    const char *em = "\xe2\x80\x94";             /* em dash */
    CHECK(wububase_utf8_len(em) == 1, "em dash is 1 codepoint");
    const char *nyan = "ny\xc3\xb1";            /* n y n-tilde */
    CHECK(wububase_utf8_len(nyan) == 3, "n-tilde len");

    /* decode individual codepoints */
    uint32_t cp; const char *p = acute;
    int k = wububase_utf8_decode(p, &cp); CHECK(k==1 && cp=='c', "decode 'c'");
    p += k; k = wububase_utf8_decode(p, &cp); CHECK(k==1 && cp=='a', "decode 'a'");
    p += k; k = wububase_utf8_decode(p, &cp); CHECK(k==1 && cp=='f', "decode 'f'");
    p += k; k = wububase_utf8_decode(p, &cp); CHECK(k==2 && cp==0xE9, "decode e-acute as one cp");

    /* encode round-trip */
    char out[8];
    int n = wububase_utf8_encode(0xE9, out);
    CHECK(n==2 && out[0]==(char)0xC3 && out[1]==(char)0xA9, "encode e-acute");
    n = wububase_utf8_encode(0x2014, out); /* em dash */
    CHECK(n==3 && out[0]==(char)0xE2 && out[1]==(char)0x80 && out[2]==(char)0x94, "encode em dash");
}

static void test_buf(void){
    Buf b; buf_init(&b);
    CHECK(buf_len(&b) == 0, "empty len");
    buf_add(&b, "hello");
    buf_add(&b, " ");
    buf_printf(&b, "%d", 42);
    CHECK(strcmp(buf_str(&b), "hello 42") == 0, "buf append + printf");
    CHECK(buf_len(&b) == 8, "buf len after appends");
    /* overflow the 256 initial cap to force realloc path */
    for (int i = 0; i < 1000; i++) buf_printf(&b, "x");
    CHECK(buf_len(&b) == 1008, "buf grows past initial capacity");
    buf_free(&b);

    Buf e; buf_init(&e);
    buf_printf(&e, "<a href=\"x&y\">%s</a>", "a<b>c");
    CHECK(strcmp(buf_str(&e), "<a href=\"x&y\">a<b>c</a>") == 0, "buf raw does NOT escape (escape is explicit)");
    buf_free(&e);
}

static void test_xml_escape(void){
    Buf b; buf_init(&b);
    wububase_xml_escape(&b, "a<b>c\"d&e");
    CHECK(strcmp(buf_str(&b), "a&lt;b&gt;c&quot;d&amp;e") == 0, "xml escape all four entities");
    buf_free(&b);

    Buf u; buf_init(&u);
    wububase_xml_escape(&u, "caf\xc3\xa9"); /* accented text must pass through untouched */
    CHECK(strcmp(buf_str(&u), "caf\xc3\xa9") == 0, "xml escape preserves UTF-8 bytes");
    buf_free(&u);
}

int main(void){
    test_utf8();
    test_buf();
    test_xml_escape();
    if (failures == 0){ printf("base: all tests passed\n"); return 0; }
    printf("base: %d FAILURES\n", failures);
    return 1;
}
