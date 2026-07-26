/* image2doc.c -- OFFICE LENS: any photo (JPEG/PNG) -> editable document with
 * spatial formatting.
 *
 *   image2doc IN.jpg|IN.png OUT.docx|OUT.md|OUT.odt|OUT.html|OUT.json
 *
 * Pipeline:
 *   JPEG/PNG  -> OcrImage (grayscale)
 *     JPEG: transcoded to PPM via ffmpeg (trust boundary: no C-side JPEG dep)
 *     PNG:    clean-room wubuimage decoder
 *   auto-quad detect -> lens_flatten (perspective un-warp + contrast stretch)
 *   CRNN line recognizer -> docmodel JSON (spatial: one paragraph per line,
 *                           lines grouped into a spatial table grid)
 *   docmodel JSON -> wubuconv (docx/md/odt/html)
 *
 * Model: LOAD=<model.crnn> (required). CHARS=... overrides the alphabet
 * (default = basic English). STRIP=20 (model slice height).
 */
#include "png.h"
#include "image.h"
#include "lens.h"
#include "imgops.h"
#include "pdfsearch.h"
#include "crnn.h"
#include "crnn_transcribe.h"
#include "lexicon.h"   /* Lexicon (optional LEX= post-correction) */
#include "conv_map.h"
#include "docfmt.h"   /* alternate OCR serializations (txt/tsv/csv/jsonl/latex/rtf/hocr/alto) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>     /* getpid, unlink */
#include <sys/wait.h>   /* pclose */
#include <sys/stat.h>   /* mkdir modes */
#include <pthread.h>    /* threaded batch (#99) */

/* Cloud-sync export hook (#90): after a document is written to `out`, mirror
 * it to a cloud store. Two mechanisms, both dependency-free:
 *   SYNC_DIR  - atomic copy of `out` into this directory (use a mounted
 *               Nextcloud/S3/WebDAV mount for true cloud sync).
 *   SYNC_CMD  - shell command with a single %s placeholder for the file path
 *               (e.g. "rclone copy %s remote:bucket" or "aws s3 cp %s s3://b/").
 * Either/both may be set. Failures are warned but non-fatal (the local file
 * already exists). */
static void sync_export(const char *out){
    if (!out || !out[0]) return;
    const char *d = getenv("SYNC_DIR");
    if (d && d[0]) {
        /* ensure the dir exists, then atomic copy (tmp + rename) */
        mkdir(d, 0755);
        char tmp[1024];
        snprintf(tmp, sizeof tmp, "%s/.sync_%d.tmp", d, (int)getpid());
        char cmd[1200];
        snprintf(cmd, sizeof cmd, "cp -- \"%s\" \"%s\" && mv -f -- \"%s\" \"%s/%s\"",
                 out, tmp, tmp, d, strrchr(out,'/') ? strrchr(out,'/')+1 : out);
        int rc = system(cmd);
        if (rc != 0) printf("warning: SYNC_DIR copy failed (%s)\n", d);
        else printf("synced %s -> %s\n", out, d);
    }
    const char *c = getenv("SYNC_CMD");
    if (c && c[0]) {
        char cmd[2048];
        int n = snprintf(cmd, sizeof cmd, c, out);
        if (n > 0 && n < (int)sizeof cmd) {
            int rc = system(cmd);
            if (rc != 0) printf("warning: SYNC_CMD failed\n");
            else printf("sync command run for %s\n", out);
        } else {
            printf("warning: SYNC_CMD too long\n");
        }
    }
}

#define LATIN_STRIP 20
#define LATIN_CHARSET "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?'\"-:;()@#&"

