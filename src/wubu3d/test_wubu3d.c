#include "wubu3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int fails = 0;
#define CK(c,m) do{ if(!(c)){ fprintf(stderr,"[FAIL] %s\n",(m)); fails++; } }while(0)

int main(void){
    wubu3d *m = wubu3d_create();
    CK(wubu3d_make_cube(m) == 0, "make cube");
    CK(wubu3d_vertex_count(m) == 8, "8 verts");
    CK(wubu3d_face_count(m) == 12, "12 tris");
    const wubu3d_vec *v = wubu3d_vertex(m, 0);
    CK(v && v->x==-0.5f && v->z==-0.5f, "corner vertex");
    CK(wubu3d_add_face(m, 0,1,99) == -1, "reject out-of-range face");
    CK(wubu3d_vertex(m, 999) == NULL, "out of range vertex");

    /* REAL engine: rotate + project the cube; front/back verts must diverge
     * on screen (perspective), proving the renderer core actually runs. */
    CK(wubu3d_rotate(m, 0.4f, 0.7f) == 0, "rotate");
    size_t nv = wubu3d_vertex_count(m);
    wubu3d_proj *p = (wubu3d_proj*)malloc(sizeof(wubu3d_proj) * nv);
    CK(wubu3d_project(m, 320, 240, 320.0f, p, nv) == 0, "project");
    /* vertex 0 and vertex 7 are opposite corners; after rotation their
     * projected screen coords must differ. */
    CK(fabs(p[0].x - p[7].x) > 1.0f || fabs(p[0].y - p[7].y) > 1.0f,
       "opposite corners project to distinct screen positions");
    /* all projected z must be positive (in front of camera). */
    int allpos = 1;
    for (size_t i=0;i<nv;i++) if (p[i].z <= 0.0f) allpos = 0;
    CK(allpos, "all projected depth positive (in front of camera)");
    free(p);

    wubu3d_destroy(m);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubu3d (mesh model + perspective renderer core: rotate/project)\n");
    return 0;
}
