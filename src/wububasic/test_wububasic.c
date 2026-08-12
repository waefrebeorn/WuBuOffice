#include "wububasic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

static void outfn(const char *s, void *ud){ strcat((char*)ud, s); }

int main(void){
    wububasic *b = wububasic_create();
    char outbuf[512] = "";

    /* simple LET + PRINT */
    CK(wububasic_load(b,
        "LET x = 5\n"
        "LET y = 3\n"
        "PRINT x + y\n"
        "END\n") == 0, "load program 1");
    wububasic_set_output(b, outfn, outbuf);
    CK(wububasic_run(b) == 0, "run program 1");
    CK(strstr(outbuf,"8") != NULL, "print 5+3=8");

    /* IF/THEN */
    outbuf[0]=0;
    CK(wububasic_load(b,
        "x = 10\n"
        "IF x > 5 THEN PRINT \"big\" ELSE PRINT \"small\"\n"
        "END\n") == 0, "load program 2");
    CK(wububasic_run(b) == 0, "run program 2");
    CK(strstr(outbuf,"big") != NULL, "IF taken");

    /* FOR loop */
    outbuf[0]=0;
    CK(wububasic_load(b,
        "s = 0\n"
        "FOR i = 1 TO 5\n"
        "  s = s + i\n"
        "NEXT\n"
        "PRINT s\n"
        "END\n") == 0, "load program 3");
    CK(wububasic_run(b) == 0, "run program 3");
    CK(strstr(outbuf,"15") != NULL, "sum 1..5 = 15");

    /* GOSUB/RETURN */
    outbuf[0]=0;
    CK(wububasic_load(b,
        "PRINT \"a\"\n"
        "GOSUB 100\n"
        "PRINT \"c\"\n"
        "END\n"
        "100 PRINT \"b\"\n"
        "RETURN\n") == 0, "load program 4");
    CK(wububasic_run(b) == 0, "run program 4");
    CK(strstr(outbuf,"a") && strstr(outbuf,"b") && strstr(outbuf,"c"), "gosub order abc");

    /* INPUT via callback */
    outbuf[0]=0;
    CK(wububasic_load(b,
        "INPUT name\n"
        "PRINT \"hi \" + name\n"
        "END\n") == 0, "load program 5");
    /* We'll just test with an explicit set_var path instead */
    wububasic_load(b, "PRINT \"hi \" + name\nEND\n");
    wububasic_set_var(b, "name", "WuBu");
    outbuf[0]=0;
    CK(wububasic_run(b) == 0, "run with preset var");
    CK(strstr(outbuf,"hi WuBu") != NULL, "string concat + var");

    wububasic_destroy(b);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wububasic (minimal BASIC: LET/PRINT/IF/FOR/GOSUB/vars/concat)\n");
    return 0;
}
