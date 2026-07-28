/* xps.c -- minimal XPS (store-only ZIP + FixedPage XML). See xps.h.
 *
 * Store-only ZIP: each entry = local file header + raw bytes; trailer = central
 * directory + end-of-central-directory. No compression (method 0), which is a
 * valid, decoder-compliant XPS container. CRC-32 is computed per entry. */
#include "xps.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint32_t crc32_buf(const uint8_t *p, size_t n){
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i=0;i<n;i++){
        crc ^= p[i];
        for (int k=0;k<8;k++) crc = (crc>>1) ^ (0xEDB88320u & (-(int32_t)(crc&1)));
    }
    return ~crc;
}

static void put32(uint8_t *b, size_t *o, uint32_t v){ b[*o]=(uint8_t)v; b[*o+1]=(uint8_t)(v>>8); b[*o+2]=(uint8_t)(v>>16); b[*o+3]=(uint8_t)(v>>24); *o+=4; }
static void put16(uint8_t *b, size_t *o, uint32_t v){ b[*o]=(uint8_t)v; b[*o+1]=(uint8_t)(v>>8); *o+=2; }

int xps_build(const char *text, int W, int H, uint8_t **out, size_t *out_len){
    if (!text || !out || !out_len) return 0;
    /* Content types + FixedPage XML */
    char ct[256];  snprintf(ct,sizeof ct,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"fpage\" ContentType=\"application/vnd.ms-package.xps-fixedpage+xml\"/>"
        "</Types>");
    char page[1024]; snprintf(page,sizeof page,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
        "<FixedPage xmlns=\"http://schemas.microsoft.com/xps/2005/06\" "
        "Width=\"%d\" Height=\"%d\">"
        "<Glyphs FontUri=\"/Documents/1/Fonts/Default.ttf\" FontRenderingEmSize=\"16\" "
        "OriginX=\"48\" OriginY=\"64\" UnicodeString=\"%s\"/>"
        "</FixedPage>", W, H, text);

    const char *names[2] = { "[Content_Types].xml", "Documents/1/Pages/1.fpage" };
    const uint8_t *data[2] = { (const uint8_t*)ct, (const uint8_t*)page };
    size_t len[2] = { strlen(ct), strlen(page) };
    int n = 2;

    size_t cap = 4096, off = 0;
    uint8_t *b = malloc(cap);
    if (!b) return 0;
    #define NEED(k) do{ while(off+(k)>cap){ cap*=2; uint8_t*nb=realloc(b,cap); if(!nb){free(b);return 0;} b=nb; } }while(0)

    size_t cd_off[2]; uint32_t cd_crc[2]; uint32_t cd_size[2]; size_t cd_lho[2];
    for (int i=0;i<n;i++){
        uint32_t crc = crc32_buf(data[i], len[i]);
        cd_crc[i]=crc; cd_size[i]=(uint32_t)len[i]; cd_lho[i]=off;
        NEED(30 + strlen(names[i]) + len[i]);
        put32(b,&off,0x04034b50u);            /* local file header sig */
        put16(b,&off,20);                     /* version needed */
        put16(b,&off,0);                      /* flags */
        put16(b,&off,0);                      /* method = store */
        put16(b,&off,0); put16(b,&off,0);     /* time, date */
        put32(b,&off,crc);
        put32(b,&off,(uint32_t)len[i]);       /* compressed size */
        put32(b,&off,(uint32_t)len[i]);       /* uncompressed size */
        put16(b,&off,(uint32_t)strlen(names[i]));
        put16(b,&off,0);                      /* extra len */
        memcpy(b+off, names[i], strlen(names[i])); off+=strlen(names[i]);
        memcpy(b+off, data[i], len[i]); off+=len[i];
    }
    size_t cd_start = off;
    for (int i=0;i<n;i++){
        NEED(46 + strlen(names[i]));
        put32(b,&off,0x02014b50u);            /* central dir sig */
        put16(b,&off,20);                     /* version made by */
        put16(b,&off,20);                     /* version needed */
        put16(b,&off,0); put16(b,&off,0);     /* flags, method */
        put16(b,&off,0); put16(b,&off,0);     /* time, date */
        put32(b,&off,cd_crc[i]);
        put32(b,&off,cd_size[i]);
        put32(b,&off,cd_size[i]);
        put16(b,&off,(uint32_t)strlen(names[i]));
        put16(b,&off,0);                      /* extra */
        put16(b,&off,0);                      /* comment */
        put16(b,&off,0); put16(b,&off,0);     /* disk, internal attr */
        put32(b,&off,0);                      /* external attr */
        put32(b,&off,(uint32_t)cd_lho[i]);    /* local header offset */
        memcpy(b+off, names[i], strlen(names[i])); off+=strlen(names[i]);
    }
    size_t cd_size_total = off - cd_start;
    NEED(22);
    put32(b,&off,0x06054b50u);                /* end of central dir */
    put16(b,&off,0); put16(b,&off,0);         /* disk numbers */
    put16(b,&off,(uint16_t)n); put16(b,&off,(uint16_t)n);
    put32(b,&off,(uint32_t)cd_size_total);
    put32(b,&off,(uint32_t)cd_start);
    put16(b,&off,0);                          /* comment len */
    #undef NEED

    *out = b; *out_len = off;
    return 1;
}

int xps_write_file(const char *path, const char *text, int W, int H){
    uint8_t *buf; size_t len;
    if (!xps_build(text, W, H, &buf, &len)) return -1;
    FILE *f = fopen(path, "wb");
    if (!f){ free(buf); return -1; }
    size_t w = fwrite(buf, 1, len, f);
    fclose(f); free(buf);
    return w==len ? 0 : -1;
}
