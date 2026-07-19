/* convnet.c -- ultra-light conv front-end. See convnet.h. C11, no deps.
 *
 * Designed for SINGLE-CORE scalar CPUs (Q6600-class): plain nested
 * loops, float, no SIMD. ~200k MACs per 28x28 image at CONV_LIGHT.
 */
#include "convnet.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

struct ConvNet {
    int inH, inW;
    int K1, S1, P1;
    int K2, S2, P2;
    /* stage1 */
    float *w1;           /* K1 * S1*S1 */
    float *b1;           /* K1 */
    /* stage2 (present iff K2>0) */
    float *w2;           /* K2 * K1*S2*S2 */
    float *b2;           /* K2 */
    /* geometry */
    int c1H, c1W, p1H, p1W;   /* conv1 out / pool1 out */
    int c2H, c2W, p2H, p2W;   /* conv2 out / pool2 out (0 if K2==0) */
    int fdim;              /* feature length */
    /* cached activations (forward) for backward */
    float *c1;           /* c1H*c1W*K1 */
    float *p1;           /* p1H*p1W*K1 */
    float *c2;           /* c2H*c2W*K2 (NULL if K2==0) */
    float *p2;           /* p2H*p2W*K2 (== features if K2>0, else NULL) */
    /* maxpool argmax index caches */
    int   *am1;          /* p1H*p1W*K1 */
    int   *am2;          /* p2H*p2W*K2 (NULL if K2==0) */
    /* gradient buffers (zeroed by convnet_zero_grad) */
    float *gw1, *gb1, *gw2, *gb2;
    /* scratch (backward) */
    float *dc1;          /* c1H*c1W*K1 : dc1 (pre-pool1 grad) */
    float *dc2;          /* c2H*c2W*K2 */
    float *dp1;          /* c1H*c1W*K1 : dL/dp1 (stage1 pool grad, decoupled from dc1) */
};

static ConvConfig DFLT = { 28, 28, 6, 5, 2, 16, 5, 2 };

/* Small, deterministic xorshift RNG for He-init (module-local, no deps). */
static uint32_t s_rng = 0x1234ABCDu;
static void   c_seed(uint32_t s) { s_rng = s ? s : 0x9E3779B9u; }
static float  c_uni(float lo, float hi) {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    float u = (float)s_rng / (float)0xFFFFFFFFu;
    return lo + u * (hi - lo);
}

