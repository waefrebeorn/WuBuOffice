/* wubugridline.h — spreadsheet gridline visibility toggle. */
#ifndef WUBUGRIDLINE_H
#define WUBUGRIDLINE_H

typedef struct {
    int show;         /* whether gridlines are drawn */
    int color;        /* RGBA packed color (0xAARRGGBB) */
    int print;        /* whether gridlines print */
} wubugridline;

int wubugridline_init(wubugridline *g);
int wubugridline_toggle(wubugridline *g);
int wubugridline_set(wubugridline *g, int show, int print);

#endif
