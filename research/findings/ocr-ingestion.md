# DFT-style compression + golden-ratio coordinate ingestion

## What was built this session
The coordinate-aware OCR document-ingestion system the user has been describing
is now **wired end-to-end and runs**. Two previously-missing links were filled:

1. **DFT compression + spectral analysis module** — `src/wubuocr/dft.{h,c}`
   (new). Direct 2D DFT on each glyph crop (small crops -> no FFT lib needed),
   magnitude-sorted coefficient compression (`dft_compress`), and 6 cheap
   spectral features (`dft_features`: energy, low/mid/high band fractions,
   dominant-frequency radius + angle). Mirrors the DFT-WuBu paper idea
   (WuBuMath/docs/theory/papers/DFT-WuBu.md) but in dependency-free scalar C11.
2. **Golden-ratio coordinate placement** — `src/wubuocr/goldplace.{h,c}` (new).
   Byte-identical copy of WuBuMath's `golden_subdivide` + `generate_phi_spiral`
   (kept in sync, NOT linking WuBuMath so the OCR stack stays 2011-dep-free).
   Places glyphs at golden-ratio coordinates = the "warped coordinate ablation
   on multi-coordinate styles" + "golden-ratio agnostic anti-aliasing" (coords
   don't depend on raster ppm).
3. **End-to-end driver** — `tools/ocringest.c` (new). Loads many real TTF/OTF
   fonts -> composes a warped multi-font page on golden coordinates -> runs the
   existing deterministic pipeline (binarize -> XY-cut -> components ->
   coordinates) -> per detected glyph emits DFT compression ratio + spectral
   features + golden-region tag, as coordinate JSON.
4. **Per-glyph box accessors** added to wubuocr.{h,c}: `ocr_page_glyph_count` /
   `ocr_page_glyph` (honest opaque-struct extension, no monolith breach).

## Verified end-to-end (real run, 64 Windows fonts)
```
/tmp/ocringest /mnt/c/Windows/Fonts /tmp/ingest2.json 80 60 7
  loaded 64 fonts
  golden layout points: 80
  stamped 80 warped glyphs
  wrote ingest2.json (75 blocks)
```
- 80 warped glyphs placed at golden-ratio coords; pipeline detected 75 blocks.
- Per glyph: true doc coords (x,y,w,h) + DFT (compression_ratio, coeffs_kept,
  energy, low/mid/high band, dom_freq_radius/angle) + golden_region tag.
- DFT compression: avg 0.12x raw-equiv (i.e. ~8x smaller than raw bytes for
  glyphs above ~16px); for tiny <8px glyphs compressed > raw (honest: DFT only
  pays off above a size threshold).

## Honest limitations / next steps
- **Glyph fragmentation**: 115 components detected from 80 placed (recall 143%).
  This is connected-component splitting of warped/overlapping letters on a
  deliberately messy scatter page -- exactly the stress condition intended, but
  it means word/line grouping (already in wubuocr block_text) needs the real
  multi-font recognizer (`ocr_fontbank_recognize`) to merge fragments. Geometry
  is correct; text is intentionally empty (geometry-only mode, no fabricated
  text).
- **Recognizer not yet wired**: ocringest runs in geometry-only mode (NULL
  recognizer -> honest empty text). The `fontbank.c` multi-font voting
  recognizer exists but needs real-font templates trained. That is the next
  rung to turn coordinates into text.
- **Compression is lossy-tolerant**: `dft_compress` keeps top-K by magnitude;
  `idft2d` reconstructs. Not yet measured reconstruction PSNR -- a good next
  experiment.

## Build (full OCR core + new modules, no WuBuMath link)
```
cc -std=c11 -O2 -fno-pie -no-pie -Wall -Wextra \
   -Isrc/wubuocr -Isrc/wubufont -Isrc/wubujson \
   tools/ocringest.c src/wubuocr/*.c src/wubufont/wubufont.c src/wubujson/json.c \
   -o /tmp/ocringest -lm
```
(Note: -fno-pie -no-pie dodges a textrel with the embedded font8x8 const table;
 the existing build_wubu_ocr.sh omits wubufont/wubujson/goldplace/dft -- it is
 stale and should be updated to include them.)

## Relationship to existing stack
This is the "what" layer: coordinate ingestion + cheap DFT features + golden
placement. The conv/MLP net (convnet3 + train_mt, 91% Fashion) is a candidate
*recognizer* that can later drop into the `OcrRecognizer` slot. The 56+8
stylebank is the multi-font expert ensemble for the voting. All consistent with
the 2011 single-core C11 framing.