ConvNet *convnet_create(const ConvConfig *cfg) {
    if (!cfg) cfg = &DFLT;
    ConvNet *cn = (ConvNet *)calloc(1, sizeof(*cn));
    if (!cn) return NULL;
    cn->inH = cfg->inH; cn->inW = cfg->inW;
    cn->K1 = cfg->K1; cn->S1 = cfg->S1; cn->P1 = cfg->P1;
    cn->K2 = cfg->K2; cn->S2 = cfg->S2; cn->P2 = cfg->P2;

    cn->c1H = cn->inH - cn->S1 + 1;
    cn->c1W = cn->inW - cn->S1 + 1;
    cn->p1H = cn->c1H / cn->P1;
    cn->p1W = cn->c1W / cn->P1;

    cn->w1 = (float *)malloc((size_t)cn->K1 * cn->S1 * cn->S1 * sizeof(float));
    cn->b1 = (float *)malloc((size_t)cn->K1 * sizeof(float));
    cn->c1 = (float *)malloc((size_t)cn->c1H * cn->c1W * cn->K1 * sizeof(float));
    cn->p1 = (float *)malloc((size_t)cn->p1H * cn->p1W * cn->K1 * sizeof(float));
    cn->am1 = (int   *)malloc((size_t)cn->p1H * cn->p1W * cn->K1 * sizeof(int));

    cn->gw1 = (float *)calloc((size_t)cn->K1 * cn->S1 * cn->S1, sizeof(float));
    cn->gb1 = (float *)calloc((size_t)cn->K1, sizeof(float));

    if (cn->K2 > 0) {
        cn->c2H = cn->p1H - cn->S2 + 1;
        cn->c2W = cn->p1W - cn->S2 + 1;
        cn->p2H = cn->c2H / cn->P2;
        cn->p2W = cn->c2W / cn->P2;
        cn->w2 = (float *)malloc((size_t)cn->K2 * cn->K1 * cn->S2 * cn->S2 * sizeof(float));
        cn->b2 = (float *)malloc((size_t)cn->K2 * sizeof(float));
        cn->c2 = (float *)malloc((size_t)cn->c2H * cn->c2W * cn->K2 * sizeof(float));
        cn->p2 = (float *)malloc((size_t)cn->p2H * cn->p2W * cn->K2 * sizeof(float));
        cn->am2 = (int   *)malloc((size_t)cn->p2H * cn->p2W * cn->K2 * sizeof(int));
        cn->gw2 = (float *)calloc((size_t)cn->K2 * cn->K1 * cn->S2 * cn->S2, sizeof(float));
        cn->gb2 = (float *)calloc((size_t)cn->K2, sizeof(float));
        cn->dc2 = (float *)malloc((size_t)cn->c2H * cn->c2W * cn->K2 * sizeof(float));
        cn->fdim = cn->K2 * cn->p2H * cn->p2W;
    } else {
        cn->fdim = cn->K1 * cn->p1H * cn->p1W;
    }
    cn->dc1 = (float *)malloc((size_t)cn->c1H * cn->c1W * cn->K1 * sizeof(float));
    cn->dp1 = (float *)malloc((size_t)cn->c1H * cn->c1W * cn->K1 * sizeof(float));

    /* He init (deterministic via module-local RNG).
     * IMPORTANT: conv biases are seeded to a small POSITIVE constant so the
     * ReLU fires at initialization. Without this, on real glyph data (mostly
     * black background + thin strokes) every pre-activation is <= 0, the
     * feature maps stay all-zero, and the downstream MLP receives a constant
     * vector -> the network collapses to random (the 3.85% we saw). This is
     * the classic "dead conv" failure, not a gradient bug. */
    c_seed(0x1234ABCDu);
    float s1 = sqrtf(2.0f/(float)(cn->S1*cn->S1));
    for (int i=0;i<cn->K1*cn->S1*cn->S1;i++) cn->w1[i]=c_uni(-1,1)*s1;
    for (int k=0;k<cn->K1;k++) cn->b1[k]=0.5f;
    float s2 = sqrtf(2.0f/(float)(cn->K1*cn->S2*cn->S2));
    if (cn->K2>0) {
        for (int i=0;i<cn->K2*cn->K1*cn->S2*cn->S2;i++) cn->w2[i]=c_uni(-1,1)*s2;
        for (int k=0;k<cn->K2;k++) cn->b2[k]=0.5f;
    }

    return cn;
}

void convnet_destroy(ConvNet *cn) {
    if (!cn) return;
    free(cn->w1); free(cn->b1); free(cn->w2); free(cn->b2);
    free(cn->c1); free(cn->p1); free(cn->c2); free(cn->p2);
    free(cn->am1); free(cn->am2);
    free(cn->gw1); free(cn->gb1); free(cn->gw2); free(cn->gb2);
    free(cn->dc1); free(cn->dc2); free(cn->dp1);
    free(cn);
}

int convnet_dim(const ConvNet *cn) { return cn->fdim; }

void convnet_zero_grad(ConvNet *cn) {
    memset(cn->gw1,0,(size_t)cn->K1*cn->S1*cn->S1*sizeof(float));
    memset(cn->gb1,0,(size_t)cn->K1*sizeof(float));
    if (cn->K2>0) {
        memset(cn->gw2,0,(size_t)cn->K2*cn->K1*cn->S2*cn->S2*sizeof(float));
        memset(cn->gb2,0,(size_t)cn->K2*sizeof(float));
    }
}
void convnet_scale_grad(ConvNet *cn, float s) {
    for (int i=0;i<cn->K1*cn->S1*cn->S1;i++) cn->gw1[i]*=s;
    for (int i=0;i<cn->K1;i++) cn->gb1[i]*=s;
    if (cn->K2>0) {
        for (int i=0;i<cn->K2*cn->K1*cn->S2*cn->S2;i++) cn->gw2[i]*=s;
        for (int i=0;i<cn->K2;i++) cn->gb2[i]*=s;
    }
}

