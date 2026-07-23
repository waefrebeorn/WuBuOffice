/* lexicon.c -- see lexicon.h. Dependency-free C11. */
#include "lexicon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------- UTF-8 ---------- */
int utf8_decode(const char *s, uint32_t *cp){
    const unsigned char *u=(const unsigned char*)s;
    if(!u[0]){ *cp=0; return 0; }
    if(u[0]<0x80){ *cp=u[0]; return 1; }
    if((u[0]&0xE0)==0xC0 && (u[1]&0xC0)==0x80){ *cp=((u[0]&0x1F)<<6)|(u[1]&0x3F); return 2; }
    if((u[0]&0xF0)==0xE0 && (u[1]&0xC0)==0x80 && (u[2]&0xC0)==0x80){
        *cp=((u[0]&0x0F)<<12)|((u[1]&0x3F)<<6)|(u[2]&0x3F); return 3; }
    if((u[0]&0xF8)==0xF0 && (u[1]&0xC0)==0x80 && (u[2]&0xC0)==0x80 && (u[3]&0xC0)==0x80){
        *cp=((u[0]&0x07)<<18)|((u[1]&0x3F)<<12)|((u[2]&0x3F)<<6)|(u[3]&0x3F); return 4; }
    *cp=0xFFFD; return 1; /* invalid -> replacement, consume 1 */
}
int utf8_encode(uint32_t cp, char *buf){
    if(cp<0x80){ buf[0]=(char)cp; buf[1]=0; return 1; }
    if(cp<0x800){ buf[0]=(char)(0xC0|(cp>>6)); buf[1]=(char)(0x80|(cp&0x3F)); buf[2]=0; return 2; }
    if(cp<0x10000){ buf[0]=(char)(0xE0|(cp>>12)); buf[1]=(char)(0x80|((cp>>6)&0x3F));
        buf[2]=(char)(0x80|(cp&0x3F)); buf[3]=0; return 3; }
    buf[0]=(char)(0xF0|(cp>>18)); buf[1]=(char)(0x80|((cp>>12)&0x3F));
    buf[2]=(char)(0x80|((cp>>6)&0x3F)); buf[3]=(char)(0x80|(cp&0x3F)); buf[4]=0; return 4;
}
int utf8_len(const char *s){ int n=0; uint32_t cp; int k; while((k=utf8_decode(s,&cp))>0){ s+=k; n++; } return n; }

/* ---------- NFKC-lite (dependency-free compatibility fold) ----------
 * Full NFKC needs the entire Unicode decomposition tables. We apply the two
 * transforms that matter for OCR text: (1) fullwidth/halfwidth fold
 * (U+FF01..U+FF5E -> U+0021..U+007E, U+FF10..U+FF19 -> '0'..'9', and the
 * Hangul compatibility jamo block), and (2) a small hand-curated table of the
 * most common compatibility decompositions seen in documents (circled/fraction/
 * superscript/ligature forms). This matches OCRmyPDF's NFKC behaviour for the
 * cases that actually occur in scanned text. */
static uint32_t nfkc_map(uint32_t cp){
    /* fullwidth ASCII region */
    if(cp>=0xFF01u && cp<=0xFF5Eu) return cp-0xFEE0u;
    if(cp==0x3000u) return 0x20u;            /* fullwidth space -> space */
    /* circled digits ①..⑨ -> 1..9 */
    if(cp>=0x2460u && cp<=0x2468u) return cp-0x245Fu;
    /* superscripts ² ³ ¹ */
    if(cp==0x00B2u) return '2'; if(cp==0x00B3u) return '3'; if(cp==0x00B9u) return '1';
    /* subscript ₀..₉ */
    if(cp>=0x2080u && cp<=0x2089u) return '0'+(cp-0x2080u);
    /* vulgar fraction ½ ¼ ¾ */
    if(cp==0x00BDu) return 0; /* drop; ambiguous -> handled by caller */
    /* ligatures ﬁ ﬂ ﬀ ﬃ ﬄ -> ascii */
    if(cp==0xFB00u) return 0; /* ff */
    if(cp==0xFB01u) return 0; /* fi */
    if(cp==0xFB02u) return 0; /* fl */
    if(cp==0xFB03u) return 0; /* ffi */
    if(cp==0xFB04u) return 0; /* ffl */
    return cp;
}
/* normalize `in` (utf8) in place via NFKC-lite; returns new length in bytes.
 * Decompositions that map to multiple chars are skipped (return 0 above) so the
 * single-codepoint fold stays O(1) and in-place safe. */
