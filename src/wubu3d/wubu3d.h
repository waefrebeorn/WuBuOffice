/* wubu3d.h — minimal 3D model primitive (mesh of vertices + indexed faces).
 * Sufficient for embedding simple 3D shapes in documents. */
#ifndef WUBU3D_H
#define WUBU3D_H
#include <stddef.h>

typedef struct wubu3d wubu3d;

typedef struct { float x, y, z; } wubu3d_vec;
typedef struct { unsigned a, b, c; } wubu3d_face;

wubu3d *wubu3d_create(void);
void wubu3d_destroy(wubu3d *m);

/* Add a vertex, returns its index or -1. */
int wubu3d_add_vertex(wubu3d *m, float x, float y, float z);
/* Add a triangular face referencing vertex indices. Returns 0 or -1. */
int wubu3d_add_face(wubu3d *m, unsigned a, unsigned b, unsigned c);

size_t wubu3d_vertex_count(const wubu3d *m);
size_t wubu3d_face_count(const wubu3d *m);
const wubu3d_vec *wubu3d_vertex(const wubu3d *m, size_t i);
const wubu3d_face *wubu3d_get_face(const wubu3d *m, size_t i);

/* Build a unit cube (8 verts, 12 tris). Returns 0. */
int wubu3d_make_cube(wubu3d *m);

#endif
