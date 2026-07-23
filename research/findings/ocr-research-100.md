# WuBuOCR — Deep Research Plan (100 points, 50+ web searches)

Compiled 2026-07-19 from 50 web searches across architecture, efficiency, training,
dewarping, multilingual, and 2011-era-CPU deployment. Each item is tied to the
actual WuBuOffice codebase (files referenced). Mark [NOW] = quick win we can ship
this session; [MED] = days; [BIG] = weeks/research.

====================================================================
A. MODEL ARCHITECTURE — make the recognizer faster & better
====================================================================
1. [NOW] Replace BiLSTM with GRU. GRU = ~28% faster, same accuracy as LSTM
   (Elnour 2020, articsledge 2025). rnn.c: add gru_create/gru_forward/gru_backward
   reusing the existing dir_fwd/dir_bwd + adam machinery. crnn.c: swap rnn_create.
2. [NOW] Add a "conv-only recurrence-free" mode (SVTR / Nagendra 2025 recurrence-
   free HTR). For short lines, the LSTM is the bottleneck; a CNN-with-patch-mixing
   trunk (convnet3.c) can read fixed-length strips without RNN. Ship as CRNN_MODE=0.
3. [MED] SVTR-style mixing blocks: local mixing (depthwise 1D conv over the strip
   sequence) + global mixing (strided pooling). Replace BiLSTM trunk in crnn.c.
   SVTR hits SOTA-fast on scene text (IJCAI 2022, 384 cites).
4. [MED] Gated conv (GRCNN, Wang 2017 / Bluche 2017): gate the convnet3 feature
   maps so the model attends to ink vs background. ~2-3% acc gain on noisy scans.
5. [NOW] BSConv (blueprint separable conv, Haase 2020): replaces depthwise-separable
   on CPU with grouped "blueprint" kernels — faster than standard conv on x86
   WITHOUT SIMD. Refactor convnet3.c conv op. #1 CPU-speed win from research.
6. [MED] CBAM attention (Woo 2018, 39k cites) after convnet3 stage 3: cheap
   channel+spatial attention; great on lightweight nets. Add to convnet3.c.
7. [MED] Progressive patch embedding (SVTR): feed the line as overlapping vertical
   patches; reduces the need for a tall fixed STRIP cell. Improves warp tolerance.
8. [BIG] Recurrence-free 2D attention over the whole line (ViT-lite) as an OPTION
   for when RNN-free is mandated; keep CTC (no transformer decoder needed).
9. [NOW] Add a tiny 1x1 "bottleneck" before the LSTM to cut its input dim (D->hid/2),
   halving RNN MACs. crnn.c forward_seq.
10. [MED] Weight sharing across conv stages (Ott 2020 "Learning in the machine"):
    reuse conv kernels across strips to slash params + memory on 64MB box.

====================================================================
B. INFERENCE SPEED — 2011 Q6600 (no SIMD, scalar C11)
====================================================================
11. [NOW] LUT-NN table lookup (Microsoft LUT-NN 2023 / DeepGEMM 2023): for 4-bit
    weights, replace MAC with a precomputed lookup table indexed by (input,weight)
    nibbles. The single biggest CPU win for ultra-low-bit. Add lut_nn.c.
12. [NOW] Fixed-point inference (Khalifa 2024 / StackOverflow NN-fixed-point):
    store weights as int16 with a shared scale; dot products in int32. Q6600 has no
    FPU penalty for int. Add scalar fixed-point path in convnet3.c + rnn.c.
13. [NOW] im2col + GEMM (sahnimanas 2023): explicit im2col then a tight GEMM
    (cache-blocked) for convnet3 — 5-20x faster than naive nested loops on x86.
14. [MED] Winograd F(2x2,3x3) conv (Liu / Park 2025): 2.25x fewer MULs for the 3x3
    kernels in convnet3.c. Theoretical 2.25x; practical ~1.8x on CPU.
