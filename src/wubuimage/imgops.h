/* imgops.h -- clean-room image preprocessing ops (8-bit grayscale OcrImage).
 * Dependency-free C11. These back the "Office Lens"-style robustness
 * features: arbitrary rotation, contrast stretch, salt-pepper median denoise,
 * and shading (vignette / uneven-illumination) correction. */
#ifndef WUBUIMAGE_IMGOPS_H
#define WUBUIMAGE_IMGOPS_H
#include "image.h"   /* OcrImage */

#ifdef __cplusplus
extern "C" {
#endif

/* Rotate by degrees (counter-clockwise). Pixels outside the source become
 * the supplied fill value (use the page background estimate). */
OcrImage *ocr_image_rotate(const OcrImage *im, double deg, uint8_t fill);

/* Contrast stretch to [lo,hi] percentiles (clamped to full 0..255).
 * Stretches the image so the darkest lo% and lightest hi% span the range. */
OcrImage *ocr_image_contrast_stretch(const OcrImage *im, int lo_pct, int hi_pct);

/* 3x3 (radius r) median filter -- removes salt-and-pepper noise. */
OcrImage *ocr_image_median(const OcrImage *im, int radius);

/* Shading correction: estimate a smooth illumination field via a large-radius
 * mean blur, divide the image by it (scaled to mean 128), then stretch back.
 * Mitigates uneven lighting / scanner vignette. */
OcrImage *ocr_image_shading_correct(const OcrImage *im, int blur_radius);

/* Unsharp mask (sharpen): out = clamp(in + amount*(in - blurred)). */
OcrImage *ocr_image_sharpen(const OcrImage *im, int blur_radius, double amount);

#ifdef __cplusplus
}
#endif
#endif
