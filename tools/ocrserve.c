/* ocrserve.c -- minimal dependency-free OCR micro-service.
 *
 * Wraps the image2doc pipeline over HTTP so other tools / "Office Lens"-style
 * clients can POST an image and get back a document. No third-party libs:
 * it implements just enough of HTTP/1.1 to receive a POST and reply, then
 * shells out to the already-built `image2doc` binary (which loads the CRNN
 * model and does all the real work). This keeps the service process-model
 * simple and reuses every feature already built.
 *
 *   build: cc ocrserve.c -o ocrserve
 *   run:   LOAD=model.crnn CHARS=... ./ocrserve 8080
 *   POST  http://host:8080/ocr?out=docx   (body = JPEG/PNG/PGM bytes)
 *   ->     document bytes (Content-Type from out extension)
 *
 * Clean-room C11, POSIX sockets.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

static volatile int g_stop = 0;
static void on_sig(int s){ (void)s; g_stop = 1; }

static void http_reply(int fd, int code, const char *ctype, const char *body, size_t blen){
    char hdr[512];
    int hl = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        code, code==200?"OK":"Error", ctype, blen);
    (void)!write(fd, hdr, (size_t)hl);
    if (blen && body) (void)!write(fd, body, blen);
}

static const char *ctype_for(const char *ext){
    if (!ext || !*ext) return "application/octet-stream";
    if (!strcmp(ext,"docx")) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (!strcmp(ext,"odt"))  return "application/vnd.oasis.opendocument.text";
    if (!strcmp(ext,"html")) return "text/html; charset=utf-8";
    if (!strcmp(ext,"md"))   return "text/markdown; charset=utf-8";
    if (!strcmp(ext,"json")) return "application/json; charset=utf-8";
    if (!strcmp(ext,"csv"))  return "text/csv; charset=utf-8";
    if (!strcmp(ext,"tsv"))  return "text/tab-separated-values";
    if (!strcmp(ext,"txt"))  return "text/plain; charset=utf-8";
    if (!strcmp(ext,"jsonl"))return "application/x-ndjson; charset=utf-8";
    if (!strcmp(ext,"latex"))return "application/x-latex";
    if (!strcmp(ext,"rtf"))  return "application/rtf";
    if (!strcmp(ext,"hocr")) return "text/html; charset=utf-8";
    if (!strcmp(ext,"alto")) return "application/xml";
    return "application/octet-stream";
}

static int read_exact(int fd, char *buf, size_t n){
    size_t got=0;
    while (got<n){ ssize_t r=read(fd,buf+got,n-got); if(r<=0) return -1; got+=(size_t)r; }
    return 0;
}

/* extract value of key in a query string ("out=docx&x=1") into out */
static void get_query_val(const char *q, const char *key, char *out, size_t outsz){
    out[0]=0; if(!q) return;
    size_t kl=strlen(key);
    for (const char *p=q; ; p=strchr(p,'&')){
        if (p==q || p[-1]=='?'){
            const char *v=p+ (p==q?0:1);
            if (strncmp(v,key,kl)==0 && v[kl]=='='){
                const char *val=v+kl+1; const char *e=strchr(val,'&');
                size_t len=e?(size_t)(e-val):strlen(val);
                if (len>=outsz) len=outsz-1;
                memcpy(out,val,len); out[len]=0; return;
            }
        }
        if (!p) break; p++; /* step past '&' */
    }
}

