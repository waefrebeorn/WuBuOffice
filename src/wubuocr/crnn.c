/* crnn.c -- CRNN recognizer (see crnn.h). */
#include "crnn.h"
#include "image.h"
#include "convnet3.h"
#include "rnn.h"
#include "gru.h"
#include "ctc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _OPENMP
#include <omp.h>
#endif

/* local UTF-8 encoder (no external dep) */
static int utf8_enc(uint32_t cp, char *buf){
    if(cp<0x80){ buf[0]=(char)cp; buf[1]=0; return 1; }
    if(cp<0x800){ buf[0]=(char)(0xC0|(cp>>6)); buf[1]=(char)(0x80|(cp&0x3F)); buf[2]=0; return 2; }
    if(cp<0x10000){ buf[0]=(char)(0xE0|(cp>>12)); buf[1]=(char)(0x80|((cp>>6)&0x3F));
        buf[2]=(char)(0x80|(cp&0x3F)); buf[3]=0; return 3; }
    buf[0]=(char)(0xF0|(cp>>18)); buf[1]=(char)(0x80|((cp>>12)&0x3F));
    buf[2]=(char)(0x80|((cp>>6)&0x3F)); buf[3]=(char)(0x80|(cp&0x3F)); buf[4]=0; return 4;
}

struct CRNN {
    int strip;          /* strip width in px */
    int stride;         /* strip step in px (<=strip for overlap; ==strip default) */
    int D;              /* conv feature dim (convnet3_dim) */
    int Hconv, Wconv;   /* conv input H/W (strip is square = strip x strip) */
    int hid;
    int C;              /* classes incl blank */
    int bidir;
    int freeze_conv;  /* 1 = don't train the conv trunk (head+LSTM+CTC only) */
    int rnn_type;     /* 1 = LSTM (default), 2 = GRU */
    ConvConfig3 cfg;    /* saved conv arch (for crnn_save/crnn_load) */
    ConvNet3 *conv;
    void *rnn;          /* either LSTM* or GRU* depending on rnn_type */
    /* output linear head: Wout (C x rnn_outdim) + bout (C) */
    int rnn_out;
    float *Wout, *bout, *gWout, *gbout;
    /* scratch */
    float *feat;        /* T*D */
    float *strips;      /* T*strip*strip (input pixels, stored for backward) */
    int curT;           /* last T used */
    int Tcap;           /* allocated capacity (in time steps) for scratch buffers */
    float *rnn_out_buf; /* T*rnn_out */
    float *logits;      /* T*C */
    float *dy_rnn;      /* T*rnn_out */
    float *dlogits;     /* T*C (from CTC) */
    /* Adam state (per param group, concatenated) */
    float *adam_m, *adam_v;
    int nparams_conv, nparams_rnn, nparams_head;
    int conv_off, rnn_off, head_off, total;
    float *param_all, *grad_all;
    uint32_t rng;
};

static float rndf(uint32_t*s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return ((float)(*s&0xFFFFFF)/(float)0xFFFFFF)*2.0f-1.0f;}

