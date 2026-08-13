/* wubu3d.h — minimal 3D model primitive (mesh of vertices + indexed faces)
 * plus a real software renderer core: perspective projection, model rotation,
 * and screen-space raster coordinates. Sufficient for embedding simple 3D
 * shapes (with depth) in documents. */
#ifndef WUBU3D_H
#define WUBU3D_H
#include <stddef.h>

typedef struct wubu3d wubu3d;

typedef struct { float x, y, z; } wubu3d_vec;
typedef struct { unsigned a, b, c; } wubu3d_face;

/* A vertex projected to normalized device coords (NDC) after the view
 * transform. z is the view-space depth (smaller = nearer). */
typedef struct { float x, y, z; } wubu3d_proj;

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

/* Rotate all vertices about the Y axis by `ay` radians and X axis by `ax`
 * radians (euler, applied Y then X), producing a transient rotated copy.
 * Returns 0 or -1 on alloc failure. The rotated snapshot is owned by the
 * model and overwritten on the next call. */
int wubu3d_rotate(wubu3d *m, float ax, float ay);

/* Perspective-project the rotated snapshot into `wubu3d_proj` for each vertex.
 * `cx,cy` is the screen center in pixels, `focal` the focal length in pixels
 * (e.g. width*0.9). View camera sits at z=+3 looking down -z. Caller passes a
 * wubu3d_proj array sized vertex_count(); returns 0 on success. */
int wubu3d_project(const wubu3d *m, int cx, int cy, float focal,
                   wubu3d_proj *out, size_t outn);

#endif