void convnet_forward(const ConvNet *cn, const float *img, float *out_features) {
    /* stage1 conv (valid) + ReLU */
    for (int k=0;k<cn->K1;k++){
        for (int y=0;y<cn->c1H;y++) for (int x=0;x<cn->c1W;x++){
            float s = cn->b1[k];
            const float *wk = cn->w1 + (size_t)k*cn->S1*cn->S1;
            for (int dy=0;dy<cn->S1;dy++) for (int dx=0;dx<cn->S1;dx++){
                int iy=y+dy, ix=x+dx;
                s += wk[dy*cn->S1+dx] * img[(size_t)iy*cn->inW+ix];
            }
            float a = s>0 ? s : 0.0f;   /* ReLU */
            cn->c1[((size_t)y*cn->c1W+x)*cn->K1+k] = a;
        }
    }
    /* stage1 maxpool */
    for (int k=0;k<cn->K1;k++) for (int py=0;py<cn->p1H;py++) for (int px=0;px<cn->p1W;px++){
        int best=0; float bv=-1e30f;
        for (int dy=0;dy<cn->P1;dy++) for (int dx=0;dx<cn->P1;dx++){
            int iy=py*cn->P1+dy, ix=px*cn->P1+dx;
            float v = cn->c1[((size_t)iy*cn->c1W+ix)*cn->K1+k];
            if (v>bv){ bv=v; best=iy*cn->c1W+ix; }
        }
        cn->p1[((size_t)py*cn->p1W+px)*cn->K1+k]=bv;
        cn->am1[((size_t)py*cn->p1W+px)*cn->K1+k]=best;
    }
    if (cn->K2>0) {
        /* stage2 conv over p1 */
        for (int k=0;k<cn->K2;k++){
            for (int y=0;y<cn->c2H;y++) for (int x=0;x<cn->c2W;x++){
                float s = cn->b2[k];
                const float *wk = cn->w2 + (size_t)k*cn->K1*cn->S2*cn->S2;
                for (int c=0;c<cn->K1;c++) for (int dy=0;dy<cn->S2;dy++) for (int dx=0;dx<cn->S2;dx++){
                    int iy=y+dy, ix=x+dx;
                    s += wk[(c*cn->S2+dy)*cn->S2+dx] * cn->p1[((size_t)iy*cn->p1W+ix)*cn->K1+c];
                }
                float a = s>0 ? s : 0.0f;
                cn->c2[((size_t)y*cn->c2W+x)*cn->K2+k]=a;
            }
        }
        for (int k=0;k<cn->K2;k++) for (int py=0;py<cn->p2H;py++) for (int px=0;px<cn->p2W;px++){
            int best=0; float bv=-1e30f;
            for (int dy=0;dy<cn->P2;dy++) for (int dx=0;dx<cn->P2;dx++){
                int iy=py*cn->P2+dy, ix=px*cn->P2+dx;
                float v = cn->c2[((size_t)iy*cn->c2W+ix)*cn->K2+k];
                if (v>bv){ bv=v; best=iy*cn->c2W+ix; }
            }
            cn->p2[((size_t)py*cn->p2W+px)*cn->K2+k]=bv;
            cn->am2[((size_t)py*cn->p2W+px)*cn->K2+k]=best;
        }
        /* features = p2 flattened */
        for (int i=0;i<cn->fdim;i++) out_features[i]=cn->p2[i];
    } else {
        for (int i=0;i<cn->fdim;i++) out_features[i]=cn->p1[i];
    }
}