static void handle(int fd){
    /* read request line + headers until blank line (\r\n\r\n or \n\n) */
    char hdr[16384]; size_t hn=0;
    for (;;){
        if (hn >= sizeof hdr -1) break;
        char c; if (read_exact(fd,&c,1)!=0) return;
        hdr[hn++]=c;
        /* end of headers: \r\n\r\n or \n\n */
        if (hn>=4 && hdr[hn-4]=='\r' && hdr[hn-3]=='\n' && hdr[hn-2]=='\r' && hdr[hn-1]=='\n') break;
        if (hn>=2 && hdr[hn-2]=='\n' && hdr[hn-1]=='\n') break;
    }
    hdr[hn]=0;
    if (hn==0) return;

    char method[16]={0}, path[1024]={0};
    sscanf(hdr,"%15s %1023s", method, path);
    char *q = strchr(path,'?'); char *query=NULL;
    if (q){ *q=0; query=q+1; }

    long clen=0; const char *cl=strstr(hdr,"Content-Length:");
    if (cl) clen=atol(cl+15);
    if (clen<0) clen=0;
    if (clen > 50L*1024*1024){ http_reply(fd,413,"text/plain","payload too large",19); return; }
    if (strcmp(method,"POST")!=0){ http_reply(fd,405,"text/plain","method not allowed",19); return; }

    char *body = clen? malloc((size_t)clen+1):NULL;
    if (clen && !body){ http_reply(fd,500,"text/plain","oom",3); return; }
    if (clen && read_exact(fd,body,(size_t)clen)!=0){ free(body); return; }
    if (body) body[clen]=0;

    char outext[32]; get_query_val(query,"out",outext,sizeof outext);
    if (!outext[0]) strcpy(outext,"docx");

    char inpath[256], outpath[256];
    snprintf(inpath,sizeof inpath,"/tmp/ocrserve_in_%d.bin",(int)getpid());
    snprintf(outpath,sizeof outpath,"/tmp/ocrserve_out_%d.%s",(int)getpid(),outext);
    FILE *fi=fopen(inpath,"wb");
    if (!fi){ free(body); http_reply(fd,500,"text/plain","cannot write temp",18); return; }
    if (body && clen) fwrite(body,1,(size_t)clen,fi); fclose(fi); free(body);

    FILE *dbg=fopen("/tmp/ocrserve_dbg.log","a");
    if(dbg){ fprintf(dbg,"req pid=%d clen=%ld infile=%d outext=%s\n",(int)getpid(),clen,access(inpath,F_OK)==0?1:0,outext); fclose(dbg); }

    /* Spawn image2doc directly (fork + execl, no /bin/sh) so we avoid
     * system()'s nested fork, which the sandbox may block. LOAD/CHARS
     * are set in the environment (inherited by the execl'd child). */
    const char *LOAD=getenv("LOAD"); const char *CHARS=getenv("CHARS");
    if (LOAD) setenv("LOAD", LOAD, 1);
    if (CHARS) setenv("CHARS", CHARS, 1);
    const char *WD=getenv("WUBDIR")?getenv("WUBDIR"):".";
    char bin[2048]; snprintf(bin,sizeof bin,"%s/build_cuda/image2doc",WD);
    pid_t ocr=fork();
    if (ocr==0){
        execl(bin, "image2doc", inpath, outpath, (char*)NULL);
        _exit(127);
    }
    int st=0; if (ocr>0) while(waitpid(ocr,&st,0)<0 && errno==EINTR) {}
    unlink(inpath);
    if (ocr<=0 || !WIFEXITED(st) || WEXITSTATUS(st)!=0 || access(outpath,F_OK)!=0){
        unlink(outpath);
        http_reply(fd,500,"text/plain","ocr failed",10); return;
    }
    FILE *fo=fopen(outpath,"rb");
    if (!fo){ unlink(outpath); http_reply(fd,500,"text/plain","open out failed",15); return; }
    fseek(fo,0,SEEK_END); long sz=ftell(fo); fseek(fo,0,SEEK_SET);
    char *ob = malloc((size_t)sz+1);
    if (!ob){ fclose(fo); unlink(outpath); http_reply(fd,500,"text/plain","oom2",4); return; }
    size_t rd=fread(ob,1,(size_t)sz,fo); fclose(fo);
    http_reply(fd,200,ctype_for(outext),ob,rd);
    free(ob); unlink(outpath);
}

int main(int argc, char **argv){
    if (argc<2){ printf("usage: LOAD=<m.crnn> CHARS=<cs> %s PORT [WUBDIR]\n", argv[0]); return 1; }
    int port=atoi(argv[1]);
    if (argc>2 && argv[2][0]) setenv("WUBDIR",argv[2],1);
    signal(SIGINT,on_sig); signal(SIGTERM,on_sig); signal(SIGCHLD,SIG_IGN);

    int s=socket(AF_INET,SOCK_STREAM,0);
    if (s<0){ perror("socket"); return 1; }
    int one=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    struct sockaddr_in a; memset(&a,0,sizeof a);
    a.sin_family=AF_INET; a.sin_port=htons((uint16_t)port); a.sin_addr.s_addr=INADDR_ANY;
    if (bind(s,(struct sockaddr*)&a,sizeof a)<0){ perror("bind"); return 1; }
    if (listen(s,16)<0){ perror("listen"); return 1; }
    printf("ocrserve listening on :%d (LOAD=%s)\n", port, getenv("LOAD")?getenv("LOAD"):"(unset)");
    fflush(stdout);
    while (!g_stop){
        int c=accept(s,NULL,NULL);
        if (c<0) break;
        pid_t p=fork();
        if (p==0){ handle(c); close(c); _exit(0); }
        else if (p>0){ close(c); }
    }
    close(s);
    return 0;
}
