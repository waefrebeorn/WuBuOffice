/* focus.h -- visible focus indicator config (UXA-51): color (RGBA) + ring
 * width in px, plus an enabled flag. Pure config; the renderer consumes it. */
#ifndef WUBUFOCUS_H
#define WUBUFOCUS_H

typedef struct Focus Focus;

Focus *focus_create(void);
void   focus_destroy(Focus *f);

void focus_set_color(Focus *f, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void focus_set_width(Focus *f, int px);
void focus_set_enabled(Focus *f, int on);

unsigned focus_color(const Focus *f);   /* 0xRRGGBBAA */
int      focus_width(const Focus *f);
int      focus_enabled(const Focus *f);

#endif /* WUBUFOCUS_H */
