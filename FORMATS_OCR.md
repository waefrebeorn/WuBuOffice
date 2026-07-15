# WuBuOCR — image → document digestion (clean-room C11)

Adds an **image ingestion front-end** to the WuBuOffice document backbone: raster
pixels → binarized page → layout blocks in reading order → a structured document
model the rest of the suite already knows how to emit (docx/odt/md/html/json).

This is the WuBuOffice answer to "digest a scanned/photographed page into an
editable document" — the same job DeepSeek-OCR and NVIDIA Nemotron-Parse do with
a 3B-parameter VLM, done here as an explicit, dependency-free structural pipeline.

## Research: documented OCR-digestion pain points (2025–2026)

Sources: DeepSeek-OCR (arXiv:2510.18234), NVIDIA Nemotron-Parse 1.1
(arXiv:2511.20478), ICCV 2025 "Survey on Reading Order, ToC & Structure",
extend.ai / llamaindex layout-analysis writeups, learnopencv DeepSeek-OCR
teardown. The recurring failure modes every source names:

1. **Reading order collapse.** Traditional OCR emits text in raster (top-to-bottom,
   left-to-right pixel) order, so **multi-column** pages interleave columns into
   gibberish. "Layout analysis must run BEFORE recognition to preserve reading
   flow" (extend.ai). This is the #1 structural pain point.
2. **Token explosion in VLM OCR.** A 1024×1024 page → ~4096 vision tokens; an A4
   scan → ~45k tokens; attention is O(n²) → billions of pairwise ops per page.
   High GPU cost, seconds per page, context truncation. DeepSeek-OCR's whole
   thesis ("optical compression": 64–400 vision tokens per page via a conv
   downsampler between SAM-base and CLIP-large) exists to fight this.
3. **Table structure loss.** Cell/row/column grid is flattened to a text blob;
   merged cells and spanning headers are mangled. Nemotron-Parse markets
   "structured table parsing with spatial grounding" as the differentiator.
4. **Layout element confusion.** Headers, footers, captions, figures, page numbers
   get inlined into body text. No block typing → no clean document model.
5. **Verbose, un-structured output** unsuitable for downstream LLM context or for
   round-tripping into an editable office document.
6. **Skew / noise / imperfect scans** break template-matching engines (classic
   Tesseract weakness on anything but clean print).

## The two reference systems (format-truth study only — no weights, no code copied)

**DeepSeek-OCR (DeepEncoder + MoE decoder).** Vision-as-compression:
- DeepEncoder = SAM-base (80M, window attention, local features) → 2-layer conv
  downsampler (16× token reduction) → CLIP-large (300M, dense global attention on
  the already-compressed 256 tokens).
- Resolution modes: Tiny 512²→64 tok, Small 640²→100, Base 1024²→256,
  Large 1280²→400, Gundam = n×640² tiles + 1×1024² global view.
- MoE decoder = DeepSeekMoE-3B, 570M active (6 routed + 2 shared experts).
- Prompt-driven: `<image>\n<|grounding|>Convert the document to markdown.`

**NVIDIA Nemotron-Parse 1.1.** Lightweight VLM (transformer) unifying OCR +
markdown + layout analysis + table parsing with **spatial grounding** (bounding
boxes per element). Same conceptual pipeline, packaged as one model.

**Why we do NOT "rip" them literally.** Both are *trained neural networks* — their
capability lives in learned weights (billions of params), not in an algorithm you
can transliterate to C. Copying weights is not clean-room and not C11. What IS
clean-room reimplementable — and what actually fixes pain points 1, 3, 4, 5, 6 — is
the **structural pipeline** those VLMs learned to approximate:

    pixels → binarize → layout analysis → reading-order blocks → typed regions
           → table grids → structured document model → editable office doc

The single learned step (glyph pixels → Unicode text) is isolated behind an
explicit **recognizer plug-in slot** (`OcrRecognizer` callback). WuBuOCR ships the
entire deterministic scaffold and a NULL/stub recognizer; a real glyph classifier
(neural or feature-based) drops into the slot without touching the pipeline. We
never fabricate recognized text — an absent recognizer yields empty glyph strings
with correct geometry, honestly.

## WuBuOCR pipeline (this module)

| Stage | File | Solves |
|-------|------|--------|
| Raster decode (PBM/PGM/PPM, clean-room) | `image.{h,c}` | input, dependency-free |
| Otsu global binarization | `binarize.{h,c}` | noise/threshold (pain #6) |
| Recursive **XY-cut** via projection profiles | `layout.{h,c}` | **reading order + multi-column (pain #1)** |
| Connected-component glyph/word boxing | `components.{h,c}` | segmentation, table cells (pain #3) |
| Facade → document JSON model + recognizer slot | `wubuocr.{h,c}` | structured output (pain #4, #5) |

XY-cut (Nagy & Seth, 1984) is the classical, deterministic layout algorithm that
recovers reading order by recursively splitting a page along its widest
whitespace gutter (alternating vertical/horizontal). It is exactly the
column-aware ordering that raster OCR lacks — the deterministic ancestor of what
DeepSeek/Nemotron learned. No training, no GPU, no dependency.

Design follows the suite standard: opaque structs, minimal includes, C11 only,
self-contained modules, sanitizer-gated (ASan+UBSan, 0 leaks/UB/warnings),
reuse-never-duplicate (JSON model via `wubujson`; document emission via the
existing `wubuconv`/`wubudoc` engine).