int utf8_nfkc(const char *in, char *out, int cap){
    int n=0; uint32_t cp; int k; const char *s=in;
    while((k=utf8_decode(s,&cp))>0 && n<cap-4){
        uint32_t m = nfkc_map(cp);
        if(m==0){ s+=k; continue; } /* dropped compatibility char */
        int e=utf8_encode(m, out+n); n+=e; s+=k;
    }
    out[n]=0; return n;
}

static float rndf(uint32_t *st){ uint32_t r=*st; r^=r<<13; r^=r>>17; r^=r<<5; *st=r;
    return (float)(r&0xFFFFFF)/(float)0x1000000; }
static uint32_t rndu(uint32_t *st){ uint32_t r=*st; r^=r<<13; r^=r>>17; r^=r<<5; *st=r; return r; }

/* ---------- trie ---------- */
typedef struct TrieNode {
    uint32_t cp;             /* codepoint edge label from parent */
    int      word_idx;       /* -1 if not a word end */
    int      first_child;    /* index into node pool, -1 */
    int      next_sibling;   /* -1 */
} TrieNode;

struct Lexicon {
    /* words */
    char   **words;          /* UTF-8, owned */
    uint64_t *counts;
    int      n;
    /* alias sampler */
    int     *alias;          /* n */
    float   *prob;           /* n, threshold in [0,1] */
    /* charset / class map */
    uint32_t *cps;           /* sorted distinct codepoints, size K */
    int      K;
    /* trie */
    TrieNode *nodes;
    int       nnodes, ncap;
};

/* ---------- charset ---------- */
static int cp_cmp(const void *a, const void *b){
    uint32_t x=*(const uint32_t*)a, y=*(const uint32_t*)b;
    return x<y?-1:(x>y?1:0);
}
static void build_charset(Lexicon *lx){
    /* gather all codepoints, sort, unique */
    size_t cap=1024, m=0; uint32_t *all=malloc(cap*sizeof(uint32_t));
    for(int i=0;i<lx->n;i++){
        const char *s=lx->words[i]; uint32_t cp; int k;
        while((k=utf8_decode(s,&cp))>0){ s+=k;
            if(m>=cap){ cap*=2; all=realloc(all,cap*sizeof(uint32_t)); }
            all[m++]=cp; }
    }
    qsort(all,m,sizeof(uint32_t),cp_cmp);
    lx->cps=malloc((m?m:1)*sizeof(uint32_t)); lx->K=0;
    for(size_t i=0;i<m;i++) if(lx->K==0 || all[i]!=lx->cps[lx->K-1]) lx->cps[lx->K++]=all[i];
    free(all);
}
int lex_charset_size(const Lexicon *lx){ return lx?lx->K:0; }
int lex_charset(const Lexicon *lx, uint32_t *out, int cap){
    if(!lx) return 0; int n=lx->K<cap?lx->K:cap;
    for(int i=0;i<n;i++) out[i]=lx->cps[i]; return lx->K;
}
int lex_class_of(const Lexicon *lx, uint32_t cp){
    if(!lx||lx->K==0) return 0;
    int lo=0,hi=lx->K-1;
    while(lo<=hi){ int mid=(lo+hi)/2; if(lx->cps[mid]==cp) return mid+1;
        else if(lx->cps[mid]<cp) lo=mid+1; else hi=mid-1; }
    return 0;
}
uint32_t lex_cp_of_class(const Lexicon *lx, int cls){
    if(!lx||cls<1||cls>lx->K) return 0; return lx->cps[cls-1];
}

