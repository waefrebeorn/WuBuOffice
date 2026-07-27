/* wubuwordview -- offscreen wubumodel_doc -> PNG renderer.
 *
 * Makes the office suite VISIBLE: renders a real document page with
 * FreeType (paragraphs, headings by style), wubuspell red wavy squiggles
 * under misspellings, and an embedded wubuchart bar chart. Writes a viewable
 * PNG. No display needed.
 *
 * The actual rendering lives in src/wuburender (shared with the live SDL
 * window), so this app is just: build a doc -> render -> write PNG.
 *
 * Usage: wubuwordview [out.png]   (defaults to /tmp/wubuword_view.png)
 */
#include "wuburender.h"
#include "wubupng.h"
#include "model.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv){
    const char *out = (argc>1)? argv[1] : "/tmp/wubuword_view.png";

    Wurender *r = wurender_create();
    if (!r){ fprintf(stderr, "renderer init failed (no font?)\n"); return 1; }

    wubumodel_doc *d = wurender_sample_doc();
    int W=900, H=1200;
    unsigned char *rgba; int rw, rh;
    if (wurender_render_doc(r, d, W, H, &rgba, &rw, &rh) != 0){
        fprintf(stderr, "render failed\n");
        wubumodel_doc_destroy(d);
        wurender_destroy(r);
        return 1;
    }
    wubupng_write_file(out, WUBUPNG_RGBA, rgba, (uint32_t)rw, (uint32_t)rh);
    printf("rendered %s (%dx%d)\n", out, rw, rh);

    free(rgba);
    wubumodel_doc_destroy(d);
    wurender_destroy(r);
    return 0;
}
