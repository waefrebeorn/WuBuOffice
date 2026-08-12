#include "wubuencrypt.h"
#include <stdlib.h>
#include <string.h>

/* ---------------- SHA-256 ---------------- */
typedef struct { unsigned h[8]; unsigned long long len; unsigned char buf[64]; size_t buflen; } sha256;
static const unsigned K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static unsigned ror(unsigned x, int n){ return (x >> n) | (x << (32 - n)); }
static void sha256_block(sha256 *s, const unsigned char *p){
    unsigned w[64]; unsigned a,b,c,d,e,f,g,h;
    for (int i=0;i<16;i++) w[i]=(p[i*4]<<24)|(p[i*4+1]<<16)|(p[i*4+2]<<8)|p[i*4+3];
    for (int i=16;i<64;i++){ unsigned s0=ror(w[i-15],7)^ror(w[i-15],18)^(w[i-15]>>3);
        unsigned s1=ror(w[i-2],17)^ror(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
    a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
    for (int i=0;i<64;i++){ unsigned S1=ror(e,6)^ror(e,11)^ror(e,25);
        unsigned ch=(e&f)^(~e&g); unsigned t1=h+S1+ch+K[i]+w[i];
        unsigned S0=ror(a,2)^ror(a,13)^ror(a,22); unsigned maj=(a&b)^(a&c)^(b&c);
        unsigned t2=S0+maj; h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha256_init(sha256 *s){ s->h[0]=0x6a09e667;s->h[1]=0xbb67ae85;s->h[2]=0x3c6ef372;s->h[3]=0xa54ff53a;
    s->h[4]=0x510e527f;s->h[5]=0x9b05688c;s->h[6]=0x1f83d9ab;s->h[7]=0x5be0cd19;s->len=0;s->buflen=0; }
static void sha256_update(sha256 *s, const unsigned char *p, size_t n){
    s->len += n;
    while (n > 0){
        size_t take = 64 - s->buflen; if (take > n) take = n;
        memcpy(s->buf + s->buflen, p, take); s->buflen += take; p += take; n -= take;
        if (s->buflen == 64){ sha256_block(s, s->buf); s->buflen = 0; }
    }
}
static void sha256_final(sha256 *s, unsigned char out[32]){
    unsigned long long bits = s->len * 8;
    unsigned char pad = 0x80;
    sha256_update(s, &pad, 1);
    unsigned char z = 0;
    while (s->buflen != 56) sha256_update(s, &z, 1);
    unsigned char lb[8];
    for (int i=0;i<8;i++) lb[i]=(unsigned char)(bits >> (56 - i*8));
    sha256_update(s, lb, 8);
    for (int i=0;i<8;i++){ out[i*4]=(unsigned char)(s->h[i]>>24); out[i*4+1]=(unsigned char)(s->h[i]>>16);
        out[i*4+2]=(unsigned char)(s->h[i]>>8); out[i*4+3]=(unsigned char)s->h[i]; }
}

/* ---------------- AES-128 ---------------- */
static const unsigned char sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};
static const unsigned char invsbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d};
static unsigned char xtime(unsigned char x){ return (unsigned char)((x<<1) ^ ((x&0x80)?0x1b:0)); }
static unsigned char mul(unsigned char a, unsigned char b){ unsigned char r=0; while(b){ if(b&1) r^=a; a=xtime(a); b>>=1; } return r; }

typedef struct { unsigned char rk[176]; int rounds; } aes_ctx;
static void aes_expand(aes_ctx *ctx, const unsigned char key[16]){
    ctx->rounds = 10;
    memcpy(ctx->rk, key, 16);
    int rcon = 1;
    for (int i = 4; i < 44; i++){
        unsigned char t[4];
        memcpy(t, ctx->rk + (i-1)*4, 4);
        if (i % 4 == 0){
            unsigned char tmp = t[0]; t[0]=t[1]; t[1]=t[2]; t[2]=t[3]; t[3]=tmp; /* rotword */
            for (int j=0;j<4;j++) t[j]=sbox[t[j]];
            t[0] ^= (unsigned char)rcon; rcon = xtime((unsigned char)rcon);
        }
        for (int j=0;j<4;j++) ctx->rk[i*4+j] = ctx->rk[(i-4)*4+j] ^ t[j];
    }
}
static void addroundkey(unsigned char s[16], const unsigned char *rk){ for (int i=0;i<16;i++) s[i]^=rk[i]; }
static void subbytes(unsigned char s[16]){ for (int i=0;i<16;i++) s[i]=sbox[s[i]]; }
static void invsubbytes(unsigned char s[16]){ for (int i=0;i<16;i++) s[i]=invsbox[s[i]]; }
static void shiftrows(unsigned char s[16]){
    unsigned char t;
    t=s[1];s[1]=s[5];s[5]=s[9];s[9]=s[13];s[13]=t;
    t=s[2];s[2]=s[10];s[10]=t; t=s[6];s[6]=s[14];s[14]=t;
    t=s[15];s[15]=s[11];s[11]=s[7];s[7]=s[3];s[3]=t;
}
static void invshiftrows(unsigned char s[16]){
    unsigned char t;
    t=s[13];s[13]=s[9];s[9]=s[5];s[5]=s[1];s[1]=t;
    t=s[2];s[2]=s[10];s[10]=t; t=s[6];s[6]=s[14];s[14]=t;
    t=s[3];s[3]=s[7];s[7]=s[11];s[11]=s[15];s[15]=t;
}
static void mixcolumns(unsigned char s[16]){
    for (int c=0;c<4;c++){
        unsigned char *col = s + c*4;
        unsigned char a0=col[0],a1=col[1],a2=col[2],a3=col[3];
        col[0]=mul(a0,2)^mul(a1,3)^a2^a3;
        col[1]=a0^mul(a1,2)^mul(a2,3)^a3;
        col[2]=a0^a1^mul(a2,2)^mul(a3,3);
        col[3]=mul(a0,3)^a1^a2^mul(a3,2);
    }
}
static void invmixcolumns(unsigned char s[16]){
    for (int c=0;c<4;c++){
        unsigned char *col = s + c*4;
        unsigned char a0=col[0],a1=col[1],a2=col[2],a3=col[3];
        col[0]=mul(a0,14)^mul(a1,11)^mul(a2,13)^mul(a3,9);
        col[1]=mul(a0,9)^mul(a1,14)^mul(a2,11)^mul(a3,13);
        col[2]=mul(a0,13)^mul(a1,9)^mul(a2,14)^mul(a3,11);
        col[3]=mul(a0,11)^mul(a1,13)^mul(a2,9)^mul(a3,14);
    }
}
static void aes_encrypt_block(aes_ctx *ctx, unsigned char in[16]){
    addroundkey(in, ctx->rk);
    for (int r=1;r<=ctx->rounds;r++){
        subbytes(in); shiftrows(in);
        if (r<ctx->rounds) mixcolumns(in);
        addroundkey(in, ctx->rk + r*16);
    }
}
static void aes_decrypt_block(aes_ctx *ctx, unsigned char in[16]){
    addroundkey(in, ctx->rk + ctx->rounds*16);
    for (int r=ctx->rounds-1;r>=1;r--){
        invshiftrows(in); invsubbytes(in); addroundkey(in, ctx->rk + r*16);
        if (r<ctx->rounds) invmixcolumns(in);
    }
    invshiftrows(in); invsubbytes(in); addroundkey(in, ctx->rk);
}

/* ---------------- public API ---------------- */
int wubuencrypt_derive(const char *password, const unsigned char salt[16],
                       unsigned char key[16], unsigned char iv[16]){
    if (!password || !key || !iv) return -1;
    sha256 s; sha256_init(&s);
    sha256_update(&s, (const unsigned char*)password, strlen(password));
    if (salt) sha256_update(&s, salt, 16);
    unsigned char d[32]; sha256_final(&s, d);
    memcpy(key, d, 16);
    memcpy(iv, d + 16, 16);
    return 0;
}

int wubuencrypt_cbc(const unsigned char key[16], const unsigned char iv[16],
                    const unsigned char *in, unsigned char *out, size_t len){
    if (!key || !iv || !in || !out || len % 16 != 0) return -1;
    aes_ctx ctx; aes_expand(&ctx, key);
    unsigned char prev[16]; memcpy(prev, iv, 16);
    for (size_t off = 0; off < len; off += 16){
        unsigned char blk[16];
        for (int i=0;i<16;i++) blk[i] = in[off+i] ^ prev[i];
        aes_encrypt_block(&ctx, blk);
        memcpy(out + off, blk, 16);
        memcpy(prev, blk, 16);
    }
    return 0;
}

int wubuencrypt_decrypt_cbc(const unsigned char key[16], const unsigned char iv[16],
                            const unsigned char *in, unsigned char *out, size_t len){
    if (!key || !iv || !in || !out || len % 16 != 0) return -1;
    aes_ctx ctx; aes_expand(&ctx, key);
    unsigned char prev[16]; memcpy(prev, iv, 16);
    for (size_t off = 0; off < len; off += 16){
        unsigned char blk[16], cipher[16];
        memcpy(cipher, in + off, 16);
        memcpy(blk, cipher, 16);
        aes_decrypt_block(&ctx, blk);
        for (int i=0;i<16;i++) out[off+i] = blk[i] ^ prev[i];
        memcpy(prev, cipher, 16);
    }
    return 0;
}

size_t wubuencrypt_pkcs7_pad(const unsigned char *in, size_t n, unsigned char *out){
    size_t padlen = 16 - (n % 16);
    size_t total = n + padlen;
    memcpy(out, in, n);
    for (size_t i=n;i<total;i++) out[i] = (unsigned char)padlen;
    return total;
}

int wubuencrypt_pkcs7_unpad(const unsigned char *in, size_t padded_len, unsigned char *out){
    if (padded_len == 0 || padded_len % 16 != 0) return -1;
    unsigned char pad = in[padded_len - 1];
    if (pad < 1 || pad > 16) return -1;
    if (padded_len < pad) return -1;
    for (size_t i = padded_len - pad; i < padded_len; i++) if (in[i] != pad) return -1;
    size_t n = padded_len - pad;
    memcpy(out, in, n);
    return (int)n;
}

unsigned char *wubuencrypt_document(const char *password, const unsigned char *data, size_t n, size_t *outlen){
    if (!password || !data || !outlen) return NULL;
    unsigned char salt[16]; for (int i=0;i<16;i++) salt[i]=(unsigned char)(0x5a ^ (i*7) ^ (unsigned char)(n>>(i%8)));
    unsigned char key[16], iv[16];
    wubuencrypt_derive(password, salt, key, iv);
    size_t padded = n + 16 - (n % 16);
    unsigned char *pt = (unsigned char*)malloc(padded);
    if (!pt) return NULL;
    wubuencrypt_pkcs7_pad(data, n, pt);
    unsigned char *ct = (unsigned char*)malloc(16 + padded);
    if (!ct){ free(pt); return NULL; }
    memcpy(ct, salt, 16);
    wubuencrypt_cbc(key, iv, pt, ct + 16, padded);
    free(pt);
    *outlen = 16 + padded;
    return ct;
}

unsigned char *wubuencrypt_open(const char *password, const unsigned char *blob, size_t bloblen, size_t *outlen){
    if (!password || !blob || !outlen || bloblen < 32 || (bloblen - 16) % 16 != 0) return NULL;
    const unsigned char *salt = blob;
    size_t ctlen = bloblen - 16;
    unsigned char key[16], iv[16];
    wubuencrypt_derive(password, salt, key, iv);
    unsigned char *ct = (unsigned char*)malloc(ctlen);
    if (!ct) return NULL;
    unsigned char *pt = (unsigned char*)malloc(ctlen);
    if (!pt){ free(ct); return NULL; }
    memcpy(ct, blob + 16, ctlen);
    wubuencrypt_decrypt_cbc(key, iv, ct, pt, ctlen);
    free(ct);
    unsigned char *plain = (unsigned char*)malloc(ctlen);
    if (!plain){ free(pt); return NULL; }
    int n = wubuencrypt_pkcs7_unpad(pt, ctlen, plain);
    free(pt);
    if (n < 0){ free(plain); return NULL; }
    *outlen = (size_t)n;
    return plain;
}