/* ---- sequence-model dispatch: route LSTM (type 1) or GRU (type 2) ---- */
static void  seq_create(CRNN *m, int din, int hid, int bidir, uint32_t seed){
    if(m->rnn_type==2) m->rnn = gru_create(din,hid,bidir,seed);
    else                m->rnn = lstm_create(din,hid,bidir,seed);
}
static int   seq_outdim(CRNN *m){ return m->rnn_type==2 ? gru_outdim((GRU*)m->rnn) : lstm_outdim((LSTM*)m->rnn); }
static int   seq_num_params(CRNN *m){ return m->rnn_type==2 ? gru_num_params((GRU*)m->rnn) : lstm_num_params((LSTM*)m->rnn); }
static float *seq_param(CRNN *m){ return m->rnn_type==2 ? gru_param((GRU*)m->rnn) : lstm_param((LSTM*)m->rnn); }
static float *seq_grad(CRNN *m){ return m->rnn_type==2 ? gru_grad((GRU*)m->rnn) : lstm_grad((LSTM*)m->rnn); }
static void  seq_zero_grad(CRNN *m){ if(m->rnn_type==2) gru_zero_grad((GRU*)m->rnn); else lstm_zero_grad((LSTM*)m->rnn); }
static void  seq_forward(CRNN *m, int T, const float *x){
    if(m->rnn_type==2) gru_forward((GRU*)m->rnn,T,x); else lstm_forward((LSTM*)m->rnn,T,x);
}
static void  seq_get_output(CRNN *m, float *y){
    if(m->rnn_type==2) gru_get_output((GRU*)m->rnn,y); else lstm_get_output((LSTM*)m->rnn,y);
}
static void  seq_backward(CRNN *m, int T, const float *dy, float *dx){
    if(m->rnn_type==2) gru_backward((GRU*)m->rnn,T,dy,dx); else lstm_backward((LSTM*)m->rnn,T,dy,dx);
}
static void  seq_free(CRNN *m){ if(!m->rnn) return; if(m->rnn_type==2) gru_free((GRU*)m->rnn); else lstm_free((LSTM*)m->rnn); }


CRNN *crnn_create(const ConvConfig3 *conv_cfg, int strip, int lstm_hid, int nclass, int bidir, uint32_t seed){
    CRNN *m = calloc(1,sizeof *m); if(!m) return NULL;
    m->strip=strip; m->stride=strip; m->Hconv=strip; m->Wconv=strip;
    m->cfg = *conv_cfg;
    m->conv = convnet3_create(conv_cfg); if(!m->conv){ free(m); return NULL; }
    m->D = convnet3_dim(m->conv);
    m->hid=lstm_hid; m->C=nclass; m->bidir=bidir; m->rng=seed?seed:0x1234u;
    const char *rt=getenv("RNN_TYPE"); m->rnn_type = (rt && atoi(rt)==2)? 2 : 1;
    m->freeze_conv=0;  /* conv trunk trainable (heap-overflow bugs that corrupted conv-bwd are fixed) */
    seq_create(m, m->D, lstm_hid, bidir, seed^0x55u); if(!m->rnn){ convnet3_destroy(m->conv); free(m); return NULL; }
    m->rnn_out = seq_outdim(m);
    m->Wout = malloc((size_t)m->C*m->rnn_out*sizeof(float));
    m->bout = malloc(m->C*sizeof(float));
    m->gWout = calloc((size_t)m->C*m->rnn_out,sizeof(float));
    m->gbout = calloc(m->C,sizeof(float));
    float s=sqrtf(1.0f/(float)(m->rnn_out));
    for(int i=0;i<m->C*m->rnn_out;i++) m->Wout[i]=rndf(&m->rng)*s;
    for(int i=0;i<m->C;i++) m->bout[i]=0.0f;
    /* param counts */
    m->nparams_conv = convnet3_layer_count(m->conv); /* not a count of params; recompute below */
    /* conv total params: sum of layer param sizes */
    m->nparams_conv=0;
    int nl=convnet3_layer_count(m->conv);
    for(int i=0;i<nl;i++){ ConvLayer3 L=convnet3_layer((ConvNet3*)m->conv,i); m->nparams_conv+=L.n; }
    m->nparams_rnn = seq_num_params(m);
    m->nparams_head = m->C*m->rnn_out + m->C;
    m->conv_off=0; m->rnn_off=m->nparams_conv; m->head_off=m->rnn_off+m->nparams_rnn; m->total=m->head_off+m->nparams_head;
    m->param_all = malloc((size_t)m->total*sizeof(float));
    m->grad_all = calloc((size_t)m->total,sizeof(float));
    m->adam_m = calloc((size_t)m->total,sizeof(float));
    m->adam_v = calloc((size_t)m->total,sizeof(float));
    /* point sub-grads at slices of grad_all */
    return m;
}

