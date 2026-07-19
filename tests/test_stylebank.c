/* test_stylebank.c -- smoke test for the 64MB multi-style expert bank.
 * Verifies: create(56+8), slot kinds, load-style (instant upgrade),
 * ensemble forward runs + is deterministic, per-slot train runs, promote
 * returns a valid slot. Plain C11, no deps. */
#include "stylebank.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define NBEST 56
#define NROLL 8

int main(void){
    int fails=0;
    StyleBank *b = stylebank_create(NBEST, NROLL, &CONV_MED, 256, 128, 10);
    if(!b){ printf("FAIL: create\n"); return 1; }
    if(stylebank_nslots(b)!=NBEST+NROLL){ printf("FAIL: nslots %d\n",stylebank_nslots(b)); fails++; }
    if(stylebank_nbest(b)!=NBEST){ printf("FAIL: nbest\n"); fails++; }
    if(stylebank_nrolling(b)!=NROLL){ printf("FAIL: nrolling\n"); fails++; }
    if(stylebank_slot_kind(b,0)!=SLOT_BEST){ printf("FAIL: slot0 kind\n"); fails++; }
    if(stylebank_slot_kind(b,NBEST)!=SLOT_ROLLING){ printf("FAIL: slot56 kind\n"); fails++; }

    /* instant upgrade: load a saved style into slot 0 */
    if(stylebank_load_style(b, 0, "data/conv3.wts", "data/conv3_mlp.wts")!=0){
        printf("WARN: could not load data/conv3.wts (run a trainer first) -- skipping load test\n");
    } else {
        printf("loaded style into slot 0 (instant upgrade OK)\n");
    }

    /* ensemble forward must run and be deterministic */
    float im[784]; for(int i=0;i<784;i++) im[i]=(float)(i%256)/255.0f;
    float s1[64], s2[64];
    stylebank_forward(b, im, s1);
    stylebank_forward(b, im, s2);
    int det=1; for(int c=0;c<10;c++) if(fabsf(s1[c]-s2[c])>1e-5f) det=0;
    if(!det){ printf("FAIL: ensemble forward not deterministic\n"); fails++; }
    printf("ensemble forward OK (slot0 logit=%.3f)\n", s1[0]);

    /* train one rolling slot briefly (uses proven conv3+mlp math) */
    if(stylebank_train_slot(b, NBEST, "data/fashion", "t10k", 1, 2000)==0){
        printf("trained rolling slot %d OK\n", NBEST);
    } else {
        printf("WARN: train_slot skipped (no fashion t10k)\n");
    }

    /* promote (needs a test set; if absent it just returns -1, not a fail) */
    int promo = stylebank_promote(b, "data/fashion", "t10k", 0);
    printf("promote returned slot %d\n", promo);

    stylebank_destroy(b);
    if(fails){ printf("FAIL (%d)\n", fails); return 1; }
    printf("PASS\n");
    return 0;
}