void convnet_backward(ConvNet *cn, const float *img, const float *features, const float *dfeatures) {
    (void)features;   /* cached forward output; backprop uses cn->c1/p1/c2/p2 */
    /* df -> dL/dp1 (or dL/dp2 if K2>0). dp1 is a dedicated
     * scratch, NOT an alias of dc1, so we can zero dc1 safely. */
    float *dp1 = cn->dp1;
    if (cn->K2>0) {
        /* dL/dp2 = df ; backward through stage2 maxpool -> dc2 ; conv2 -> dp1 */
        float *dp2 = cn->dc2;  /* dL/dp2 placed in C2-SPACE (matches am2 + conv2 indexing) */
        for (int i=0;i<cn->c2H*cn->c2W*cn->K2;i++) dp2[i]=0.0f;
        for (int k=0;k<cn->K2;k++) for (int py=0;py<cn->p2H;py++) for (int px=0;px<cn->p2W;px++){
            int pc = ((size_t)py*cn->p2W+px)*cn->K2+k;   /* p2-space cell */
            dp2[cn->am2[pc]] += dfeatures[pc];               /* scatter to its c2-space argmax */
        }
        /* conv2 backward: dc2 -> gw2, gb2, dp1 */
        for (int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) dp1[i]=0.0f;
        for (int k=0;k<cn->K2;k++){
            for (int y=0;y<cn->c2H;y++) for (int x=0;x<cn->c2W;x++){
                float g = cn->dc2[((size_t)y*cn->c2W+x)*cn->K2+k];
                cn->gb2[k] += g;
                float *wk = cn->w2 + (size_t)k*cn->K1*cn->S2*cn->S2;
                for (int c=0;c<cn->K1;c++) for (int dy=0;dy<cn->S2;dy++) for (int dx=0;dx<cn->S2;dx++){
                    int iy=y+dy, ix=x+dx;
                    int pidx=((size_t)iy*cn->p1W+ix)*cn->K1+c;
                    float pv = cn->p1[pidx];
                    cn->gw2[((size_t)k*cn->K1*cn->S2*cn->S2)+((size_t)(c*cn->S2+dy)*cn->S2+dx)] += g*pv;
                    dp1[pidx] += g * wk[(c*cn->S2+dy)*cn->S2+dx];
                }
            }
        }
    } else {
        /* K2==0: features ARE the stage1 pooled activations (p1-space).
         * Scatter dL/dp1 into dp1 in C1-SPACE (matches am1 + conv1). */
        for (int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) cn->dp1[i]=0.0f;
        for (int k=0;k<cn->K1;k++) for (int py=0;py<cn->p1H;py++) for (int px=0;px<cn->p1W;px++){
            int pc = ((size_t)py*cn->p1W+px)*cn->K1+k;     /* p1-space cell */
            cn->dp1[cn->am1[pc]] += dfeatures[pc];           /* scatter to its c1-space argmax */
        }
    }
    /* stage1 maxpool backward: dp1 at argmax -> dc1 */
    for (int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) cn->dc1[i]=0.0f;
    for (int k=0;k<cn->K1;k++) for (int py=0;py<cn->p1H;py++) for (int px=0;px<cn->p1W;px++){
        int a = cn->am1[((size_t)py*cn->p1W+px)*cn->K1+k];   /* c1-space argmax */
        cn->dc1[a] += cn->dp1[a];                              /* dL/dp1 lives at that c1 cell */
    }
    /* ReLU grad: zero where c1<=0 */
    /* conv1 backward: dc1 -> gw1,gb1,dimg */
    (void)img; /* img used below */
    for (int k=0;k<cn->K1;k++){
        for (int y=0;y<cn->c1H;y++) for (int x=0;x<cn->c1W;x++){
            int cidx=((size_t)y*cn->c1W+x)*cn->K1+k;
            float dpre = cn->dc1[cidx];
            if (cn->c1[cidx] <= 0.0f) dpre = 0.0f;   /* ReLU */
            cn->gb1[k] += dpre;
            float *wk = cn->gw1 + (size_t)k*cn->S1*cn->S1;
            for (int dy=0;dy<cn->S1;dy++) for (int dx=0;dx<cn->S1;dx++){
                int iy=y+dy, ix=x+dx;
                wk[dy*cn->S1+dx] += dpre * img[(size_t)iy*cn->inW+ix];
            }
        }
    }
    if (getenv("CN_DBG")) {
        float sd1=0,sp1=0,sw=0;
        for (int i=0;i<cn->c1H*cn->c1W*cn->K1;i++) sd1+=fabsf(cn->dc1[i]);
        for (int i=0;i<cn->p1H*cn->p1W*cn->K1;i++) sp1+=fabsf(cn->dp1[i]);
        for (int i=0;i<cn->K1*cn->S1*cn->S1;i++) sw+=fabsf(cn->gw1[i]);
        fprintf(stderr,"CN_DBG dc1=%.4f dp1=%.4f gw1=%.4f gw1[0]=%.6f\n",sd1,sp1,sw,cn->gw1[0]);
    }
}

