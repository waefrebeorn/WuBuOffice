/* dft.h -- 2D DFT compression + spectral analysis for OCR glyph regions.
 *
 * "DFT-style compression and analysis to allow for very cheap" ingestion:
 * a glyph's tight bounding-box crop is transformed to the frequency domain
 * with a straightforward O(N^2 log N)-ish direct 2D DFT (N = crop side, small
 * glyph crops ~ 8..40 px, so direct is fine and dependency-free). The
 * magnitude spectrum is then:
 *   1. COMPRESSED: keep only the top-K coefficients by magnitude, store
 *      (u,v,re,im) triples -> far fewer numbers than raw pixels, and the
 *      low-frequency energy compaction means K << N^2 still reconstructs.
 *   2. ANALYZED: emit cheap spectral features (total energy, low/mid/high
 *      band energy fractions, dominant-frequency location) that make a glyph
 *      far more separable than raw pixels under warp/style variation.
 *
 * This mirrors the DFT-WuBu idea (WuBuMath/docs/theory/papers/DFT-WuBu.md):
 * transform regional visual content to frequency domain before downstream
 * modeling, for decorrelation + compression + robustness. Here it is done in
 * plain scalar C11 with no FFT lib (small glyph crops -> direct DFT is cheap).
 *
 * The module is OPAQUE-free by design (it is a math utility, not a stateful
 * stage) so it can be dropped into the coordinate ingestion without a god
 * struct. All buffers are caller-provided.
 */
#ifndef WUBUOCR_DFT_H
#define WUBUOCR_DFT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute the full 2D DFT of a W x H grayscale crop (0=black..255=white).
 * `re`,`im` are caller buffers of size W*H (row-major). Forward transform:
 *   F[u,v] = sum_{x,y} px[y*W+x] * exp(-2*pi*i (ux/W + vy/H))
 * Output is NOT normalized (it is the raw complex spectrum). */
void dft2d(const uint8_t *px, int W, int H, double *re, double *im);

/* In-place inverse 2D DFT: complex spectrum (re,im) of size W*H -> real
 * reconstruction into `out` (size W*H). Values are clamped to [0,255]. */
void idft2d(const double *re, const double *im, int W, int H, uint8_t *out);

/* Compression: from a full spectrum (re,im,W*H), select the `keep` largest
 * magnitude coefficients, write them as packed (u,v,re,im) records into
 * `buf` (caller buffer, >= keep*4 doubles), return the number actually
 * written (<= keep). Magnitudes sorted descending so the prefix is the
 * optimal-rate-ordered stream. `keep` may exceed W*H (then all are kept). */
int dft_compress(const double *re, const double *im, int W, int H,
                 int keep, double *buf);

/* Cheap spectral analysis features for a spectrum (re,im,W*H). Fills:
 *   out[0] = total energy (sum |F|^2)
 *   out[1] = low-band fraction  (DC + 4 lowest radial freqs)
 *   out[2] = mid-band fraction
 *   out[3] = high-band fraction (sums to 1 with low+mid)
 *   out[4] = dominant freq radius (weighted mean of sqrt(u^2+v^2) by |F|^2)
 *   out[5] = dominant freq angle (atan2(v,u) of the 2nd largest peak, the
 *            true "texture direction" -- robust under 2D/3D warp)
 * Returns number of features written (6). */
int dft_features(const double *re, const double *im, int W, int H, double *out);

/* Compression RATIO helper: given crop side N and kept coeffs K, the raw
 * pixel byte count is N*N (uint8). The compressed stream is K*4 doubles
 * (~K*32 bytes). Returns raw_bytes/compressed_bytes ratio (1.0 = break-even). */
double dft_ratio(int N, int keep);

#ifdef __cplusplus
}
#endif

#endif /* WUBUOCR_DFT_H */