15. [NOW] Cache-blocking for the GEMM (Cornell CS3410): tile to L1/L2 so the strip
    features + weights stay hot. 2-4x on Q6600's tiny cache.
16. [NOW] OpenMP over time-steps (Oliveira 2025 / atharva253): #pragma omp parallel
    for the per-strip conv+forward. 4 cores => ~3.5x. Build with -fopenmp.
17. [NOW] Glyph cache in wubufont (osor.io font cache, Ghostscript): rasterize each
    (codepoint,ppm) once into an atlas; reuse across the page. Cuts render time 10x.
18. [MED] Early-exit (Laskaridis 2021, 2-8x): attach a shallow classifier after
    convnet3; if high-confidence, skip the LSTM for easy strips (clean text).
19. [NOW] Incremental / memoized strip inference: cache conv features per (strip x,y)
    so re-segmentation of a line doesn't re-run conv. crnn.c strips buffer.
20. [MED] Lookup-table activation (sigmoid/tanh) for LSTM: precompute 256-entry
    tables; replace exp/tanh libcalls (slow on 2011 FPU). rnn.c.

====================================================================
C. TRAINING — converge faster, generalize better
====================================================================
21. [NOW] Label smoothing on CTC (Lukasik 2020): soften target distribution;
    mitigates overconfidence + the repeat-collapse errors we see. ctc.c.
22. [NOW] Focal CTC (Feng 2019, 40 cites): reweight rare/blank classes — fixes the
    unbalanced Zipf wordlist (top-10 words dominate). ctc.c loss reweight.
23. [NOW] Cosine LR already in; add warmup (5% linear) + AutoLRS-style Bayesian
    schedule search (Jin 2021) over 1 trial to pick LR. crnn_lex_train.c.
24. [MED] MixUp/CutMix for line images (NoiseCutMix 2025): paste a patch of one
    word-line onto another with interpolated target — robustness to smear/ink.
25. [MED] Knowledge distillation (KD) teacher->student (Amit 2023): train a big
    CRNN on a server, distill logits to the 2011 tiny net. Recovers ~2-3% acc.
26. [MED] Pruning (Molchanov 2016, 3675 cites): prune conv kernels by importance;
    fine-prune. Shrinks model + speeds inference on the 64MB target.
27. [NOW] int8/int4 quantization-aware training (gmicloud 2025): quantize weights in
    training loop so the fixed-point path (B12) is accurate. 
28. [MED] GRU + AdamW (decoupled weight decay) instead of Adam; stabilizes big LRs.
29. [NOW] Curriculum already in; add a 2nd stage: clean->warp->warp+noise->photo,
    ramping severity in 4 bands (our WARM concept extended).
30. [BIG] Self-supervised pretrain (Yamanko 2024, RePre IJCAI 2022): contrastive
    pretrain convnet3 on unlabeled warped pages, fine-tune on words. Transformer-free.

====================================================================
D. DEWARPING / GEOMETRY — close the photo gap for real
====================================================================
31. [MED] Adopt DocGeoNet/TPS (Feng 2022/2023, 64 cites): replace our 4-corner
    lens_flatten with a learned Thin-Plate-Spline grid. Needs a tiny TPS net (CNN).
32. [MED] DocTr / DocTr++ (2022/2023): coordinate-denoising transformer for
    rectification; SOTA but heavier — port the geometric prior, not the transformer.
33. [NOW] Axis-aligned dewarping (arxiv 2507.15000): cheap x/y independent correction
    covers most phone-photo curl; 10x cheaper than full TPS. lens.c.
34. [NOW] Multi-corner (8-point) lens_flatten: our current 4-corner assumes a
    rectangle; real docs curve. Extend lens.c to a quad-mesh (4x4 grid) warp.
35. [MED] Synthetic warp distribution = real photo distribution: fit our
    gen_line jitter params to measured phone-camera warp (perspective + curl + roll).