void crnn_free(CRNN *m){
    if(!m) return;
    convnet3_destroy(m->conv); seq_free(m);
    free(m->Wout); free(m->bout); free(m->gWout); free(m->gbout);
    free(m->feat); free(m->strips); free(m->rnn_out_buf); free(m->logits); free(m->dy_rnn); free(m->dlogits);
    free(m->param_all); free(m->grad_all); free(m->adam_m); free(m->adam_v);
    free(m);
}

int crnn_feat_dim(const CRNN *m){ return m? m->D : 0; }

void crnn_set_freeze_conv(CRNN *m, int freeze){ if(m) m->freeze_conv = freeze?1:0; }
void crnn_set_stride(CRNN *m, int stride){ if(m && stride>0 && stride<=m->strip) m->stride=stride; }
int  crnn_get_stride(const CRNN *m){ return m? m->stride : 0; }

int crnn_time_steps(const CRNN *m, int img_w){
    if(img_w < m->strip) return 1;
    return (img_w - m->strip) / m->stride + 1;
}

/* Ensure all T-sized scratch buffers hold at least T time steps. Reallocs on
 * growth (T varies with image width & stride). */
static void ensure_cap(CRNN *m, int T){
    if(T <= m->Tcap) return;
    m->strips      = realloc(m->strips,      (size_t)T*m->strip*m->strip*sizeof(float));
    m->feat        = realloc(m->feat,        (size_t)T*m->D*sizeof(float));
    m->rnn_out_buf = realloc(m->rnn_out_buf, (size_t)T*m->rnn_out*sizeof(float));
    m->logits      = realloc(m->logits,      (size_t)T*m->C*sizeof(float));
    m->dy_rnn      = realloc(m->dy_rnn,      (size_t)T*m->rnn_out*sizeof(float));
    m->dlogits     = realloc(m->dlogits,     (size_t)T*m->C*sizeof(float));
    m->Tcap = T;
}

/* slice img into T strips of width `strip`, run conv per strip -> feat (T*D).
 * OpenMP-parallel over strips when compiled with -fopenmp (per-strip conv is
 * independent; convnet3 is read-only here, so no race on weights).
 * Early-exit: if EARLY_EXIT and a strip is near-uniform (blank whitespace),
 * skip the conv entirely (write zeros) -- real speedup on inter-word gaps. */
static int slice_and_conv(CRNN *m, const OcrImage *img, float *feat){
    int W=ocr_image_width(img), H=ocr_image_height(img);
    int T = crnn_time_steps(m, W);
    ensure_cap(m, T);
    if(feat==NULL) feat = m->feat;
    float *stripbuf = m->strips;
    int do_ee = getenv("EARLY_EXIT")? atoi(getenv("EARLY_EXIT")):0;
    #ifdef _OPENMP
    #pragma omp parallel for schedule(static)
    #endif
    for(int t=0;t<T;t++){
        int x0 = t*m->stride;
        float *sb = stripbuf + (size_t)t*m->strip*m->strip;
        int isblank=1; float mn=255,mx=0;
        for(int y=0;y<m->strip;y++){
            int yy = (y < H)? y : H-1;
            for(int x=0;x<m->strip;x++){
                int xx = x0 + x; if(xx>=W) xx=W-1;
                uint8_t g=ocr_image_get(img, xx, yy);
                sb[y*m->strip+x] = g/255.0f;
                if(g<128){ isblank=0; }            /* any ink */
                if(g<mn)mn=g; if(g>mx)mx=g;
            }
        }
        if(do_ee && isblank && (mx-mn)<8){ memset(feat+(size_t)t*m->D,0,m->D*sizeof(float)); continue; }
        convnet3_forward(m->conv, sb, feat + (size_t)t*m->D);
    }
    m->curT = T;
    return T;
}

