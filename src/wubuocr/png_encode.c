/* png_encode.c -- minimal STORE-method PNG encoder (grayscale 8-bit). */
#include "png_encode.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t crc_tbl[256]; static int crc_ready=0;
static void crc_init(void){
    for(uint32_t n=0;n<256;n++){ uint32_t c=n; for(int k=0;k<8;k++) c=(c&1)?(0xEDB88320u^(c>>1)):(c>>1); crc_tbl[n]=c; }
    crc_ready=1;
}
static uint32_t crc32(const uint8_t*b,size_t n){ if(!crc_ready)crc_init(); uint32_t c=0xFFFFFFFFu; for(size_t i=0;i<n;i++)c=crc_tbl[(c^b[i])&0xFF]^(c>>8); return c^0xFFFFFFFFu; }
static uint32_t adler32(const uint8_t*b,size_t n){ uint32_t a=1,b2=0; for(size_t i=0;i<n;i++){a=(a+b[i])%65521;b2=(b2+a)%65521;} return (b2<<16)|a; }

static void put_be32(uint8_t*p, uint32_t v){ p[0]=(v>>24)&0xFF; p[1]=(v>>16)&0xFF; p[2]=(v>>8)&0xFF; p[3]=v&0xFF; }

/* Write one PNG chunk at *o: [len(4)][type(4)][data(len)][crc(4)]. Advances *o. */
static void write_chunk(uint8_t *buf, size_t *o, const char *type,
                        const uint8_t *data, size_t len){
    put_be32(buf+*o, (uint32_t)len);           *o+=4;
    memcpy(buf+*o, type, 4);                    *o+=4;
    if(len && data) memcpy(buf+*o, data, len);  *o+=len;
    /* CRC over type + data */
    uint32_t c = crc32(buf+*o-len-4, 4+len);
    put_be32(buf+*o, c);                        *o+=4;
}

int png_encode_gray(const uint8_t *gray, uint32_t W, uint32_t H, uint8_t **out, size_t *out_len){
    if(!out||!out_len) return -1;
    size_t stride = (size_t)W;                 /* 1 byte/pixel, gray */
    size_t raw = (size_t)H*(1+stride);          /* +filter byte per row */
    uint8_t *rawbuf = malloc(raw>0?raw:1); if(!rawbuf) return -1;
    for(uint32_t y=0;y<H;y++){
        rawbuf[(size_t)y*(1+stride)] = 0;       /* filter type 0 (none) */
        memcpy(rawbuf+(size_t)y*(1+stride)+1, gray+(size_t)y*stride, stride);
    }

    /* zlib STORE stream: split raw into <=65535-byte stored blocks. */
    size_t nblocks = (raw + 65534)/65535; if(nblocks==0) nblocks=1;
    size_t zlen = 2 + nblocks*5 + raw + 4;      /* zlib hdr + per-block(5) + data + adler */
    uint8_t *z = malloc(zlen); if(!z){ free(rawbuf); return -1; }
    size_t zo=0;
    z[zo++]=0x78; z[zo++]=0x01;                  /* zlib header */
    size_t rem=raw, ri=0;
    do {
        size_t blk = rem>65535? 65535 : rem;
        int final = (rem-blk)==0;
        z[zo++] = final?1:0;                     /* BFINAL, BTYPE=00 (stored) */
        uint16_t L=(uint16_t)blk, N=(uint16_t)~blk;
        z[zo++]=L&0xFF; z[zo++]=(L>>8)&0xFF; z[zo++]=N&0xFF; z[zo++]=(N>>8)&0xFF;
        memcpy(z+zo, rawbuf+ri, blk); zo+=blk; ri+=blk; rem-=blk;
    } while(rem>0);
    uint32_t ad = adler32(rawbuf, raw);
    put_be32(z+zo, ad); zo+=4;                   /* adler32 is big-endian in zlib */
    size_t idat_len = zo;

    /* assemble PNG: sig(8) + IHDR(12+13) + IDAT(12+idat_len) + IEND(12) */
    size_t total = 8 + (12+13) + (12+idat_len) + 12;
    uint8_t *buf = malloc(total); if(!buf){ free(rawbuf); free(z); return -1; }
    size_t o=0;
    const uint8_t sig[8]={137,80,78,71,13,10,26,10}; memcpy(buf, sig, 8); o+=8;

    uint8_t ihdr[13];
    put_be32(ihdr,   W);
    put_be32(ihdr+4, H);
    ihdr[8]=8; ihdr[9]=0; ihdr[10]=0; ihdr[11]=0; ihdr[12]=0; /* 8-bit gray, no interlace */
    write_chunk(buf, &o, "IHDR", ihdr, 13);
    write_chunk(buf, &o, "IDAT", z, idat_len);
    write_chunk(buf, &o, "IEND", NULL, 0);

    free(rawbuf); free(z);
    *out = buf; *out_len = o;
    return 0;
}
