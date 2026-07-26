# WuBuOffice — 100 Feature Roadmap (Office-Lens grade OCR)

Research basis: Office Lens, Adobe Scan, OCRmyPDF, Tesseract PSMs, Sauvola
adaptive binarization, hOCR/ALTO, searchable-PDF, EXIF orientation,
despeckle/binarization surveys, layout-analysis / reading-order papers.

Legend: [DONE] implemented this pass · [CORE] pipeline exists · [TODO] planned.

## A. Capture & Input (photos → document)
1. [DONE] JPEG decode via ffmpeg transcode (trust boundary, no C-side libjpeg)
2. [DONE] PNG decode (clean-room, no libpng)
3. [DONE] Netpbm (PGM/PPM) native decode
4. [DONE] EXIF auto-rotate (`-autorotate` in ffmpeg pipe)
5. [CORE] Multi-page TIFF / PDF image input
6. [TODO] HEIC/AVIF decode (via ffmpeg)
7. [TODO] Direct camera capture (v4l2) front-end
|8. [DONE] Batch / watch-folder ingest (`ocrwatch` daemon, polls dir → OCR)
|9. [DONE] Drop-folder watcher (same `ocrwatch`, sidecar .seen tracking)
|10. [TODO] Clipboard image paste ingest

## B. Pre-processing (the "Lens" front door)
11. [DONE] Adaptive background (median) + polarity flip for CRNN
12. [DONE] Deskew (rotation correction, ±8° scan)
13. [DONE] Sauvola adaptive binarization (clean-room, integral-image O(W·H))
14. [DONE] Salt-and-pepper despeckle (1-opening on ink mask)
15. [DONE] Auto-crop to ink bounding box (margin removal)
16. [DONE] Empty-page guard (uniform page → `{"blocks":[]}`)
|17. [CORE] Perspective / 4-corner un-warp (`lens_flatten`, QUAD=1 opt-in)
|18. [DONE] Reliable automatic document-corner detection (quad finder)
|19. [DONE] Shading / vignette correction (`ocr_image_shading_correct`)
|20. [DONE] Contrast stretch normalization (`ocr_image_contrast_stretch`)
|21. [TODO] Color desaturation tuned per-region
|22. [TODO] Motion-blur / defocus repair (Wiener-lite)
|23. [TODO] Page-frame / shadow removal
|24. [TODO] Hole-punch / staple mark suppression
|25. [TODO] Non-local-means denoise
|26. [DONE] Median salt-and-pepper denoise (`ocr_image_median`)
|27. [DONE] Unsharp-mask sharpen (`ocr_image_sharpen`)
## C. Layout analysis & reading order
26. [DONE] Horizontal line segmentation (projection profile)
27. [DONE] Multi-column detection (widest-gutter split)
28. [DONE] Row×column cell grid → table (reading order preserved)
29. [CORE] XY-cut region segmentation (`ocr_layout`)
30. [DONE] Recursive N-column splitting (true 3+/N-col)
31. [DONE] Reading-order model (layout-aware sequence; order index + header/footer detection in docmodel)
32. [DONE] Table-structure detection (ruled/blank grids → tagged `table` block w/ cell coords)
33. [TODO] Figure / image region detection + captioning hook
34. [TODO] Header / footer / page-number zones
35. [TODO] Drop-cap & float detection
36. [TODO] List / bullet detection
37. [TODO] Mathematical-region detection
38. [TODO] Reading-order for mixed LTR/RTL

## D. Recognition engine
39. [CORE] CRNN + log-space CTC (Graves 2006, skip-transition fix)
40. [CORE] 14 multiscript fonts trained ≥96% (5/14 + English delivered)
41. [DONE] English basic-Latin boosted to 97.7% held-out
|42. [DONE] Light photo-augmentation fine-tune (no collapse)
|43. [DONE] Confidence score per line (CTC path prob → 0..100; `crnn_recognize_scored`, emitted as `conf` in docmodel JSON)
|44. [DONE] Word-level confidence + uncertain-word flag (per-char `cconf` array in docmodel)
|45. [DONE] Lexicon / dictionary beam correction (`lex_correct`, `LEX=` wordlist post-pass)
46. [DONE] Language auto-detect → charset/script tag (per-block `lang` + doc-level majority vote)
47. [TODO] Handwriting model (separate CRNN head)
48. [DONE] Math/equation recognition (line detector + `math` block w/ latex placeholder)
49. [TODO] Barcode / QR decode → embedded text
50. [TODO] ONNX import of external models