static uint8_t *readf(const char *p, size_t *n){
    FILE *f=fopen(p,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long s=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t *b=malloc(s?(size_t)s:1);
    if(!b){fclose(f);return NULL;}
    if(fread(b,1,(size_t)s,f)!=(size_t)s){fclose(f);free(b);return NULL;}
    fclose(f); *n=(size_t)s; return b;
}

/* Magic-byte sniff: JPEG (FFD8), PNG (89504E47), Netpbm (P1..P6). */
static int is_jpeg(const uint8_t *d){ return d[0]==0xFF && d[1]==0xD8; }
static int is_png(const uint8_t *d){ return d[0]==0x89 && d[1]=='P' && d[2]=='N' && d[3]=='G'; }

/* Transcode a JPEG (or any ffmpeg-readable image) to a PGM blob via ffmpeg,
 * returned malloc'd (caller frees). Returns NULL on failure. This is the
 * trust-boundary approach: the C core stays dependency-free; real photos are
 * transcoded outside it. */
static uint8_t *jpeg_to_pgm(const uint8_t *data, size_t len, size_t *out_len){
    /* write the JPEG to a temp file, ffmpeg to PGM on stdout */
    char tmppath[256]; snprintf(tmppath,sizeof tmppath,"/tmp/img2doc_%d.jpg", (int)getpid());
    FILE *tf=fopen(tmppath,"wb"); if(!tf) return NULL;
    fwrite(data,1,len,tf); fclose(tf);
    char cmd[512]; snprintf(cmd,sizeof cmd,
        "ffmpeg -y -i %s -autorotate -f image2pipe -vcodec pgm - 2>/dev/null", tmppath);
    FILE *pp=popen(cmd,"r"); if(!pp) return NULL;
    size_t cap=65536, used=0; uint8_t *buf=malloc(cap);
    if(!buf){pclose(pp);return NULL;}
    size_t r; uint8_t tmp[8192];
    while((r=fread(tmp,1,sizeof tmp,pp))>0){
        if(used+r+1>cap){ while(used+r+1>cap) cap*=2; buf=realloc(buf,cap); if(!buf){pclose(pp);return NULL;} }
        memcpy(buf+used,tmp,r); used+=r;
    }
    pclose(pp); unlink(tmppath);
    buf[used]=0; *out_len=used; return buf;
}

/* Otsu threshold on a grayscale image (returns 0-255). */
static uint8_t otsu(const OcrImage *im){
    size_t hist[256]; memset(hist,0,sizeof hist);
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) hist[ocr_image_get(im,(size_t)x,(size_t)y)]++;
    size_t total=(size_t)W*H; double sum=0;
    for(int i=0;i<256;i++) sum+=i*hist[i];
    double sumB=0, wB=0, wF=0, max=0; int T=127;
    for(int t=0;t<255;t++){
        wB+=hist[t]; if(wB==0)continue;
        wF=total-wB; if(wF==0)break;
        sumB+=t*hist[t];
        double mB=sumB/wB, mF=(sum-sumB)/wF;
        double between=wB*wF*(mB-mF)*(mB-mF);
        if(between>max){max=between;T=t;}
    }
    return (uint8_t)T;
}

/* Auto-detect the 4 document corners from a photo of a page.
 * Robust method: binarize (Otsu, polarity-aware so a light page on a darker
 * background reads as foreground), find the centroid of foreground pixels, then
 * take the 4 foreground pixels that are extreme along each diagonal direction
 * (TL:-x-y, TR:+x-y, BR:+x+y, BL:-x+y). This yields the true quadrilateral
 * corners even when the page is rotated/slanted in the photo, unlike the naive
 * axis-aligned bounding box. Returns 1 on success. */
static int detect_quad(const OcrImage *im, Pt2 corners[4]){
    int W=(int)ocr_image_width(im), H=(int)ocr_image_height(im);
    uint8_t T=otsu(im);
    /* polarity: a photographed document is usually a LIGHT page. Decide fg by
     * which side of the threshold has more pixels (the page dominates area). */
    long lo=0, hi=0;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        if(ocr_image_get(im,(size_t)x,(size_t)y)<T) lo++; else hi++;
    }
    int page_is_light = hi >= lo;   /* foreground = pixels on the page side */
    /* accumulate centroid of foreground */
    long sx=0, sy=0, n=0;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int v=ocr_image_get(im,(size_t)x,(size_t)y);
        int fg = page_is_light ? (v>=T) : (v<T);
        if(fg){ sx+=x; sy+=y; n++; }
    }
    if(n < (long)W*2) return 0;   /* too little foreground -> not a page */
    double cx=(double)sx/n, cy=(double)sy/n;
    /* 4 diagonal directions; track the pixel maximizing the projection */
    double best[4]; Pt2 c[4];
    for(int k=0;k<4;k++){ best[k]=-1e18; c[k]=(Pt2){0,0}; }
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int v=ocr_image_get(im,(size_t)x,(size_t)y);
        int fg = page_is_light ? (v>=T) : (v<T);
        if(!fg) continue;
        double dx=x-cx, dy=y-cy;
        /* TL,TR,BR,BL maximizing (+-dx)+-dy */
        double proj[4] = { -dx-dy, dx-dy, dx+dy, -dx+dy };
        for(int k=0;k<4;k++) if(proj[k]>best[k]){ best[k]=proj[k]; c[k]=(Pt2){x,y}; }
    }
    /* require the four corners to be spread out (degenerate page guard) */
    if(best[0] < 0 || best[1] < 0 || best[2] < 0 || best[3] < 0) return 0;
    corners[0]=c[0]; corners[1]=c[1]; corners[2]=c[2]; corners[3]=c[3];
    return 1;
}