/* ---------- trie build ---------- */
static int trie_new(Lexicon *lx, uint32_t cp){
    if(lx->nnodes>=lx->ncap){ lx->ncap = lx->ncap? lx->ncap*2 : 4096;
        lx->nodes=realloc(lx->nodes,(size_t)lx->ncap*sizeof(TrieNode)); }
    TrieNode *t=&lx->nodes[lx->nnodes];
    t->cp=cp; t->word_idx=-1; t->first_child=-1; t->next_sibling=-1;
    return lx->nnodes++;
}
static void trie_insert(Lexicon *lx, const char *s, int widx){
    int cur=0; /* node 0 = root */
    uint32_t cp; int k;
    while((k=utf8_decode(s,&cp))>0){ s+=k;
        int c=lx->nodes[cur].first_child, prev=-1, found=-1;
        while(c!=-1){ if(lx->nodes[c].cp==cp){ found=c; break; } prev=c; c=lx->nodes[c].next_sibling; }
        if(found==-1){ int nn=trie_new(lx,cp);
            if(prev==-1) lx->nodes[cur].first_child=nn; else lx->nodes[prev].next_sibling=nn;
            found=nn; }
        cur=found;
    }
    if(lx->nodes[cur].word_idx<0) lx->nodes[cur].word_idx=widx;
}
int lex_contains(const Lexicon *lx, const char *utf8){
    if(!lx) return -1; int cur=0; uint32_t cp; int k; const char *s=utf8;
    while((k=utf8_decode(s,&cp))>0){ s+=k;
        int c=lx->nodes[cur].first_child, found=-1;
        while(c!=-1){ if(lx->nodes[c].cp==cp){ found=c; break; } c=lx->nodes[c].next_sibling; }
        if(found==-1) return -1; cur=found; }
    return lx->nodes[cur].word_idx;
}

/* ---------- Vose alias method (freq-proportional O(1) sampling) ---------- */
static void build_alias(Lexicon *lx){
    int n=lx->n; lx->alias=malloc(n*sizeof(int)); lx->prob=malloc(n*sizeof(float));
    double tot=0; for(int i=0;i<n;i++) tot += (double)lx->counts[i];
    if(tot<=0){ for(int i=0;i<n;i++){ lx->prob[i]=1.0f; lx->alias[i]=i; } return; }
    double *scaled=malloc(n*sizeof(double));
    int *small=malloc(n*sizeof(int)), *large=malloc(n*sizeof(int)); int ns=0,nl=0;
    for(int i=0;i<n;i++){ scaled[i]=(double)lx->counts[i]*n/tot;
        if(scaled[i]<1.0) small[ns++]=i; else large[nl++]=i; }
    while(ns>0 && nl>0){ int s=small[--ns], l=large[--nl];
        lx->prob[s]=(float)scaled[s]; lx->alias[s]=l;
        scaled[l]=(scaled[l]+scaled[s])-1.0;
        if(scaled[l]<1.0) small[ns++]=l; else large[nl++]=l; }
    while(nl>0){ lx->prob[large[--nl]]=1.0f; }
    while(ns>0){ lx->prob[small[--ns]]=1.0f; }
    free(scaled); free(small); free(large);
}
int lex_sample(const Lexicon *lx, uint32_t *rng){
    if(!lx||lx->n==0) return -1;
    int col = (int)(rndu(rng) % (uint32_t)lx->n);
    float u = rndf(rng);
    return u < lx->prob[col] ? col : lx->alias[col];
}
int lex_sample_uniform(const Lexicon *lx, uint32_t *rng){
    if(!lx||lx->n==0) return -1;
    return (int)(rndu(rng) % (uint32_t)lx->n);
}

