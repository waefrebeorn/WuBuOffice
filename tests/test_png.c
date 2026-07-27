/* test_png.c -- validate the shared wubupng encoder independently.
 *
 * The encoder is proven correct against a third-party decoder (PIL reads its
 * RGBA/GRAY8 output byte-exact). Here we validate it WITHOUT depending on the
 * OCR grayscale decoder's RGBA path: we parse our own IHDR + inflate our own
 * IDAT with zlib, and assert the chunk framing / scanline layout are right.
 * GRAY8 is additionally round-tripped through ocr_image_from_png (its native
 * path) to confirm real-world decoders accept it.
 */
#include "wubupng.h"
#include "png.h"          /* ocr_image_from_png (GRAY8 native path) */
#include "image.h"       /* OcrImage */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int failures = 0;
#define CHECK(c, m) do { if(!(c)){ fprintf(stderr,"FAIL: %s\n", m); failures++; } } while(0)

/* Minimal self-decoder: read IHDR, inflate IDAT with zlib, sanity-check the
 * raw scanlines (filter byte 0, expected length). Returns malloc'd raw buffer
 * (caller frees) or NULL. */
static unsigned char *self_inflate(const uint8_t *data, size_t len,
                                   uint32_t *W, uint32_t *H, int *ct, size_t *rawlen){
    if (len < 8 || memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0) return NULL;
    size_t pos = 8;
    size_t idat_cap = 0, idat_len = 0;
    unsigned char *idat = NULL;
    while (pos + 8 <= len){
        uint32_t clen; memcpy(&clen, data+pos, 4); clen = __builtin_bswap32(clen);
        char type[5]; memcpy(type, data+pos+4, 4); type[4]=0;
        const unsigned char *cdata = data+pos+8;
        if (memcmp(type,"IHDR",4)==0){
            memcpy(W, cdata, 4); *W = __builtin_bswap32(*W);
            memcpy(H, cdata+4, 4); *H = __builtin_bswap32(*H);
            *ct = cdata[9];
        } else if (memcmp(type,"IDAT",4)==0){
            if (idat_len + clen > idat_cap){ size_t nc = idat_cap? idat_cap*2:4096;
                while (nc < idat_len + clen) nc *= 2;
                unsigned char *nb = realloc(idat, nc); if(!nb){ free(idat); return NULL; }
                idat = nb; idat_cap = nc; }
            memcpy(idat + idat_len, cdata, clen); idat_len += clen;
        } else if (memcmp(type,"IEND",4)==0){
            break;
        }
        pos += 12 + clen;
    }
    if (!idat || idat_len == 0) { free(idat); return NULL; }
    /* raw = H * (1 + W*bpp); bpp from colortype */
    int bpp = (*ct==6)?4 : (*ct==0)?1 : (*ct==2)?3 : 4;
    size_t need = (size_t)*H * (1 + (size_t)*W * bpp);
    unsigned char *raw = malloc(need ? need : 1);
    uLong rl = need;
    if (uncompress(raw, &rl, idat, idat_len) != Z_OK){ free(idat); free(raw); return NULL; }
    free(idat);
    *rawlen = rl;
    return raw;
}

static void test_rgba(void){
    const uint32_t W = 4, H = 2;
    unsigned char px[W*H*4];
    unsigned char *p = px;
    unsigned char src[8][3] = {
        {255,255,255},{0,0,0},{255,0,0},{0,255,0},
        {0,0,255},{128,128,128},{255,255,0},{255,0,255}};
    for (int i = 0; i < 8; i++){ *p++=src[i][0]; *p++=src[i][1]; *p++=src[i][2]; *p++=255; }
    uint8_t *buf; size_t len;
    CHECK(wubupng_encode(WUBUPNG_RGBA, px, W, H, &buf, &len) == 0, "encode rgba");

    uint32_t dW=0, dH=0; int ct=0; size_t rawlen=0;
    unsigned char *raw = self_inflate(buf, len, &dW, &dH, &ct, &rawlen);
    CHECK(raw != NULL, "self-inflate succeeds");
    if (raw){
        CHECK(dW==W && dH==H, "IHDR dimensions match");
        CHECK(ct==6, "colortype RGBA");
        size_t need = (size_t)H*(1+(size_t)W*4);
        CHECK(rawlen==need, "raw scanline length matches");
        /* filter bytes must all be 0 */
        int allzero=1; for (uint32_t y=0;y<H;y++) if (raw[(size_t)y*(1+(size_t)W*4)]!=0) allzero=0;
        CHECK(allzero, "every scanline filter byte is 0");
        /* pixel (0,0) red? src[0]=(255,255,255) -> white. Check last pixel magenta. */
        unsigned char *last = raw + (size_t)(H-1)*(1+(size_t)W*4) + 1 + ((size_t)W-1)*4;
        CHECK(last[0]==255 && last[1]==0 && last[2]==255, "last pixel magenta R,G,B");
        free(raw);
    }
    free(buf);
}

static void test_gray(void){
    const uint32_t W = 8, H = 1;
    unsigned char px[8] = {0, 32, 64, 96, 128, 160, 192, 224};
    uint8_t *buf; size_t len;
    CHECK(wubupng_encode(WUBUPNG_GRAY8, px, W, H, &buf, &len) == 0, "encode gray");

    int interlaced = -1;
    OcrImage *im = ocr_image_from_png(buf, len, &interlaced);
    CHECK(im != NULL, "decoder accepts our GRAY8 PNG");
    if (im){
        for (int x = 0; x < 8; x++){
            int v = ocr_image_get(im, x, 0);
            CHECK(v == px[x], "gray value preserved");
        }
        ocr_image_free(im);
    }
    free(buf);
}

int main(void){
    test_rgba();
    test_gray();
    if (failures == 0){ printf("png: all tests passed\n"); return 0; }
    printf("png: %d FAILURES\n", failures);
    return 1;
}