int crnn_forward_seq(CRNN *m, int T, const float *seq_feats, float *logits){
    ensure_cap(m, T);
    memcpy(m->feat, seq_feats, (size_t)T*m->D*sizeof(float));
    seq_forward(m, T, m->feat);
    seq_get_output(m, m->rnn_out_buf);
    for(int t=0;t<T;t++) for(int c=0;c<m->C;c++){
        float a = m->bout[c];
        for(int k=0;k<m->rnn_out;k++) a += m->Wout[c*m->rnn_out+k]*m->rnn_out_buf[(size_t)t*m->rnn_out+k];
        logits[(size_t)t*m->C+c]=a;
    }
    return T;
}

int crnn_forward_img(CRNN *m, const OcrImage *img, float *logits){
    int T = slice_and_conv(m, img, NULL);
    /* NOTE: slice_and_conv may realloc m->logits via ensure_cap(), so always
     * use m->logits (not the passed pointer) for the post-conv sequence step. */
    (void)logits;
    return crnn_forward_seq(m, T, m->feat, m->logits);
}

int crnn_predict(CRNN *m, const OcrImage *img, int *out){
    int T = crnn_time_steps(m, ocr_image_width(img));
    if(!m->logits) m->logits = malloc((size_t)T*m->C*sizeof(float));
    crnn_forward_img(m, img, m->logits);
    /* BEAM env (default 0 => greedy) selects beam-search decode; higher width
     * fixes repeat-collapse on ambiguous/long words. */
    int beam = getenv("BEAM")? atoi(getenv("BEAM")) : 0;
    if(beam>0) return ctc_beam_decode(T, m->C, m->logits, beam, out);
    return ctc_greedy_decode(T, m->C, m->logits, out);
}

int crnn_recognize(CRNN *m, const OcrImage *line, const char *charset,
                   char *out, int cap){
    int T = crnn_time_steps(m, ocr_image_width(line));
    int *idx = malloc((size_t)T*sizeof(int));
    int n = crnn_predict(m, line, idx);
    int o=0, cslen=(int)strlen(charset);
    for(int i=0;i<n && o<cap-1;i++){
        int k=idx[i];                 /* class 1..C-1 */
        if(k>=1 && k-1<cslen) out[o++]=charset[k-1];
    }
    out[o]='\0';
    free(idx);
    return o;
}

int crnn_recognize_scored(CRNN *m, const OcrImage *line, const char *charset,
                          char *out, int cap, int *conf){
    int T = crnn_time_steps(m, ocr_image_width(line));
    int *idx = malloc((size_t)T*sizeof(int));
    int n = crnn_predict(m, line, idx);
    int o=0, cslen=(int)strlen(charset);
    /* confidence: mean over frames of the max softmax of the logits */
    float csum = 0.0f;
    if (m->logits && n > 0) {
        for (int t = 0; t < n; t++) {
            const float *row = m->logits + (size_t)t * m->C;
            float mx = -1e30f; float denom = 0.0f;
            for (int c = 0; c < m->C; c++) { float e = (float)expf(row[c]); denom += e; if (e > mx) mx = e; }
            if (denom > 0.0f) csum += mx / denom;
        }
        csum = (csum / (float)n) * 100.0f;
    }
    if (conf) *conf = (int)(csum + 0.5f);
    for(int i=0;i<n && o<cap-1;i++){
        int k=idx[i];
        if(k>=1 && k-1<cslen) out[o++]=charset[k-1];
    }
    out[o]='\0';
    free(idx);
    return o;
}