/* Spatial docmodel: the docmodel JSON from crnn_transcribe_page_json already
 * preserves reading order (one paragraph per line). For true column/layout
 * preservation, crnn_transcribe_page_json would need to run XY-cut first; that
 * is a future enhancement. For now, line-order paragraphs are the spatial
 * structure. */

/* Per-page pipeline: load one image, apply the shared preprocessing flags,
 * transcribe it with the (shared, read-only) model, and write the requested
 * output format + run the cloud-sync hook. One job in the batch. Returns 0 on
 * success, 1 on failure. Isolated so the threaded batch (#99) runs it per
 * page in its own thread (model + lexicon are shared read-only; each page
 * gets its own image load + transcription). */
typedef struct {
    double rotate_deg; int no_deskew, do_contrast, do_median, do_shading, do_sharpen;
} preproc_t;

static int do_page(CRNN *m, const char *charset, const preproc_t *pp,
                   Lexicon *lex, const char *in, const char *out, const char *outext) {
    /* load image (JPEG via ffmpeg, PNG via decoder, or Netpbm) */
    size_t pn=0; uint8_t *pbuf=readf(in,&pn);
    if(!pbuf){ printf("cannot read %s\n",in); return 1; }
    OcrImage *page=NULL;
    if(is_jpeg(pbuf)){
        size_t plen=0; uint8_t *ppm=jpeg_to_pgm(pbuf,pn,&plen);
        free(pbuf);
        if(!ppm){ printf("JPEG transcode failed (ffmpeg?): %s\n",in); return 1; }
        page=ocr_image_from_netpbm(ppm,plen);
        free(ppm);
    } else if(is_png(pbuf)){
        int interlaced=0;
        page=ocr_image_from_png(pbuf,pn,&interlaced);
        free(pbuf);
        if(interlaced){ printf("rejecting interlaced (Adam7) PNG: %s\n",in); return 1; }
    } else {
        page=ocr_image_from_netpbm(pbuf,pn);
        free(pbuf);
    }
    if(!page){ printf("image decode failed (JPEG/PNG/Netpbm): %s\n",in); return 1; }

    /* optional preprocessing ops (clean-room) */
    if(pp->rotate_deg!=0){
        OcrImage *r=ocr_image_rotate(page,pp->rotate_deg,128);
        if(r){ ocr_image_free(page); page=r; }
    }
    if(pp->do_shading){
        OcrImage *s=ocr_image_shading_correct(page,16);
        if(s){ ocr_image_free(page); page=s; }
    }
    if(pp->do_contrast){
        OcrImage *c=ocr_image_contrast_stretch(page,2,98);
        if(c){ ocr_image_free(page); page=c; }
    }
    if(pp->do_median){
        OcrImage *me=ocr_image_median(page,1);
        if(me){ ocr_image_free(page); page=me; }
    }
    if(pp->do_sharpen){
        OcrImage *sp=ocr_image_sharpen(page,1,1.0);
        if(sp){ ocr_image_free(page); page=sp; }
    }
    if(pp->no_deskew){ setenv("DESKEW","0",1); }

    /* auto document corner detection + perspective flatten (opt-in, QUAD=1) */
    if(getenv("QUAD") && getenv("QUAD")[0]=='1'){
        Pt2 corners[4];
        if(detect_quad(page,corners)){
            OcrImage *flat=lens_flatten(page,corners,0,0,1);
            if(flat){ ocr_image_free(page); page=flat; }
        }
    }

    /* transcribe -> docmodel JSON */
    char *json=NULL;
    if(crnn_transcribe_page_json(m,page,LATIN_STRIP,charset,lex,&json)!=0 || !json){
        printf("transcription failed: %s\n", in); ocr_image_free(page); return 1;
    }

    /* output stage */
    int rc = 1;
    if (strcmp(outext, "pdf") == 0) {
        FILE *o = fopen(out, "wb");
        if (o) {
            if (wubuocr_write_searchable_pdf(page, json, o) == 0) rc = 0;
            fclose(o);
        }
        if (rc) printf("searchable pdf write failed: %s\n", out);
    } else {
        char *alt = NULL;
        if      (strcmp(outext,"json") ==0) alt=strdup(json);
        else if (strcmp(outext,"txt")  ==0) alt=docfmt_to_text(json);
        else if (strcmp(outext,"tsv")  ==0) alt=docfmt_to_tsv(json);
        else if (strcmp(outext,"csv")  ==0) alt=docfmt_to_csv(json);
        else if (strcmp(outext,"jsonl")==0) alt=docfmt_to_jsonl(json);
        else if (strcmp(outext,"latex")==0) alt=docfmt_to_latex(json);
        else if (strcmp(outext,"rtf")  ==0) alt=docfmt_to_rtf(json);
        else if (strcmp(outext,"hocr") ==0) alt=docfmt_to_hocr(json);
        else if (strcmp(outext,"alto") ==0) alt=docfmt_to_alto(json);
        else if (strcmp(outext,"tei")  ==0) alt=docfmt_to_tei(json);
        else if (strcmp(outext,"xlsx") ==0) {
            char *xbuf=NULL; size_t xlen=0;
            if (docfmt_to_xlsx(json,&xbuf,&xlen)==0 && xbuf){
                FILE *o=fopen(out,"wb");
                if(o){ fwrite(xbuf,1,xlen,o); fclose(o); free(xbuf); rc=0; }
            }
            if(rc) printf("xlsx write failed: %s\n",out);
        }
        if (alt) {
            FILE *o=fopen(out,"wb");
            if(o){ fwrite(alt,1,strlen(alt),o); fclose(o); free(alt); rc=0; }
            else { free(alt); printf("cannot write %s\n",out); }
        }
        if (rc && !alt) {
            /* JSON -> editable document (docx/md/odt/html) via wubuconv */
            uint8_t *blob=NULL; size_t blen=0;
            if (wubuconv_convert_mem((const uint8_t*)json,strlen(json),"json",outext,&blob,&blen)==0 && blob){
                FILE *o=fopen(out,"wb");
                if(o){ fwrite(blob,1,blen,o); fclose(o); free(blob); rc=0; }
                else { free(blob); printf("cannot write %s\n",out); }
            } else printf("document conversion failed (json -> %s)\n",outext);
        }
    }
    if (rc==0) {
        printf("wrote %s from %s via CRNN line recognizer\n", out, in);
        sync_export(out);
    }
    ocr_image_free(page);
    free(json);
    return rc;
}

