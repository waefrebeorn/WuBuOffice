#include "wubu3d.h"
#include <stdlib.h>

struct wubu3d {
    wubu3d_vec *verts; size_t nv, vcap;
    wubu3d_face *faces; size_t nf, fcap;
};

wubu3d *wubu3d_create(void){ return (wubu3d*)calloc(1,sizeof(wubu3d)); }
void wubu3d_destroy(wubu3d *m){ if(!m) return; free(m->verts); free(m->faces); free(m); }

int wubu3d_add_vertex(wubu3d *m, float x, float y, float z){
    if (!m) return -1;
    if (m->nv == m->vcap){
        size_t nc = m->vcap?m->vcap*2:32;
        wubu3d_vec *nv = (wubu3d_vec*)realloc(m->verts, nc*sizeof(wubu3d_vec));
        if (!nv) return -1;
        m->verts = nv; m->vcap = nc;
    }
    m->verts[m->nv].x=x; m->verts[m->nv].y=y; m->verts[m->nv].z=z;
    return (int)(m->nv++);
}

int wubu3d_add_face(wubu3d *m, unsigned a, unsigned b, unsigned c){
    if (!m || a>=m->nv || b>=m->nv || c>=m->nv) return -1;
    if (m->nf == m->fcap){
        size_t nc = m->fcap?m->fcap*2:32;
        wubu3d_face *nf = (wubu3d_face*)realloc(m->faces, nc*sizeof(wubu3d_face));
        if (!nf) return -1;
        m->faces = nf; m->fcap = nc;
    }
    m->faces[m->nf].a=a; m->faces[m->nf].b=b; m->faces[m->nf].c=c;
    m->nf++;
    return 0;
}

size_t wubu3d_vertex_count(const wubu3d *m){ return m?m->nv:0; }
size_t wubu3d_face_count(const wubu3d *m){ return m?m->nf:0; }
const wubu3d_vec *wubu3d_vertex(const wubu3d *m, size_t i){ return (m&&i<m->nv)?&m->verts[i]:NULL; }
const wubu3d_face *wubu3d_get_face(const wubu3d *m, size_t i){ return (m&&i<m->nf)?&m->faces[i]:NULL; }

int wubu3d_make_cube(wubu3d *m){
    if (!m) return -1;
    static const float C = 0.5f;
    float v[8][3] = {
        {-C,-C,-C},{ C,-C,-C},{ C, C,-C},{-C, C,-C},
        {-C,-C, C},{ C,-C, C},{ C, C, C},{-C, C, C}};
    for (int i=0;i<8;i++) if (wubu3d_add_vertex(m, v[i][0],v[i][1],v[i][2])<0) return -1;
    static const unsigned f[12][3] = {
        {0,1,2},{0,2,3},{4,6,5},{4,7,6},{0,4,5},{0,5,1},
        {1,5,6},{1,6,2},{2,6,7},{2,7,3},{3,7,4},{3,4,0}};
    for (int i=0;i<12;i++) if (wubu3d_add_face(m, f[i][0],f[i][1],f[i][2])<0) return -1;
    return 0;
}