## E. Output formats
51. [CORE] DOCX (Word)
52. [CORE] Markdown
53. [CORE] ODT (OpenDocument)
54. [CORE] HTML
55. [CORE] Editable docmodel JSON
56. [DONE] Multi-column → DOCX table (spatial preserved)
|57. [DONE] Searchable PDF (invisible text layer over image)
|58. [DONE] PDF/A (archival, accessible) — XMP + OutputIntent + embedded sRGB ICC
|59. [DONE] hOCR output (`docfmt_to_hocr`)
|60. [DONE] ALTO XML output (`docfmt_to_alto`)
|61. [DONE] TEI / academic XML (`docfmt_to_tei`)
|62. [DONE] CSV table extraction (`docfmt_to_csv`)
|63. [DONE] Excel (xlsx) for tables
|64. [TODO] Epub reflow
|65. [DONE] RTF (`docfmt_to_rtf`)
|66. [DONE] LaTeX (`docfmt_to_latex`)
|67. [DONE] Plain text (.txt) (`docfmt_to_text`)
|68. [DONE] JSON-Lines (one block per line, streaming) (`docfmt_to_jsonl`)

## F. Editing & post-processing
69. [CORE] WuBuNote text buffer (Notepad++-class editing core)
70. [TODO] In-place correction UI (click word → edit)
71. [TODO] Spell-check + suggested fixes
72. [TODO] Find / replace across document
73. [TODO] Style / heading promotion
74. [TODO] Redaction layer
75. [TODO] Annotation / comment layer

## G. Robustness & testing
76. [DONE] Unit tests: binarize (Otsu/Sauvola/despeckle/autocrop)
77. [DONE] Unit tests: multi-column detection (1/2/3-col)
78. [CORE] `test_crnn_transcribe` regression
79. [TODO] Accuracy harness on held-out sets (char/word exact)
80. [TODO] Golden-page fixture corpus
81. [TODO] Fuzz input decoder (malformed JPEG/PNG)
82. [DONE] AddressSanitizer/UBSan gate (WITH_SANITIZER=ON; full suite clean)
83. [TODO] Performance benchmark (MPix/s)
84. [TODO] Deterministic seed for augmentation
85. [TODO] Coverage report

## H. Integrations & UX
|86. [DONE] CLI: `image2doc IN OUT [--rotate DEG] [--no-deskew] [--contrast] [--median] [--shading] [--sharpen]` (15 output formats: docx,md,odt,html,json,txt,tsv,csv,jsonl,latex,rtf,hocr,alto,tei,xlsx,pdf)
|87. [TODO] TUI previewer (wubutui render of recognized doc)
|88. [DONE] REST micro-service (`ocrserve` — POST image → document, 11 formats)
|89. [DONE] Watch-folder → auto-OCR daemon (`ocrwatch`)
90. [DONE] Cloud-sync export (SYNC_DIR mirror + SYNC_CMD hook)
91. [TODO] Keyboard-shortcut capture (scan hotkey)
92. [TODO] Multi-language batch (per-page charset)

## I. Accessibility & compliance
93. [DONE] PDF/UA tags (structure tree + MarkInfo + /Lang)
94. [DONE] Alt-text for figures (figure-region detection + `figure` block w/ bbox + alt placeholder)
95. [DONE] Language tag per block (script auto-detect via codepoint ranges)
96. [DONE] Unicode NFC normalization (Latin precomposition, `wubuocr_nfc_latin`)

## J. Performance & deployment
97. [CORE] CUDA GRU path (`WITH_CUDA`)
98. [DONE] OpenCL fallback (GPU GRU forward, runtime dlsym, CPU fallback)
99. [DONE] Threaded page batch (--batch N, pthreads, shared model)
100. [DONE] Static single-binary build (STATIC=ON → fully static image2doc) (musl)

---
|Implemented this pass (cumulative): 1,2,3,4,11,12,13,14,15,16,19,20,26,27,
|43,56,57,59,60,62,65,66,67,68,76,77,8,9,86,88,89,30,18,63,42,44,58,61,45,31,93,94,46,96,90,32,99,94b,82,98,100,48 (56 features).
|Core/p...[truncated]
|Highest-value TODOs: figure captioning via vision model, handwriting model (47),
|multilingual font pack, barcode/QR decode (49), end-to-end accuracy harness (79).