/* #99: threaded page batch. Worker processes a contiguous slice of the job
 * list. The model + lexicon are shared (read-only); each page does its own
 * load + transcribe + output inside do_page. */
typedef struct { CRNN *m; const char *charset; const preproc_t *pp;
                 Lexicon *lex; const char **pos; int lo, hi; int *fail; } batch_arg_t;
static void *batch_worker(void *p) {
    batch_arg_t *b = (batch_arg_t*)p;
    for (int j = b->lo; j < b->hi; j++) {
        const char *in = b->pos[2*j], *out = b->pos[2*j+1];
        const char *outext = strrchr(out, '.');
        if (!outext || !outext[1]) { printf("output needs an extension: %s\n", out); *(b->fail)=1; continue; }
        outext++;
        if (do_page(b->m, b->charset, b->pp, b->lex, in, out, outext) != 0) *(b->fail)=1;
    }
    return NULL;
}

int image2doc_main(int argc, char **argv){
    if(argc<3){
        printf("usage: LOAD=<model.crnn> %s IN.jpg|IN.png|IN.pgm OUT.docx|OUT.md|OUT.odt|OUT.html|OUT.json\n",argv[0]);
        return 1;
    }
    const char *LOAD=getenv("LOAD");
    if(!LOAD){ printf("set LOAD=<trained .crnn>\n"); return 1; }
    const char *CHARS=getenv("CHARS");
    const char *charset=CHARS?CHARS:LATIN_CHARSET;

    /* --- optional CLI flags --- */
    double rotate_deg=0; int no_deskew=0, do_contrast=0, do_median=0, do_shading=0, do_sharpen=0;
    int batch=1;   /* #99: threaded page batch (default 1 = serial, same as before) */
    const char *pos[256]; int np=0;
    for(int a=1;a<argc;a++){
        if(!strcmp(argv[a],"--rotate") && a+1<argc){ rotate_deg=atof(argv[++a]); }
        else if(!strcmp(argv[a],"--no-deskew")){ no_deskew=1; }
        else if(!strcmp(argv[a],"--contrast")){ do_contrast=1; }
        else if(!strcmp(argv[a],"--median")){ do_median=1; }
        else if(!strcmp(argv[a],"--shading")){ do_shading=1; }
        else if(!strcmp(argv[a],"--sharpen")){ do_sharpen=1; }
        else if(!strcmp(argv[a],"--batch") && a+1<argc){ batch=atoi(argv[++a]); if(batch<1) batch=1; }
        else if(np<256) pos[np++]=argv[a];
    }
    if(np<2){ printf("usage: LOAD=<model.crnn> %s [--rotate DEG] [--no-deskew] [--contrast] [--median] [--shading] [--sharpen] [--batch N] IN1 OUT1 [IN2 OUT2 ...]\\n",argv[0]); return 1; }
    if(np % 2 != 0){ printf("batch needs matched IN/OUT pairs\\n"); return 1; }
    if(batch>np/2) batch=np/2;   /* don't spawn more threads than jobs */

    /* --- load model once (shared, read-only across all pages in a batch) --- */
    CRNN *m=NULL;
    if(!crnn_load(LOAD,&m)||!m){ printf("crnn_load failed: %s\n",LOAD); return 1; }

    /* --- optional lexicon for beam post-correction (LEX=wordlist.txt), shared --- */
    Lexicon *lex=NULL;
    if(getenv("LEX") && getenv("LEX")[0]){
        lex = lex_load(getenv("LEX"), 2000000);
        if(!lex) printf("warning: lex_load failed: %s (continuing without)\n", getenv("LEX"));
    }

    /* shared preprocessing flags for every page in the batch */
    preproc_t pp = { rotate_deg, no_deskew, do_contrast, do_median, do_shading, do_sharpen };

    /* --- batch output stage (#99) ---
     * Each page does its own load + transcribe + output inside do_page; the
     * model + lexicon are shared (read-only). A single IN/OUT pair runs once
     * (identical to the old serial behaviour); --batch N splits the job list
     * across N pthreads. */
    int njobs = np / 2;
    int fail = 0;
    if (batch <= 1) {
        for (int j = 0; j < njobs; j++) {
            const char *in = pos[2*j], *out = pos[2*j+1];
            const char *outext = strrchr(out, '.');
            if (!outext || !outext[1]) { printf("output needs an extension: %s\n", out); fail=1; continue; }
            outext++;
            if (do_page(m, charset, &pp, lex, in, out, outext) != 0) fail = 1;
        }
    } else {
        pthread_t th[256];
        batch_arg_t ba[256];
        int per = (njobs + batch - 1) / batch;
        int nthreads = 0;
        for (int t = 0; t < batch && t*per < njobs; t++) {
            int lo = t*per, hi = lo+per; if (hi > njobs) hi = njobs;
            ba[t].m = m; ba[t].charset = charset; ba[t].pp = &pp; ba[t].lex = lex;
            ba[t].pos = pos; ba[t].lo = lo; ba[t].hi = hi; ba[t].fail = &fail;
            if (pthread_create(&th[t], NULL, batch_worker, &ba[t]) == 0) nthreads++;
            else { /* fall back to inline if thread spawn fails */
                for (int j = lo; j < hi; j++) {
                    const char *in = pos[2*j], *out = pos[2*j+1];
                    const char *outext = strrchr(out, '.');
                    if (!outext || !outext[1]) { printf("output needs an extension: %s\n", out); fail=1; continue; }
                    outext++;
                    if (do_page(m, charset, &pp, lex, in, out, outext) != 0) fail = 1;
                }
            }
        }
        for (int t = 0; t < nthreads; t++) pthread_join(th[t], NULL);
    }

    if(lex) lex_free(lex);
    crnn_free(m);
    return fail ? 1 : 0;
}

#ifndef WUBEOFFICE_EMBED
int main(int argc,char**argv){ return image2doc_main(argc,argv); }
#endif