int crnn_recognize_scored_chars(CRNN *m, const OcrImage *line, const char *charset,
                                char *out, int cap, int *conf,
                                int *cconf, int cconf_cap){
    int T = crnn_time_steps(m, ocr_image_width(line));
    int *idx = malloc((size_t)T*sizeof(int));
    int n = crnn_predict(m, line, idx);
    int o=0, cslen=(int)strlen(charset);
    float csum = 0.0f;
    if (m->logits && n > 0) {
        for (int t = 0; t < n; t++) {
            const float *row = m->logits + (size_t)t * m->C;
            float mx = -1e30f; float denom = 0.0f;
            for (int c = 0; c < m->C; c++) { float e = (float)expf(row[c]); denom += e; if (e > mx) mx = e; }
            float p = (denom > 0.0f) ? mx/denom : 0.0f;
            /* emitted character t gets this frame's max-softmax as its confidence */
            if (t < cconf_cap) cconf[t] = (int)(p*100.0f + 0.5f);
            csum += p;
        }
        csum = (csum / (float)n) * 100.0f;
    }
    if (conf) *conf = (int)(csum + 0.5f);
    for(int i=0;i<n && o<cap-1;i++){
        int k=idx[i];
        if(k>=1 && k-1<cslen) out[o++]=charset[k-1];
    }
    out[o]='\0';
    free(idx);
    return o;
}

/* UTF-8-aware recognition. `cp_of_class(cls)` returns the codepoint for class
 * cls (cls in 0..C-1, where 0 is blank), or 0 to skip. Multibyte-safe: each
 * codepoint is UTF-8 encoded into out. This replaces the byte-indexed
 * crnn_recognize (ASCII-only) so the file-level document path is multilingual. */
int crnn_recognize_utf8(CRNN *m, const OcrImage *line,
                        uint32_t (*cp_of_class)(int cls, void *u), void *u,
                        char *out, int cap){
    int T = crnn_time_steps(m, ocr_image_width(line));
    int *idx = malloc((size_t)T*sizeof(int));
    int n = crnn_predict(m, line, idx);
    int o=0;
    for(int i=0;i<n;i++){
        int k=idx[i];
        uint32_t cp = cp_of_class ? cp_of_class(k, u) : 0;
        if(!cp) continue;
        char b[8]; int nb=utf8_enc(cp,b);
        if(o+nb>=cap-1) break;
        memcpy(out+o,b,nb); o+=nb;
    }
    out[o]='\0';
    free(idx);
    return o;
}

/* ---- model persistence ---- */
#include <stdio.h>
#define CRNN_MAGIC 0x43524E31u  /* 'CRN1' */

int crnn_save(const CRNN *m, const char *path){
    if(!m||!path) return 0;
    FILE *f=fopen(path,"wb"); if(!f) return 0;
    uint32_t magic=CRNN_MAGIC;
    int cbam = convnet3_cbam_enabled((ConvNet3*)m->conv) ? 1 : 0;
    int flags = (m->rnn_type & 0xF) | ((cbam&0xF)<<4);
    int hdr[8]={ m->strip, m->hid, m->C, m->bidir, m->D, m->rnn_out, m->stride, flags };
    int ok=1;
    ok&=fwrite(&magic,sizeof magic,1,f)==1;
    ok&=fwrite(&m->cfg,sizeof m->cfg,1,f)==1;
    ok&=fwrite(hdr,sizeof hdr,1,f)==1;
    /* conv layers */
    int nl=convnet3_layer_count((ConvNet3*)m->conv);
    for(int i=0;i<nl;i++){ ConvLayer3 L=convnet3_layer((ConvNet3*)m->conv,i);
        ok&=fwrite(L.param,sizeof(float),L.n,f)==(size_t)L.n; }
    /* rnn + head */
    ok&=fwrite(seq_param((CRNN*)m),sizeof(float),m->nparams_rnn,f)==(size_t)m->nparams_rnn;
    ok&=fwrite(m->Wout,sizeof(float),(size_t)m->C*m->rnn_out,f)==(size_t)m->C*m->rnn_out;
    ok&=fwrite(m->bout,sizeof(float),m->C,f)==(size_t)m->C;
    /* CBAM (if present) */
    int csz=convnet3_cbam_size((ConvNet3*)m->conv);
    if(csz>0){ float *buf=malloc((size_t)csz*sizeof(float)); convnet3_cbam_pack((ConvNet3*)m->conv,buf);
        ok&=fwrite(buf,sizeof(float),csz,f)==(size_t)csz; free(buf); }
    fclose(f);
    return ok;
}

