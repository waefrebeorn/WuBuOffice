#include "wubu3d.h"
#include <stdio.h>
#include <stdlib.h>

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

    wubu3d_destroy(m);
    if (fails) { printf("FAILED (%d)\n", fails); return 1; }
    printf("PASS: wubu3d (mesh model: unit cube 8v/12f)\n");
    return 0;
}
