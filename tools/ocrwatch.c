/* ocrwatch.c -- OCR watch-folder daemon.
 *
 * Polls a directory for new image files (jpg/png/pgm/bmp) and, for each,
 * runs the image2doc pipeline to emit a document of the chosen format
 * (default .docx). Dependency-free (POSIX opendir/readdir, no inotify,
 * so it runs anywhere). Already-processed files are tracked in a .seen
 * sidecar so restarts are safe.
 *
 *   build: cc ocrwatch.c -o ocrwatch
 *   run:   LOAD=m.crnn CHARS=.. ./ocrwatch IN_DIR [OUT_DIR] [EXT]
 *           EXT defaults to docx; use e.g. md/json/csv/...
 *
 * Clean-room C11. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>

static int ends(const char *s, const char *suf){
    size_t a=strlen(s), b=strlen(suf); if(a<b) return 0;
    return strcasecmp(s+a-b, suf)==0;
}
static int is_image(const char *n){
    return ends(n,".jpg")||ends(n,".jpeg")||ends(n,".png")||ends(n,".pgm")||ends(n,".pbm")||ends(n,".bmp")||ends(n,".tif")||ends(n,".tiff");
}
static void sleep_s(int s){ struct timespec ts={s,0}; nanosleep(&ts,NULL); }

int main(int argc, char **argv){
    if (argc<2){ printf("usage: LOAD=<m.crnn> CHARS=<cs> %s IN_DIR [OUT_DIR] [EXT]\n", argv[0]); return 1; }
    const char *indir=argv[1];
    const char *outdir=argc>2?argv[2]:indir;
    const char *ext=argc>3?argv[3]:"docx";
    const char *LOAD=getenv("LOAD"); const char *CHARS=getenv("CHARS");
    if (!LOAD||!CHARS){ printf("set LOAD and CHARS env\n"); return 1; }
    char seen[2048]; snprintf(seen,sizeof seen,"%s/.ocrwatch_seen", indir);

    printf("ocrwatch: watching %s -> %s/*.%s (LOAD=%s)\n", indir, outdir, ext, LOAD);
    fflush(stdout);

    for (;;){
        FILE *sf=fopen(seen,"a+"); if (sf) fclose(sf); /* ensure exists */
        DIR *d=opendir(indir); if (!d){ sleep_s(3); continue; }
        struct dirent *e;
        while ((e=readdir(d))!=NULL){
            const char *n=e->d_name;
            if (!is_image(n)) continue;
            char inpath[2048]; snprintf(inpath,sizeof inpath,"%s/%s", indir, n);
            char outpath[2048]; snprintf(outpath,sizeof outpath,"%s/%s.%s", outdir, n, ext);
            /* skip if output already exists */
            if (access(outpath,F_OK)==0) continue;
            printf("[%s] -> %s\n", n, outpath); fflush(stdout);
            /* spawn image2doc directly (fork+execl, no /bin/sh) */
            char bin[2048]; snprintf(bin,sizeof bin,"%s/build_cuda/image2doc",
                getenv("WUBDIR")?getenv("WUBDIR"):".");
            pid_t ocr=fork();
            if (ocr==0){
                setenv("LOAD",LOAD,1); setenv("CHARS",CHARS,1);
                execl(bin,"image2doc",inpath,outpath,(char*)NULL);
                _exit(127);
            }
            int st=0; if (ocr>0) while(waitpid(ocr,&st,0)<0 && errno==EINTR) {}
            if (ocr<=0 || !WIFEXITED(st) || WEXITSTATUS(st)!=0)
                printf("  ! image2doc failed for %s\n", n);
            else {
                FILE *a=fopen(seen,"a"); if(a){ fprintf(a,"%s\n",n); fclose(a); }
            }
        }
        closedir(d);
        sleep_s(2);
    }
    return 0;
}