int crnn_load(const char *path, CRNN **out){
    if(out) *out=NULL;
    if(!path||!out) return 0;
    FILE *f=fopen(path,"rb"); if(!f) return 0;
    uint32_t magic=0; ConvConfig3 cfg; int hdr[8];
    if(fread(&magic,sizeof magic,1,f)!=1 || magic!=CRNN_MAGIC){ fclose(f); return 0; }
    if(fread(&cfg,sizeof cfg,1,f)!=1){ fclose(f); return 0; }
    /* header length is variable: old files wrote 7 ints, new write 8. Read up to 8. */
    size_t got=fread(hdr,sizeof(int),8,f);
    if(got<7){ fclose(f); return 0; }
    int strip=hdr[0], hid=hdr[1], C=hdr[2], bidir=hdr[3];
    int flags = (got>=8)? hdr[7] : 0;
    int rnn_type = flags & 0xF;
    int want_cbam = (flags>>4)&0xF;
    CRNN *m=crnn_create(&cfg, strip, hid, C, bidir, 1u);
    if(!m){ fclose(f); return 0; }
    m->rnn_type = rnn_type;   /* load may switch LSTM<->GRU on a fresh create; NOTE: a
                                 * mismatch would rebuild weights, so we instead re-create. */
    /* If the saved rnn_type differs from what crnn_create built, rebuild rnn. */
    if(m->rnn_type != rnn_type || convnet3_cbam_enabled((ConvNet3*)m->conv) != want_cbam){
        /* rebuild rnn with correct type */
        seq_free(m);
        m->rnn_type = rnn_type;
        seq_create(m, m->D, m->hid, m->bidir, 1u^0x55u);
        m->rnn_out = seq_outdim(m);
        /* rebuild head for new rnn_out */
        free(m->Wout); free(m->bout); free(m->gWout); free(m->gbout);
        m->Wout=malloc((size_t)m->C*m->rnn_out*sizeof(float));
        m->bout=malloc(m->C*sizeof(float));
        m->gWout=calloc((size_t)m->C*m->rnn_out,sizeof(float));
        m->gbout=calloc(m->C,sizeof(float));
        m->nparams_rnn = seq_num_params(m);
        m->nparams_head = m->C*m->rnn_out + m->C;
        m->rnn_off=m->nparams_conv; m->head_off=m->rnn_off+m->nparams_rnn; m->total=m->head_off+m->nparams_head;
        m->param_all=realloc(m->param_all,(size_t)m->total*sizeof(float));
        m->grad_all =calloc((size_t)m->total,sizeof(float));
        m->adam_m  =calloc((size_t)m->total,sizeof(float));
        m->adam_v  =calloc((size_t)m->total,sizeof(float));
        if(want_cbam) convnet3_enable_cbam((ConvNet3*)m->conv);
    }
    if(m->D!=hdr[4] || m->rnn_out!=hdr[5]){ crnn_free(m); fclose(f); return 0; }
    if(hdr[6]>0) m->stride = hdr[6];   /* restore stride (0 in old files -> keep default) */
    int ok=1, nl=convnet3_layer_count(m->conv);
    for(int i=0;i<nl;i++){ ConvLayer3 L=convnet3_layer(m->conv,i);
        ok&=fread(L.param,sizeof(float),L.n,f)==(size_t)L.n; }
    ok&=fread(seq_param(m),sizeof(float),m->nparams_rnn,f)==(size_t)m->nparams_rnn;
    ok&=fread(m->Wout,sizeof(float),(size_t)m->C*m->rnn_out,f)==(size_t)m->C*m->rnn_out;
    ok&=fread(m->bout,sizeof(float),m->C,f)==(size_t)m->C;
    /* CBAM (if saved) */
    int csz=convnet3_cbam_size((ConvNet3*)m->conv);
    if(csz>0){ float *buf=malloc((size_t)csz*sizeof(float));
        ok&=fread(buf,sizeof(float),csz,f)==(size_t)csz; convnet3_cbam_unpack((ConvNet3*)m->conv,buf); free(buf); }
    fclose(f);
    if(!ok){ crnn_free(m); return 0; }
    *out=m;
    return 1;
}

