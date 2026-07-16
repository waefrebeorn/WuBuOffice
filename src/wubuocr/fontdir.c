/* fontdir.c -- discover and load a whole directory tree of fonts (see
 * fontdir.h). Clean C11, self-contained. Uses POSIX dirent + wubufont.
 *
 * A real "huge font repository" (e.g. the Google Fonts corpus) is DEEPLY
 * nested: ofl/a/actor/.../Actor-Regular.ttf. So the loader recurses into
 * subdirectories (bounded by `max` fonts and a depth cap) rather than only
 * scanning one level -- that is what lets the bank study a large variety. */

#include "fontdir.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <sys/stat.h>  /* stat, S_ISDIR */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int is_font_name(const char *name) {
    size_t L = strlen(name);
    if (L < 5) return 0;
    const char *ext = name + L - 4;
    return strcasecmp(ext, ".ttf") == 0 ||
           strcasecmp(ext, ".otf") == 0 ||
           strcasecmp(ext, ".ttc") == 0;
}

static uint8_t *slurp(const char *path, size_t *out_n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *b = malloc((size_t)sz + 1);
    if (!b) { fclose(f); return NULL; }
    size_t rd = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[rd] = 0;
    *out_n = rd;
    return b;
}

/* Recursive collector. Stops early once `n` reaches `max` or depth hits 0.
 * Returns the number collected so far (always <= max). */
static size_t collect(const char *dir, Font ***fonts, uint8_t ***bufs,
                       char ***paths, size_t *n, size_t max, int depth) {
    if (depth <= 0 || *n >= max) return *n;
    DIR *d = opendir(dir);
    if (!d) return *n;

    struct dirent *e;
    while ((e = readdir(d)) != NULL && *n < max) {
        if (e->d_name[0] == '.') continue;
        /* build full path; skip if it would overflow (defensive) */
        char path[PATH_MAX];
        int wlen = snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (wlen < 0 || (size_t)wlen >= sizeof path) continue;

        /* directory -> recurse (depth bounded) */
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            collect(path, fonts, bufs, paths, n, max, depth - 1);
            continue;
        }
        if (!is_font_name(e->d_name)) continue;

        size_t sz = 0;
        uint8_t *b = slurp(path, &sz);
        if (!b) continue;
        Font *fo = font_open(b, sz);
        if (!fo) { free(b); continue; }
        (*fonts)[*n] = fo; (*bufs)[*n] = b;
        (*paths)[*n] = strdup(path);
        (*n)++;
    }
    closedir(d);
    return *n;
}

size_t ocr_font_dir_load(const char *dir, Font ***out_fonts, uint8_t ***out_bufs,
                         char ***out_paths, size_t max) {
    if (!dir || !out_fonts || !out_bufs || !out_paths || max == 0) return 0;
    *out_fonts = NULL; *out_bufs = NULL; *out_paths = NULL;

    Font **fonts = malloc(max * sizeof *fonts);
    uint8_t **bufs = malloc(max * sizeof *bufs);
    char **paths = malloc(max * sizeof *paths);
    if (!fonts || !bufs || !paths) {
        free(fonts); free(bufs); free(paths);
        return 0;
    }

    size_t n = 0;
    /* depth 32 is far deeper than any real font tree; bounds recursion
     * against symlink cycles without needing a visited-set. */
    collect(dir, &fonts, &bufs, &paths, &n, max, 32);

    if (n == 0) {
        free(fonts); free(bufs); free(paths);
        return 0;
    }
    *out_fonts = fonts; *out_bufs = bufs; *out_paths = paths;
    return n;
}

void ocr_font_dir_free(Font **fonts, uint8_t **bufs, char **paths, size_t count) {
    if (!fonts && !bufs && !paths) return;
    for (size_t i = 0; i < count; i++) {
        if (fonts) font_free(fonts[i]);
        if (bufs) free(bufs[i]);
        if (paths) free(paths[i]);
    }
    free(fonts); free(bufs); free(paths);
}