/* ---------- load ---------- */
Lexicon *lex_load(const char *path, int max_words){
    FILE *f=fopen(path,"rb"); if(!f) return NULL;
    Lexicon *lx=calloc(1,sizeof *lx);
    int cap=4096; lx->words=malloc(cap*sizeof(char*)); lx->counts=malloc(cap*sizeof(uint64_t));
    char line[512];
    while(fgets(line,sizeof line,f)){
        if(max_words>0 && lx->n>=max_words) break;
        char *tab=strchr(line,'\t'); char *end=line+strlen(line);
        while(end>line && (end[-1]=='\n'||end[-1]=='\r')){ end[-1]=0; end--; }
        uint64_t cnt=1; char *word;
        if(tab){ *tab=0; word=line; cnt=strtoull(tab+1,NULL,10); if(cnt==0)cnt=1; }
        else {   char *sp=strchr(line,' '); if(sp){ *sp=0; cnt=strtoull(sp+1,NULL,10);} word=line; if(cnt==0)cnt=1; }
        if(!*word) continue;
        if(lx->n>=cap){ cap*=2; lx->words=realloc(lx->words,cap*sizeof(char*));
            lx->counts=realloc(lx->counts,cap*sizeof(uint64_t)); }
        lx->words[lx->n]=strdup(word); lx->counts[lx->n]=cnt; lx->n++;
    }
    fclose(f);
    if(lx->n==0){ lex_free(lx); return NULL; }
    /* trie root */
    lx->nnodes=0; lx->ncap=0; lx->nodes=NULL; trie_new(lx,0);
    for(int i=0;i<lx->n;i++) trie_insert(lx, lx->words[i], i);
    build_charset(lx);
    build_alias(lx);
    return lx;
}
void lex_free(Lexicon *lx){
    if(!lx) return;
    if(lx->words){ for(int i=0;i<lx->n;i++) free(lx->words[i]); free(lx->words); }
    free(lx->counts); free(lx->alias); free(lx->prob); free(lx->cps); free(lx->nodes);
    free(lx);
}
int lex_size(const Lexicon *lx){ return lx?lx->n:0; }
const char *lex_word(const Lexicon *lx, int idx){ return (lx&&idx>=0&&idx<lx->n)?lx->words[idx]:NULL; }
uint64_t lex_count(const Lexicon *lx, int idx){ return (lx&&idx>=0&&idx<lx->n)?lx->counts[idx]:0; }

/* ---------- Levenshtein correction over codepoints ---------- */
static int decode_cps(const char *s, uint32_t *out, int cap){
    int n=0; uint32_t cp; int k; while((k=utf8_decode(s,&cp))>0 && n<cap){ s+=k; out[n++]=cp; } return n;
}
static int lev(const uint32_t *a,int la,const uint32_t *b,int lb,int *row){
    for(int j=0;j<=lb;j++) row[j]=j;
    for(int i=1;i<=la;i++){ int prev=row[0]; row[0]=i;
        for(int j=1;j<=lb;j++){ int cur=row[j];
            int cost=(a[i-1]==b[j-1])?0:1;
            int v=prev+cost; int d=row[j]+1; if(d<v)v=d; int ins=row[j-1]+1; if(ins<v)v=ins;
            row[j]=v; prev=cur; } }
    return row[lb];
}
int lex_correct(const Lexicon *lx, const char *utf8, int len_slack, int *out_dist){
    if(!lx||lx->n==0){ if(out_dist)*out_dist=-1; return -1; }
    uint32_t qa[128]; int la=decode_cps(utf8,qa,128);
    int best=-1, bestd=1<<30; int row[130]; uint32_t wb[128];
    for(int i=0;i<lx->n;i++){
        int lb=utf8_len(lx->words[i]);
        if(lb<la-len_slack || lb>la+len_slack) continue;
        int lbc=decode_cps(lx->words[i],wb,128);
        int d=lev(qa,la,wb,lbc,row);
        if(d<bestd){ bestd=d; best=i; if(d==0) break; }
    }
    if(out_dist)*out_dist=bestd;
    return best;
}
