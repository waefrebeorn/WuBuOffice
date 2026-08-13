#include "wubuanimation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void) {
    wubuanimation *a = wubuanimation_create();
    CK(wubuanimation_add(a,"title",WUBU_AN_FADE,1.0,0.0,0) == 0, "add fade");
    CK(wubuanimation_add(a,"pic",WUBU_AN_FLYIN,1.5,0.5,3) == 0, "add flyin");
    CK(wubuanimation_count(a) == 2, "2 keys");

    const wubuan_key *k = wubuanimation_get(a,1);
    CK(k && strcmp(k->target,"pic")==0 && k->type==WUBU_AN_FLYIN && k->repeat==3, "key values");
    CK(wubuanimation_get(a,9) == NULL, "out of range");
    CK(wubuanimation_add(a,"x",WUBU_AN_SPIN,-1,0,0) == -1, "reject neg dur");

    /* REAL engine: progress must interpolate over time. */
    /* before delay => 0 */
    CK(wubuanimation_progress(a, "pic", 0.2) == 0.0, "flyin before delay = 0");
    /* at start of active window (t=0.5) => 0 */
    CK(wubuanimation_progress(a, "pic", 0.5) == 0.0, "flyin at delay = 0");
    /* mid active window => in (0,1), eased (flyin is ease-out cubic so >0.5 at mid) */
    double pm = wubuanimation_progress(a, "pic", 1.25);
    CK(pm > 0.0 && pm < 1.0, "flyin midpoint in (0,1)");
    CK(pm > 0.5, "flyin ease-out > linear at mid");
    /* after one cycle but repeats remaining => cycles back toward 0 */
    double pe = wubuanimation_progress(a, "pic", 2.0);
    CK(pe >= 0.0 && pe <= 1.0, "flyin repeats within range at t=2.0");
    /* a non-repeating clone stays at 1 after completion */
    wubuanimation *b = wubuanimation_create();
    wubuanimation_add(b, "pic", WUBU_AN_FLYIN, 1.0, 0.0, 0);
    CK(wubuanimation_progress(b, "pic", 10.0) == 1.0, "non-repeat flyin after dur = 1 (stays)");
    wubuanimation_destroy(b);
    /* title fade at mid => 0.5 */
    double pf = wubuanimation_progress(a, "title", 0.5);
    CK(pf > 0.0 && pf < 1.0, "fade midpoint in (0,1)");
    /* unknown target => -1 */
    CK(wubuanimation_progress(a, "nope", 0.5) < 0.0, "unknown target = -1");

    wubuanimation_destroy(a);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubuanimation (keyframe engine: eased per-object timeline progress)\n");
    return 0;
}