float crnn_train_step(CRNN *m, int T, const float *seq_feats, int L, const int *target, const OcrImage *img){
    /* forward */
    float *feat;
    if(img){ T = crnn_time_steps(m, ocr_image_width(img)); ensure_cap(m, T);
             slice_and_conv(m, img, m->feat); feat=m->feat; }
    else   { ensure_cap(m, T); memcpy(m->feat, seq_feats, (size_t)T*m->D*sizeof(float)); feat=m->feat; }
    seq_forward(m, T, feat);
    seq_get_output(m, m->rnn_out_buf);
    for(int t=0;t<T;t++) for(int c=0;c<m->C;c++){
        float a=m->bout[c]; for(int k=0;k<m->rnn_out;k++) a+=m->Wout[c*m->rnn_out+k]*m->rnn_out_buf[(size_t)t*m->rnn_out+k];
        m->logits[(size_t)t*m->C+c]=a;
    }
    /* CTC loss + grad w.r.t logits */
    float loss = ctc_loss(T, m->C, L, target, m->logits, m->dlogits,
                           getenv("CTC_SMOOTH")? (float)atof(getenv("CTC_SMOOTH")) : 0.0f,
                           getenv("CTC_FOCAL")? (float)atof(getenv("CTC_FOCAL")) : 0.0f);
    /* dlogits -> drnn_out (head backward) */
    if(!m->dy_rnn) m->dy_rnn=malloc((size_t)T*m->rnn_out*sizeof(float));
    for(int t=0;t<T;t++) for(int k=0;k<m->rnn_out;k++){
        float g=0; for(int c=0;c<m->C;c++) g += m->dlogits[(size_t)t*m->C+c]*m->Wout[c*m->rnn_out+k];
        m->dy_rnn[(size_t)t*m->rnn_out+k]=g;
    }
    /* head weight grads */
    memset(m->gWout,0,(size_t)m->C*m->rnn_out*sizeof(float));
    memset(m->gbout,0,m->C*sizeof(float));
    for(int t=0;t<T;t++) for(int c=0;c<m->C;c++){
        m->gbout[c]+=m->dlogits[(size_t)t*m->C+c];
        for(int k=0;k<m->rnn_out;k++) m->gWout[c*m->rnn_out+k]+=m->dlogits[(size_t)t*m->C+c]*m->rnn_out_buf[(size_t)t*m->rnn_out+k];
    }
    /* LSTM backward (accumulates into lstm grad) */
    seq_zero_grad(m);
    if(!m->feat) m->feat=malloc((size_t)T*m->D*sizeof(float));
    float *dx = malloc((size_t)T*m->D*sizeof(float));
    seq_backward(m, T, m->dy_rnn, dx);
    /* conv backward per strip (only when we ran conv fwd on an image, and not frozen) */
    if(!m->freeze_conv && img && m->strips){
    convnet3_zero_grad(m->conv);
    for(int t=0;t<T;t++){
        float *sb = m->strips + (size_t)t*m->strip*m->strip;
        float *df = dx + (size_t)t*m->D;
        convnet3_backward(m->conv, sb, m->feat+(size_t)t*m->D, df);
    }
    }
    free(dx);
    return loss;
}

