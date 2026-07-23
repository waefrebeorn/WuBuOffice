/* png.c -- clean-room PNG decoder (see png.h). */
#include "png.h"
#include "image.h"
#include "../wubuzip/inflate.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t crc32_table[256];
static int crc32_ready = 0;
static void crc32_init(void){
    for(uint32_t n=0;n<256;n++){
        uint32_t c=n;
        for(int k=0;k<8;k++) c = (c&1) ? (0xEDB88320u ^ (c>>1)) : (c>>1);
        crc32_table[n]=c;
    }
    crc32_ready=1;
}
static uint32_t crc32_buf(const uint8_t *buf, size_t len){
    if(!crc32_ready) crc32_init();
    uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<len;i++) c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}

/* PNG chunk: 4-byte len (big-endian), 4-byte type, data, 4-byte CRC. */
typedef struct { uint32_t len; char type[5]; const uint8_t *data; uint32_t crc; } Chunk;

static const uint8_t PNG_SIG[8] = {137,80,78,71,13,10,26,10};

/* unfilter a scanline row given previous row (both with 1 filter byte prefix). */
static void unfilter_row(const uint8_t *restrict cur, const uint8_t *restrict prev,
                         uint8_t *restrict out, int bpp, int w){
    uint8_t ft = cur[0];
    for(int x=0;x<w*bpp;x++){
        int a = (x>=bpp) ? out[x-bpp] : 0;
        int b = prev ? prev[x] : 0;
        int c = (x>=bpp && prev) ? prev[x-bpp] : 0;
        int v = cur[1+x];
        int r;
        switch(ft){
            case 0: r=v; break;
            case 1: r=v+a; break;
            case 2: r=v+b; break;
            case 3: r=v+((a+b)>>1); break;
            case 4: { int p=a+b-c; int pa=abs(p-a),pb=abs(p-b),pc=abs(p-c);
                      int pr = (pa<=pb && pa<=pc)?a:(pb<=pc?b:c); r=v+pr; break; }
            default: r=v; break;
        }
        out[x] = (uint8_t)(r & 0xFF);
    }
}

