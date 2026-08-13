#include "wubusmartart.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef struct { char *text; } node;

struct wubusmartart {
    wubusa_layout layout;
    node *nodes;
    size_t n, cap;
};

wubusmartart *wubusmartart_create(void){
    wubusmartart *s = (wubusmartart*)calloc(1,sizeof(wubusmartart));
    if (s) s->layout = WUBU_SA_PROCESS;
    return s;
}
void wubusmartart_destroy(wubusmartart *s){
    if (!s) return;
    for (size_t i=0;i<s->n;i++) free(s->nodes[i].text);
    free(s->nodes); free(s);
}
int wubusmartart_set_layout(wubusmartart *s, wubusa_layout layout){
    if (!s || layout < WUBU_SA_PROCESS || layout > WUBU_SA_LIST) return -1;
    s->layout = layout; return 0;
}
wubusa_layout wubusmartart_layout(const wubusmartart *s){ return s?s->layout:0; }
int wubusmartart_add_node(wubusmartart *s, const char *text){
    if (!s || !text) return -1;
    if (s->n == s->cap){ size_t nc=s->cap?s->cap*2:8; node*nn=(node*)realloc(s->nodes,nc*sizeof(node)); if(!nn) return -1; s->nodes=nn; s->cap=nc; }
    s->nodes[s->n].text = strdup(text);
    if (!s->nodes[s->n].text) return -1;
    s->n++;
    return 0;
}
size_t wubusmartart_count(const wubusmartart *s){ return s?s->n:0; }
const char *wubusmartart_node(const wubusmartart *s, size_t i){ return (s&&i<s->n)?s->nodes[i].text:NULL; }

int wubusmartart_layout_boxes(const wubusmartart *s, float fw, float fh,
                             wubusa_box *out, size_t outn){
    if (!s || !out || fw <= 0 || fh <= 0) return -1;
    if (outn < s->n) return -1;
    size_t n = s->n;
    if (n == 0) return 0;

    switch (s->layout) {
    case WUBU_SA_PROCESS: {
        float bw = fw / (float)n * 0.8f;
        float bh = fh * 0.4f;
        float y = (fh - bh) * 0.5f;
        float gap = fw / (float)n;
        for (size_t i=0;i<n;i++){
            out[i].w = bw; out[i].h = bh;
            out[i].x = gap * (float)i + (gap - bw) * 0.5f;
            out[i].y = y;
        }
        return 0;
    }
    case WUBU_SA_LIST: {
        float bh = fh / (float)n * 0.7f;
        float bw = fw * 0.8f;
        float x = fw * 0.1f;
        float gap = fh / (float)n;
        for (size_t i=0;i<n;i++){
            out[i].w = bw; out[i].h = bh;
            out[i].x = x;
            out[i].y = gap * (float)i + (gap - bh) * 0.5f;
        }
        return 0;
    }
    case WUBU_SA_CYCLE: {
        float cx = fw*0.5f, cy = fh*0.5f;
        float r = (fw < fh ? fw : fh) * 0.35f;
        float bw = r*0.6f, bh = r*0.6f;
        for (size_t i=0;i<n;i++){
            float a = (2.0f*M_PI*(float)i) / (float)n - M_PI/2.0f;
            out[i].w = bw; out[i].h = bh;
            out[i].x = cx + cosf(a)*r - bw*0.5f;
            out[i].y = cy + sinf(a)*r - bh*0.5f;
        }
        return 0;
    }
    case WUBU_SA_HIERARCHY: {
        float bw = fw / (float)n * 0.7f;
        float bh = fh * 0.3f;
        /* root = node 0, centered top */
        out[0].w = bw; out[0].h = bh;
        out[0].x = (fw - bw)*0.5f; out[0].y = fh*0.08f;
        /* children fanned in the bottom row */
        float gap = fw / (float)n;
        for (size_t i=1;i<n;i++){
            out[i].w = bw; out[i].h = bh;
            out[i].x = gap*(float)i + (gap-bw)*0.5f;
            out[i].y = fh*0.62f;
        }
        return 0;
    }
    default:
        return -1;
    }
}
