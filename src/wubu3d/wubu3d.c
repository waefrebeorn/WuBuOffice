#include "wubu3d.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

struct wubu3d {
    wubu3d_vec *verts; size_t nv, vcap;
    wubu3d_face *faces; size_t nf, fcap;
    wubu3d_vec *rot;    /* rotated snapshot, size == nv */
};

wubu3d *wubu3d_create(void){ return (wubu3d*)calloc(1,sizeof(wubu3d)); }
void wubu3d_destroy(wubu3d *m){
    if(!m) return;
    free(m->verts); free(m->faces); free(m->rot); free(m);
}

int wubu3d_add_vertex(wubu3d *m, float x, float y, float z){
    if (!m) return -1;
    if (m->nv == m->vcap){
        size_t nc = m->vcap?m->vcap*2:32;
        wubu3d_vec *nv = (wubu3d_vec*)realloc(m->verts, nc*sizeof(wubu3d_vec));
        if (!nv) return -1;
        m->verts = nv; m->vcap = nc;
        wubu3d_vec *nr = (wubu3d_vec*)realloc(m->rot, nc*sizeof(wubu3d_vec));
        if (!nr) return -1;
        m->rot = nr;
    }
    m->verts[m->nv].x=x; m->verts[m->nv].y=y; m->verts[m->nv].z=z;
    m->nv++;
    return (int)(m->nv-1);
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

int wubu3d_rotate(wubu3d *m, float ax, float ay){
    if (!m || m->nv==0) return -1;
    float cy = cosf(ay), sy = sinf(ay);
    float cx = cosf(ax), sx = sinf(ax);
    for (size_t i=0;i<m->nv;i++){
        float x = m->verts[i].x, y = m->verts[i].y, z = m->verts[i].z;
        /* rotate about Y */
        float x1 = x*cy + z*sy;
        float z1 = -x*sy + z*cy;
        /* rotate about X */
        float y2 = y*cx - z1*sx;
        float z2 = y*sx + z1*cx;
        m->rot[i].x = x1; m->rot[i].y = y2; m->rot[i].z = z2;
    }
    return 0;
}

int wubu3d_project(const wubu3d *m, int cx, int cy, float focal,
                   wubu3d_proj *out, size_t outn){
    if (!m || !out) return -1;
    if (outn < m->nv) return -1;
    const float cam_z = 3.0f;
    for (size_t i=0;i<m->nv;i++){
        float z = m->rot[i].z + cam_z;
        if (z <= 0.05f) z = 0.05f;     /* behind/clipping camera */
        float s = focal / z;
        out[i].x = cx + m->rot[i].x * s;
        out[i].y = cy - m->rot[i].y * s; /* screen y is down */
        out[i].z = z;
    }
    return 0;
}
