/* wubuoffice.c -- unified launcher (UI parity: one name for the whole suite).
 *
 * The unified GUI shell is `wubuos`; `wubuoffice` is the user-facing
 * entry point that boots it with the same arguments. We resolve the sibling
 * `wubuos` binary next to this executable (install puts both in the same
 * bin dir) and exec it, so a single install serves both names. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv){
    /* path of this exe -> dir of sibling wubuos */
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (n < 0){ fprintf(stderr, "wubuoffice: cannot resolve self: %s\n", strerror(errno)); return 1; }
    path[n] = 0;
    char *slash = strrchr(path, '/');
    if (!slash){ fprintf(stderr, "wubuoffice: bad exe path\n"); return 1; }
    *(slash+1) = 0;                       /* keep the directory */
    size_t dl = strlen(path);
    const char *name = "wubuos";
    if (dl + strlen(name) + 1 > sizeof(path)){
        fprintf(stderr, "wubuoffice: path too long\n"); return 1;
    }
    strcat(path, name);

    /* build argv for the child (argv[0] -> wubuos) */
    char **child = malloc(sizeof(char*) * (size_t)(argc + 1));
    if (!child){ fprintf(stderr, "wubuoffice: oom\n"); return 1; }
    child[0] = path;
    for (int i = 1; i < argc; i++) child[i] = argv[i];
    child[argc] = NULL;

    execv(path, child);
    /* only reached on failure */
    fprintf(stderr, "wubuoffice: failed to launch %s: %s\n", path, strerror(errno));
    free(child);
    return 127;
}
