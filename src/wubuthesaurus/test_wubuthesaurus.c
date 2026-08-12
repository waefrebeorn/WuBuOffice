#include "wubuthesaurus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubuthesaurus *t = wubuthesaurus_create();
    const char *happy[] = {"glad","joyful","cheerful","content",NULL};
    const char *big[] = {"large","huge","enormous","vast",NULL};
    CK(wubuthesaurus_add(t,"happy",happy)==0 && wubuthesaurus_add(t,"Big",big)==0,"add entries");
    CK(wubuthesaurus_count(t)==2,"count 2");

    const char **s = wubuthesaurus_lookup(t,"happy");
    CK(s && strcmp(s[0],"glad")==0 && strcmp(s[1],"joyful")==0 && s[3]!=NULL && s[4]==NULL,"lookup happy");
    /* case-insensitive */
    s = wubuthesaurus_lookup(t,"BIG");
    CK(s && strcmp(s[0],"large")==0,"lookup BIG case-insensitive");
    CK(wubuthesaurus_lookup(t,"nope")==NULL,"absent word");

    /* overwrite */
    const char *great[] = {"excellent","superb",NULL};
    CK(wubuthesaurus_add(t,"happy",great)==0,"overwrite happy");
    s = wubuthesaurus_lookup(t,"happy");
    CK(s && s[0]!=NULL && s[1]!=NULL && s[2]==NULL,"overwritten syns");

    wubuthesaurus_destroy(t);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuthesaurus (word->synonyms store, case-insensitive, overwrite)\n");
    return 0;
}
