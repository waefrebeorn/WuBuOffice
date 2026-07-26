/* gru_layout.c -- the ONE shared GRU weight-offset table.
 * Single source of truth for the flat GRU weight layout, used by gru.c
 * (CPU scalar), gru_gpu.c (CUDA matmul), and ocl_gru.c (OpenCL fallback)
 * so no two implementations can drift apart. See gru_layout.h.
 *
 * Layout order (per direction block):
 *   Wz, Wr, Wh, Uz, Ur, Uh, Bz, Br, Bh   (3 input, 3 hidden, 3 bias)
 * param[]/grad[] in every build index through this same table, so the
 * optimizer and the .crnn save/load are layout-stable.
 */
#include "gru_layout.h"

GRUOffs gru_offs(int H, int D){
    GRUOffs o;
    o.Wz=0;            o.Wr=o.Wz+H*D; o.Wh=o.Wr+H*D;
    o.Uz=o.Wh+H*D;     o.Ur=o.Uz+H*H; o.Uh=o.Ur+H*H;
    o.Bz=o.Uh+H*H;     o.Br=o.Bz+H;     o.Bh=o.Br+H;
    o.block=o.Bh+H;
    return o;
}