OcrImage *ocr_image_from_png(const uint8_t *data, size_t len, int *was_interlaced){
    if(was_interlaced) *was_interlaced=0;
    if(len < 8 || memcmp(data, PNG_SIG, 8)!=0) return NULL;
    /* parse IHDR */
    size_t off = 8;
    uint32_t W=0,H=0; int bitdepth=0, colortype=0, interlace=0;
    int have_ihdr=0;
    while(off + 8 <= len){
        uint32_t clen = ((uint32_t)data[off]<<24)|((uint32_t)data[off+1]<<16)|((uint32_t)data[off+2]<<8)|(uint32_t)data[off+3];
        if(off + 8 + clen + 4 > len) break;
        char type[5]; memcpy(type, data+off+4, 4); type[4]=0;
        const uint8_t *cdata = data+off+8;
        uint32_t crc = ((uint32_t)data[off+8+clen]<<24)|((uint32_t)data[off+8+clen+1]<<16)|((uint32_t)data[off+8+clen+2]<<8)|(uint32_t)data[off+8+clen+3];
        if(crc32_buf(data+off+4, 4+clen) != crc) return NULL; /* corrupt */
        if(memcmp(type,"IHDR",4)==0){
            W=((uint32_t)cdata[0]<<24)|((uint32_t)cdata[1]<<16)|((uint32_t)cdata[2]<<8)|(uint32_t)cdata[3];
            H=((uint32_t)cdata[4]<<24)|((uint32_t)cdata[5]<<16)|((uint32_t)cdata[6]<<8)|(uint32_t)cdata[7];
            bitdepth=cdata[8]; colortype=cdata[9]; interlace=cdata[12];
            have_ihdr=1;
        }
        if(memcmp(type,"IDAT",4)==0) break; /* IHDR done */
        off += 8 + clen + 4;
    }
    if(!have_ihdr || W==0 || H==0) return NULL;
    if(interlace){ if(was_interlaced)*was_interlaced=1; return NULL; }
    int channels;
    switch(colortype){
        case 0: channels=1; break;   /* gray */
        case 2: channels=3; break;   /* rgb */
        case 4: channels=2; break;   /* gray+alpha */
        case 6: channels=4; break;   /* rgba */
        default: return NULL;
    }
    if(bitdepth!=8 && bitdepth!=16) return NULL; /* we handle 8/16 */
    int bpp = channels * (bitdepth/8); /* bytes per pixel */

    /* collect IDAT chunks */
    size_t idat_cap=0, idat_len=0; uint8_t *idat=NULL;
    off=8;
    while(off+8<=len){
        uint32_t clen=(data[off]<<24)|(data[off+1]<<16)|(data[off+2]<<8)|data[off+3];
        if(off+8+clen+4>len) break;
        char type[5]; memcpy(type,data+off+4,4); type[4]=0;
        const uint8_t *cdata=data+off+8;
        if(memcmp(type,"IDAT",4)==0){
            if(idat_len+clen > idat_cap){
                size_t ncap = idat_cap? idat_cap*2 : 65536;
                while(ncap < idat_len+clen) ncap*=2;
                uint8_t *n = realloc(idat, ncap); if(!n){free(idat);return NULL;}
                idat=n; idat_cap=ncap;
            }
            memcpy(idat+idat_len, cdata, clen); idat_len+=clen;
        } else if(memcmp(type,"IEND",4)==0){ break; }
        off += 8+clen+4;
    }
    if(!idat || idat_len==0){ free(idat); return NULL; }

    uint8_t *raw=NULL; size_t raw_len=0;
    if(wubuzip_inflate(idat, idat_len, &raw, &raw_len, 1)!=0){ free(idat); free(raw); return NULL; }
    free(idat);

    /* raw = H rows of (1 filter byte + w*bpp bytes). */
    size_t stride = 1 + (size_t)W*bpp;
    if(raw_len < stride*(size_t)H){ free(raw); return NULL; }

    OcrImage *im = ocr_image_create(W,H);
    if(!im){ free(raw); return NULL; }
    uint8_t *dstplane = (uint8_t*)ocr_image_pixels(im);
    uint8_t *prev = NULL, *cur_out = malloc(stride>0?stride:1);
    if(!cur_out){ ocr_image_free(im); free(raw); return NULL; }
    for(uint32_t y=0;y<H;y++){
        const uint8_t *rawrow = raw + (size_t)y*stride;
        unfilter_row(rawrow, prev, cur_out, bpp, W);
        uint8_t *dst = dstplane + (size_t)y*W;
        for(uint32_t x=0;x<W;x++){
            const uint8_t *p = cur_out + x*bpp;
            int g;
            if(channels==1){ g = (bitdepth==16)? ((p[0]<<8)|p[1])>>8 : p[0]; }
            else if(channels==2){ /* gray+alpha: blend over white */
                int v = (bitdepth==16)? ((p[0]<<8)|p[1])>>8 : p[0];
                int a = (bitdepth==16)? ((p[2]<<8)|p[3])>>8 : p[2];
                g = v + ((255-a)*(255-v))/255;
            } else { /* rgb or rgba */
                int r=(bitdepth==16)?((p[0]<<8)|p[1])>>8:p[0];
                int gg=(bitdepth==16)?((p[2]<<8)|p[3])>>8:p[2];
                int b=(bitdepth==16)?((p[4]<<8)|p[5])>>8:p[4];
                int lum = (int)(0.299*r+0.587*gg+0.114*b);
                if(channels==4){ int a=(bitdepth==16)?((p[6]<<8)|p[7])>>8:p[6];
                    lum = lum + ((255-a)*(255-lum))/255; }
                g = lum;
            }
            dst[x] = (uint8_t)(g<0?0:(g>255?255:g));
        }
        prev = cur_out;
    }
    free(cur_out); free(raw);
    return im;
}