void crnn_adam(CRNN *m, float lr, int t){
    float b1=0.9f,b2=0.999f,eps=1e-8f;
    float bc1=1.0f-powf(b1,(float)t), bc2=1.0f-powf(b2,(float)t);
    /* gather params+grads into param_all/grad_all for uniform update */
    int nl=convnet3_layer_count(m->conv); int off=0;
    for(int i=0;i<nl;i++){ ConvLayer3 L=convnet3_layer(m->conv,i); memcpy(m->param_all+off,L.param,L.n*sizeof(float)); memcpy(m->grad_all+off,L.grad,L.n*sizeof(float)); off+=L.n; }
    memcpy(m->param_all+m->rnn_off, seq_param(m), m->nparams_rnn*sizeof(float));
    memcpy(m->grad_all+m->rnn_off, seq_grad(m), m->nparams_rnn*sizeof(float));
    memcpy(m->param_all+m->head_off, m->Wout, (size_t)m->C*m->rnn_out*sizeof(float));
    memcpy(m->param_all+m->head_off+(size_t)m->C*m->rnn_out, m->bout, m->C*sizeof(float));
    memcpy(m->grad_all+m->head_off, m->gWout, (size_t)m->C*m->rnn_out*sizeof(float));
    memcpy(m->grad_all+m->head_off+(size_t)m->C*m->rnn_out, m->gbout, m->C*sizeof(float));
    /* conv grad clip (per-element) to tame the trunk (see wubuocr skill) */
    for(int i=0;i<m->nparams_conv;i++){
        float g=m->grad_all[i];
        if(g>0.3f) m->grad_all[i]=0.3f; else if(g<-0.3f) m->grad_all[i]=-0.3f;
    }
    /* Conv trunk: plain SGD with a TINY LR. Adam normalizes away gradient scale,
     * which defeats the small-conv-step strategy and destabilizes the head, so the
     * conv MUST use grad-proportional SGD (see wubuocr skill: conv_fac ~5e-4). */
    float conv_lr = lr*0.02f;
    for(int i=0;i<m->nparams_conv;i++) m->param_all[i] -= conv_lr*m->grad_all[i];
    /* rnn + head: Adam (with optional global-norm clip to stop CTC explosions) */
    float gclip = getenv("GRAD_CLIP")? (float)atof(getenv("GRAD_CLIP")) : 0.0f;
    float gnorm = 0.0f;
    if(gclip>0.0f){ for(int i=m->nparams_conv;i<m->total;i++){ float g=m->grad_all[i]; gnorm+=g*g; } gnorm=sqrtf(gnorm); }
    float gscale = (gclip>0.0f && gnorm>gclip)? gclip/gnorm : 1.0f;
    for(int i=m->nparams_conv;i<m->total;i++){
        float g=m->grad_all[i]*gscale;
        m->adam_m[i]=b1*m->adam_m[i]+(1-b1)*g;
        m->adam_v[i]=b2*m->adam_v[i]+(1-b2)*g*g;
        float mhat=m->adam_m[i]/bc1, vhat=m->adam_v[i]/bc2;
        m->param_all[i]-= lr*mhat/(sqrtf(vhat)+eps);
    }
    /* scatter back */
    off=0; for(int i=0;i<nl;i++){ ConvLayer3 L=convnet3_layer(m->conv,i); memcpy(L.param,m->param_all+off,L.n*sizeof(float)); off+=L.n; }
    convnet3_sgd_cbam(m->conv, lr);
    memcpy(seq_param(m), m->param_all+m->rnn_off, m->nparams_rnn*sizeof(float));
    memcpy(m->Wout, m->param_all+m->head_off, (size_t)m->C*m->rnn_out*sizeof(float));
    memcpy(m->bout, m->param_all+m->head_off+(size_t)m->C*m->rnn_out, m->C*sizeof(float));
    /* zero grads */
    convnet3_zero_grad(m->conv); seq_zero_grad(m);
    memset(m->gWout,0,(size_t)m->C*m->rnn_out*sizeof(float)); memset(m->gbout,0,m->C*sizeof(float));
}
