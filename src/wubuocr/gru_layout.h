/* gru_layout.h -- public GRU weight-offset table.
 * Shared by gru.c (CPU forward/backward) and ocl_gru.c (OpenCL fallback) so
 * both compute the SAME flat weight layout and never drift apart. Keep this in
 * sync with gru.c's layout contract:
 *   Order: Wz, Wr, Wh, Uz, Ur, Uh, Bz, Br, Bh  (3 input, 3 hidden, 3 bias).
 */
#ifndef WUBUOCR_GRU_LAYOUT_H
#define WUBUOCR_GRU_LAYOUT_H
#include <stddef.h>

typedef struct { int Wz,Wr,Wh,Uz,Ur,Uh,Bz,Br,Bh,block; } GRUOffs;

/* Per-direction block offsets for a GRU with hidden dim H, input dim D. */
GRUOffs gru_offs(int H, int D);

#endif