36. [NOW] Contrast/illumination correction before recognition (DocTr illumination
    branch): per-line local contrast normalization in lens.c.
37. [BIG] DocMatcher (Hertlein WACV 2025): line-matching rectification using text
    lines as structural cues — robust when corners are unavailable.
38. [NOW] Bilinear (not nearest) resample in crop_norm: removes the jaggies that
    confuse the conv on dewarped lines. ocr_render.h.
39. [MED] Learnable unwarping as a network head: a small CNN predicts the TPS grid
    from the page; trains end-to-end with CTC (documents -> text, no labels).
40. [NOW] Skew/rotation correction (deskew, awesome-ocr): Hough/projection deskew
    before line segmentation; cheap, big win on scanned docs.

====================================================================
E. DATA & AUGMENTATION — more & better training data
====================================================================
41. [NOW] STRAug (Atienza 2021, 36 STR augs): port the 8 geometry + texture augs
    (perspective, shrink, rotate, erosion, dilation, grayscale, etc.) into
    ocr_render.h gen_line.
42. [NOW] TextRecognitionDataGenerator (Belval, multilingual): generate millions of
    synthetic word images per language from our 32 wordlists + fonts. 9M-image
    recipe (Krishnan 2016).
43. [MED] Real photo fine-tune: use a few hundred real phone photos (our own) with
    the warp curriculum to bridge synth->real (domain adaptation).
44. [NOW] Extend wordlists to top-10k (already curated) for harder coverage; train
    on 10k, eval on 1k. data/wordlists.
45. [MED] Per-language font mixing: render each word with 3-5 fonts per script so the
    net is font-invariant (currently 1 font = overfit to Latin.ttf glyph style).
46. [NOW] IAM handwriting baseline (tuandoan998): add a handwriting wordlist +
    cursive font to extend beyond printed (HTR). 
47. [MED] Synthetic noise injection: add JPEG/scan speckle, bleed-through, coffee
    stains as augmentation (real-document robustness).