void convnet_apply_plain(ConvNet *cn, float lr) {
    for (int i=0;i<cn->K1*cn->S1*cn->S1;i++) cn->w1[i] -= lr*cn->gw1[i];
    for (int i=0;i<cn->K1;i++) cn->b1[i] -= lr*cn->gb1[i];
    if (cn->K2>0) {
        for (int i=0;i<cn->K2*cn->K1*cn->S2*cn->S2;i++) cn->w2[i] -= lr*cn->gw2[i];
        for (int i=0;i<cn->K2;i++) cn->b2[i] -= lr*cn->gb2[i];
    }
}

int convnet_layer_count(const ConvNet *cn) { return cn->K2>0 ? 4 : 2; }
ConvLayer convnet_layer(ConvNet *cn, int idx) {
    ConvLayer L = {0};
    switch (idx) {
        case 0: L.param=cn->w1; L.grad=cn->gw1; L.n=cn->K1*cn->S1*cn->S1; break;
        case 1: L.param=cn->b1; L.grad=cn->gb1; L.n=cn->K1; break;
        case 2: L.param=cn->w2; L.grad=cn->gw2; L.n=cn->K2*cn->K1*cn->S2*cn->S2; break;
        default:L.param=cn->b2; L.grad=cn->gb2; L.n=cn->K2; break;
    }
    return L;
}

static void dump_floats(FILE*f, const float*a, int n){ for(int i=0;i<n;i++) fprintf(f,"%a\n",a[i]); }
static int read_floats(FILE*f, float*a, int n){ for(int i=0;i<n;i++) if(fscanf(f,"%a\n",&a[i])!=1) return -1; return 0; }

int convnet_save(const ConvNet *cn, const char *path) {
    FILE *f=fopen(path,"w"); if(!f) return -1;
    fprintf(f,"wubu_ocr_conv_v1 %d %d %d %d %d %d %d %d\n",
            cn->inH,cn->inW,cn->K1,cn->S1,cn->P1,cn->K2,cn->S2,cn->P2);
    dump_floats(f,cn->w1,cn->K1*cn->S1*cn->S1);
    dump_floats(f,cn->b1,cn->K1);
    if(cn->K2>0){ dump_floats(f,cn->w2,cn->K2*cn->K1*cn->S2*cn->S2); dump_floats(f,cn->b2,cn->K2); }
    fclose(f); return 0;
}
int convnet_load(const char *path, ConvNet **out, ConvConfig *cfg) {
    FILE *f=fopen(path,"r"); if(!f) return -1;
    ConvConfig c; int inH,inW,K1,S1,P1,K2,S2,P2;
    if(fscanf(f,"wubu_ocr_conv_v1 %d %d %d %d %d %d %d %d\n",&inH,&inW,&K1,&S1,&P1,&K2,&S2,&P2)!=8){fclose(f);return -1;}
    c=(ConvConfig){inH,inW,K1,S1,P1,K2,S2,P2};
    ConvNet *cn=convnet_create(&c); if(!cn){fclose(f);return -1;}
    if(read_floats(f,cn->w1,cn->K1*cn->S1*cn->S1)){convnet_destroy(cn);fclose(f);return -1;}
    if(read_floats(f,cn->b1,cn->K1)){convnet_destroy(cn);fclose(f);return -1;}
    if(cn->K2>0){
        if(read_floats(f,cn->w2,cn->K2*cn->K1*cn->S2*cn->S2)){convnet_destroy(cn);fclose(f);return -1;}
        if(read_floats(f,cn->b2,cn->K2)){convnet_destroy(cn);fclose(f);return -1;}
    }
    fclose(f); *out=cn; if(cfg)*cfg=c; return 0;
}
