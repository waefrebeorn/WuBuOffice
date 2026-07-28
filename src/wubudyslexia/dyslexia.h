/* dyslexia.h -- dyslexia-friendly mode (UXA-52): a toggle that selects a
 * high-legibility rendering profile (wider spacing + a bundled friendly face
 * name). The renderer/font picker consume it. Opaque. */
#ifndef WUBUDYSLEXIA_H
#define WUBUDYSLEXIA_H

typedef struct Dyslexia Dyslexia;

Dyslexia *dyslexia_create(void);
void      dyslexia_destroy(Dyslexia *d);

void  dyslexia_set_enabled(Dyslexia *d, int on);
int   dyslexia_enabled(const Dyslexia *d);

/* friendly face name (e.g. "OpenDyslexic" if installed; falls back to the
 * current family). Stored so the font picker can apply it. */
void  dyslexia_set_face(Dyslexia *d, const char *face);
const char *dyslexia_face(const Dyslexia *d);

/* spacing multiplier (1.0 = normal, 1.5 = wider). */
void  dyslexia_set_spacing(Dyslexia *d, float s);
float dyslexia_spacing(const Dyslexia *d);

#endif /* WUBUDYSLEXIA_H */