48. [NOW] NFKC Unicode normalization (UAX #15, OCRmyPDF): fold compatibility chars
    so ① and (1) and fullwidth map consistently. lexicon.c utf8.
49. [MED] Curriculum of distortion difficulty (ours) + STRAug = the full recipe.
50. [BIG] GAN-based synthetic document pages (DocGAN / pix2pixHD dewarping): generate
    full distorted pages for end-to-end training.

====================================================================
F. DECODING & LANGUAGE MODEL — punch above weight (tokenization method)
====================================================================
51. [MED] Word Beam Search (Scheidl 2018, 133 cites; CTCWordBeamSearch C++):
    combine beam search + lexicon token-passing. Replaces our Levenshtein snap.
    Massive accuracy gain on real words (we measured 75%->91% with naive correct).
52. [NOW] Char n-gram LM (our own, from the wordlists): a 2-3 gram over codepoints
    biases the beam toward real word shapes. Train from data/wordlists in 1 pass.
53. [NOW] Beam search (not just greedy) in crnn_recognize_utf8: width 8-16. Fixes
    the repeat-collapse (XXGCKV->XGCKV) we see. ctc.c.
54. [MED] KenLM-style tiny ARPA LM per language (compressed): 50KB/language, gives
    word-level priors to the beam. Build from wordlists.
55. [NOW] Levenshtein correction already in; tighten len_slack by word length
    (slack = max(1, len/4)) so 'great'->'completely' mistakes stop happening.
56. [MED] Confusion-matrix error model: learn common OCR confusions (m->rn, l->li)
    from eval, bias the LM to correct them.
57. [NOW] Spell-correct only low-confidence prefixes (beam margin small) to avoid
    over-correcting already-correct words.
58. [MED] Number/punctuation handling: beam search must allow arbitrary char
    strings (Word Beam Search "beam" mode) for digits/dates the lexicon lacks.
59. [NOW] Script-specific decode: RTL languages (Arabic/Hebrew) reverse the output
    sequence after CTC (reading order). crnn_recognize_utf8 flag.
60. [BIG] Lightweight transformer LM (distilled, <1M params) as the decoder prior —
    but run as a SEPARATE post-step, not in the net (keeps recognizer 2011-clean).

====================================================================
G. MULTILINGUAL / SCRIPT SPECIFICS
====================================================================
61. [MED] Script ID before decode (Huang 2021 Multiplexed OCR / llamaindex): a
    tiny 1-layer conv classifies the script of a line, routes to the right model.
62. [NOW] Unified charset per script family (Latin+Cyrillic+Greek share Latin-ish
    cells? no—keep separate; but one MODEL per family reduces model count).
63. [MED] Arabic RTL + cursive: render right-to-left, join glyph forms (initial/
    medial/final) — needs a shaping step before rasterize (HarfBuzz-lite or table).
64. [MED] Indic joined chars (Mathew 2016, Palrecha 2011): segmentation-free CRNN
    already handles this (good); add dead-consonant + matra rendering rules.
65. [MED] CJK: charset is 3000 (zh) — CTC classes explode. Use a 2-level decode:
    CRNN gives radical/component hints, a CJK lexicon resolves the character.
66. [NOW] Hangul (ko): jamo composition — render by syllable block; charset small
    (958) so it's cheap; verify our model already handles it.
67. [MED] Japanese (ja): kana + kanji mixed; treat kanji as the rare long-tail, kana
    as the common stream. Curriculum weights kanji higher.
68. [NOW] Per-language wordlists already 32; add script-coverage report (chars seen
    vs needed) so we know which scripts are undertrained.
69. [BIG] Zero-shot cross-script transfer: train on Latin, adapt to a new script
    with 100 real words (few-shot). KD + frozen conv.
70. [MED] Font fallback chain: if a codepoint is missing in the primary font, try
    the next font in the family (already partly in corpus-flat).

====================================================================
H. LAYOUT / DOCUMENT MODEL — the full pipeline
====================================================================
71. [MED] DocLayout-YOLO (opendatalab, YOLOv10): detect blocks/tables/headers on
    2011 CPU with a tiny YOLO — but keep it OPTIONAL (our XY-cut already works).
72. [NOW] XY-cut reading order already in layout.c; add column-balancing for
    multi-column (Nagy & Seth 1984) — verified approach.
73. [NOW] Table reconstruction: detect grid lines, emit Markdown table syntax.
74. [MED] Reading-order graph (Lee 2024 ACD): adjacent-character detection for
    correct order in complex layouts.
75. [NOW] Output to .md/.docx/.odt (our wubuconv engine): wire crnn_recognize into
    the document model with coordinate JSON (already in page_compose.c).
76. [MED] Lossless round-trip: OCR text + original coordinates => re-render editable
    doc that overlays the scan (proofread mode).
77. [NOW] Multi-column + caption handling for magazines/paper (common real docs).
78. [BIG] End-to-end trainable page->text (attention over blocks) — research, not
    for 2011 box.
79. [MED] Handwriting line model (separate, IAM-pretrained) selected by script ID.
80. [NOW] Confidence per word -> highlight low-confidence for human proofread.

====================================================================
I. EVALUATION / BENCHMARKS / OPS
====================================================================
81. [NOW] Adopt CC-OCR style eval (Yang 2025, 39 sub-tasks): char/word/exact +
    per-script breakdown. Our EVAL block already does this.
82. [NOW] Track MACs/img + ms/img on Q6600 as the primary speed metric (not FLOPs).
83. [MED] Ablation harness: script to toggle BSConv/GRU/beam/WBS and report delta.
84. [NOW] Model card per language: acc, speed, charset coverage (auto from MANIFEST).
85. [MED] CI on the 2011 box: run a 1-epoch train + photo demo as a gate.
86. [NOW] Reproducible seed + logged curriculum params per model (we have env vars).
87. [BIG] Compare vs Tesseract/PaddleOCR on our 32 langs as the honest ceiling.
88. [NOW] Memory budget assert (<64MB weights+scratch) in crnn.c init.
89. [MED] On-device profiling: cache misses, SIMD-off cost, OpenMP scaling.
90. [NOW] Version the model format (CRN1 + stride already); bump on arch change.

====================================================================
J. RESEARCH FRONTIERS (watch, adopt when proven)
====================================================================
91. [BIG] SVIPTR (arxiv 2401.10110): fastest tiny STR model — port the trunk idea.
92. [BIG] DocTr++ unrestricted rectification — when corners unavailable.
93. [MED] Binary/1-bit nets (BNN survey 2025, Reddit 1-bit): extreme 64MB fit.
94. [BIG] Mogrifier/CRT LSTM variants — cheaper recurrence.
95. [MED] Blueprint separable + Winograd combo (Haase + Park): max CPU conv speed.
96. [BIG] Contrastive pretrain (Yamanko 2024) without labels — data-efficient.
97. [MED] Token-passing LM (Scheidl) integrated with our Zipf lexicon = ideal fit.
98. [BIG] Unified multilingual single-model (Huang 2021) vs per-script models:
    trade accuracy vs model count on 64MB.
99. [MED] Differentiable dewarping (DocGeoNet) trained jointly with CTC.
100. [NOW] Doc this plan in research/findings/ocr-research-100.md and link from SKILL.

====================================================================
EVIDENCE HIGHLIGHTS (from 50 searches)
- SVTR (IJCAI 2022, 384 cites): recurrence-free patch mixing beats CRNN on speed.
- GRU 28% faster than LSTM, equal acc (Elnour 2020, articsledge 2025).
- BSConv faster than depthwise-separable on CPU (Haase 2020, 311 cites).
- LUT-NN / DeepGEMM: table-lookup inference for <=4-bit (Microsoft 2023).
- Word Beam Search: lexicon + beam = token passing, SOTA CTC decode (Scheidl 2018).
- DocGeoNet/TPS: learned thin-plate-spline rectification (Feng 2022/23, 64 cites).
- STRAug: 36 STR-specific augmentations (Atienza 2021, 48 cites).
- Focal CTC: fixes unbalanced word-frequency datasets (Feng 2019, 40 cites).
- Label smoothing helps CTC overconfidence (Lukasik 2020, 562 cites).
- Early-exit nets: 2-8x adaptive compute (Laskaridis 2021, 221 cites).
- TextRecognitionDataGenerator: 9M multilingual synthetic word images (Krishnan 2016).
- CC-OCR: 39-subtask benchmark, multilingual (Yang 2025, 83 cites).
- NFKC normalization standard for OCR text (UAX #15, OCRmyPDF).
- Indic/CJK: segmentation-free CRNN is the right approach (Mathew 2016, Palrecha 2011).

CURRENT CODEBASE STATE (2026-07-19)
- crnn.c (CRNN+CTC, stride/overlap, crnn_recognize_utf8), convnet3.c (3-stage conv),
  rnn.c (BiLSTM), ctc.c, lexicon.c (Zipf+Vose+lens-correction+wchar), ocr_render.h
  (SHARED train/infer geometry), lens.c (4-corner flatten), image.c, png*/wubuzip.
- Trainers: crnn_lex_train.c (real-word, curriculum, WARP), crnn_warp_train.c.
- Demos: crnn_photo_demo.c (multilingual front door), crnn_doc_demo.c.
- Data: data/wordlists/<32 langs>/top{1k,10k}.txt (hermitdave/FrequencyWords, CC-BY-SA).
- Measured: EN clean 99.6% (+lexicon), EN warped photo 65-83% char, ES 75.5% char,
  DE 82.3% char (warp=1 curriculum). AR pending (3rd job didn't launch).
