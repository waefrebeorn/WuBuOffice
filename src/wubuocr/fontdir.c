/* fontdir.c -- discover and load a whole directory of fonts (see fontdir.h).
 * Clean C11, self-contained. Uses POSIX dirent + wubufont. */
#include "fontdir.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */

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

size_t ocr_font_dir_load(const char *dir, Font ***out_fonts, uint8_t ***out_bufs,
                         char ***out_paths, size_t max) {
    if (!dir || !out_fonts || !out_bufs || !out_paths || max == 0) return 0;
    *out_fonts = NULL; *out_bufs = NULL; *out_paths = NULL;

    DIR *d = opendir(dir);
    if (!d) return 0;

    Font **fonts = malloc(max * sizeof *fonts);
    uint8_t **bufs = malloc(max * sizeof *bufs);
    char **paths = malloc(max * sizeof *paths);
    if (!fonts || !bufs || !paths) {
        free(fonts); free(bufs); free(paths);
        closedir(d);
        return 0;
    }

    size_t n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < max) {
        if (e->d_name[0] == '.') continue;
        if (!is_font_name(e->d_name)) continue;
        char *path = malloc(strlen(dir) + strlen(e->d_name) + 2);
        if (!path) continue;
        sprintf(path, "%s/%s", dir, e->d_name);
        size_t sz = 0;
        uint8_t *b = slurp(path, &sz);
        if (!b) { free(path); continue; }
        Font *fo = font_open(b, sz);
        if (!fo) { free(b); free(path); continue; }
        fonts[n] = fo; bufs[n] = b; paths[n] = path; n++;
    }
    closedir(d);

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
