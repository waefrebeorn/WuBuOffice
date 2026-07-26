cmake_minimum_required(VERSION 3.10)
project(WuBuOffice C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Expose POSIX functions (strdup, open_memstream) under -std=c11 so pointer-
# returning POSIX calls are not implicitly truncated to int.
add_compile_definitions(_POSIX_C_SOURCE=200809L)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# Optional AddressSanitizer + UndefinedBehaviorSanitizer build. Enables the
# leak/UB gate used by the CI "sanitizer" job. Off by default so the normal
# build stays fast and dependency-free.
option(WITH_SANITIZER "Build with -fsanitize=address,undefined (leak/UB gate)" OFF)
if(WITH_SANITIZER)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g -O1)
  add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
endif()

# Static single-binary build (#100): link every executable with -static so the
# produced image2doc / ocrserve / crnn_* tools are self-contained (no shared-lib
# or libc runtime dependency). Off by default (shared is fine for development).
option(STATIC "Build fully static executables (-static)" OFF)
if(STATIC)
  add_link_options(-static)
  # OpenMP/most static toolchains need pthread and the static math/libc present;
  # they are on this box (libc.a/libm.a/libpthread.a). Keep -static for all targets.
endif()

# --- wubuzip: from-scratch ZIP + DEFLATE (reader and writer) ---
add_library(wubuzip STATIC
  src/wubuzip/crc.c
  src/wubuzip/zip.c
  src/wubuzip/reader.c
  src/wubuzip/bit.c
  src/wubuzip/bitw.c
  src/wubuzip/canon.c
  src/wubuzip/huffman.c
  src/wubuzip/fixed.c
  src/wubuzip/fixedcode.c
  src/wubuzip/limitcode.c
  src/wubuzip/lz77.c
  src/wubuzip/deflate.c
  src/wubuzip/block.c
  src/wubuzip/inflate.c)
target_include_directories(wubuzip PUBLIC src/wubuzip)

# --- wubuxml: streaming XML writer ---
add_library(wubuxml STATIC src/wubuxml/xml.c)
target_include_directories(wubuxml PUBLIC src/wubuxml)

# --- wubucfb: MS-CFB / OLE2 compound-file container reader + writer (legacy substrate) ---
add_library(wubucfb STATIC src/wubucfb/cfb.c src/wubucfb/cfb_write.c)
target_include_directories(wubucfb PUBLIC src/wubucfb)

# --- wubuformula: from-scratch formula engine (lexer/parser/AST/evaluator/functions) ---
add_library(wubuformula STATIC
  src/wubuformula/value.c
  src/wubuformula/value_util.c
  src/wubuformula/lexer.c
  src/wubuformula/ast.c
  src/wubuformula/parser.c
  src/wubuformula/eval.c
  src/wubuformula/functions/registry.c
  src/wubuformula/functions/logic.c
  src/wubuformula/functions/math.c
  src/wubuformula/functions/text.c
  src/wubuformula/functions/lookup.c
  src/wubuformula/functions/datefin.c
  src/wubuformula/functions/stat.c)
target_include_directories(wubuformula PUBLIC
  src/wubuformula
  src/wubuformula/functions)
target_link_libraries(wubuformula PUBLIC m)

# --- wubumodel: WS11 unified object model (shared substrate for all apps) ---
add_library(wubumodel STATIC
  src/wubumodel/model.c
  src/wubumodel/model_io.c
  src/wubumodel/doc_cache.c
  src/wubumodel/load_async.c)
target_include_directories(wubumodel PUBLIC src/wubumodel)
target_link_libraries(wubumodel PRIVATE wubuoxml pthread)

# ws07#1339: this is a NATIVE office suite. Reject Electron / JVM / any
# bundled web runtime at configure time so a dependency can never silently
# pull one in. Native C11 + pthreads only.
if(DEFINED ENV{ELECTRON})
  message(FATAL_ERROR "WuBuOffice is native-only: ELECTRON is not allowed "
          "(ws07#1339). Build with plain CMake, no Electron toolchain.")
endif()
if(DEFINED ENV{WUBOFFICE_ALLOW_JVM})
  message(WARNING "WUBOFFICE_ALLOW_JVM set: linking a JVM is permitted only for "
          "explicit server-side tooling, never the document apps (ws07#1339).")
endif()
find_library(JVM_LIB jvm)
if(JVM_LIB AND NOT WUBOFFICE_ALLOW_JVM)
  message(FATAL_ERROR "libjvm found on the link path and WUBOFFICE_ALLOW_JVM is "
          "not set. WuBuOffice document apps must not link the JVM (ws07#1339). "
          "Set -DWUBOFFICE_ALLOW_JVM=ON only for server-side tooling.")
endif()

# --- wubuoxml: OPC package writer + reader (built on wubuzip) ---
add_library(wubuoxml STATIC
  src/wubuoxml/package.c
  src/wubuoxml/rels_path.c
  src/wubuoxml/reader.c
  src/wubuoxml/extract.c
  src/wubuoxml/docx_document.c
  src/wubuxml/parser.c)
target_include_directories(wubuoxml PUBLIC src/wubuoxml)
target_link_libraries(wubuoxml PUBLIC wubuzip)

# --- app object libraries (shared between standalone exe and unified CLI) ---
add_library(wubuword_app OBJECT
  apps/wubuword/main.c apps/wubuword/word_doc.c apps/wubuword/word_table.c apps/wubuword/assemble.c)
target_include_directories(wubuword_app PRIVATE src/wubuoxml src/wubuxml apps/wubuword)
target_link_libraries(wubuword_app PRIVATE wubuoxml wubuxml)

add_library(wubucell_app OBJECT apps/wubucell/main.c apps/wubucell/cell_model.c apps/wubucell/cell_eval.c apps/wubucell/cell_io.c apps/wubucell/style.c apps/wubucell/cell_read.c apps/wubucell/cell_csv.c)
target_include_directories(wubucell_app PRIVATE src/wubuoxml src/wubuxml apps/wubucell src/wubuformula)
target_link_libraries(wubucell_app PRIVATE wubuoxml wubuxml wubuformula)

add_library(wubushow_app OBJECT apps/wubushow/main.c apps/wubushow/show_model.c apps/wubushow/show_io.c apps/wubushow/show_read.c)
target_include_directories(wubushow_app PRIVATE src/wubuoxml apps/wubushow)
target_link_libraries(wubushow_app PRIVATE wubuoxml)

add_library(wuburead_app OBJECT apps/wuburead/main.c apps/wuburead/read_rtf.c apps/wuburead/read_html.c apps/wuburead/read_epub.c)
target_include_directories(wuburead_app PRIVATE src/wubuoxml src/wubuzip src/wubuxml apps/wubuedit)
target_link_libraries(wuburead_app PRIVATE wubuoxml wubuxml wubuzip wubuedit_app)

add_library(wubuedit_app OBJECT apps/wubuedit/edit.c apps/wubuedit/docmodel.c)
target_include_directories(wubuedit_app PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow)
target_link_libraries(wubuedit_app PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)

# wubudoc: Markdown + HTML + RTF + JSON serializers over all models
add_library(wubudoc_app OBJECT apps/wubudoc/doc_md.c apps/wubudoc/doc_rtf.c apps/wubudoc/model_json.c apps/wubudoc/epub.c)
target_include_directories(wubudoc_app PRIVATE src/wubuoxml src/wubuxml src/wubuzip apps/wubuword apps/wubuedit apps/wubudoc apps/wubucell apps/wubushow)
target_link_libraries(wubudoc_app PRIVATE wubuoxml wubuxml wubuzip)

# wubuodf: OpenDocument (odt/ods/odp) read + write over the same three models
add_library(wubuodf_app OBJECT apps/wubuodf/odf_pkg.c apps/wubuodf/odf_body.c apps/wubuodf/odf_text.c apps/wubuodf/odf_sheet.c apps/wubuodf/odf_pres.c apps/wubuodf/odf_flat.c)
target_include_directories(wubuodf_app PRIVATE src/wubuoxml src/wubuxml src/wubuzip apps/wubuword apps/wubuedit apps/wubucell apps/wubushow apps/wubuodf)
target_link_libraries(wubuodf_app PRIVATE wubuoxml wubuxml wubuzip)
# wubuconv: unified format conversion (semantic models; the "treat as media"
# engine that gives WuBuOffice its superior formatting vs binary waterfalls)
add_library(wubuconv_app OBJECT
  apps/wubuconv/conv_map.c apps/wubuconv/conv_bridge.c apps/wubuconv/model_from_json.c)
target_include_directories(wubuconv_app PRIVATE
  src/wubuoxml src/wubuxml src/wubuzip src/wubucfb apps/wubuword apps/wubuedit
  apps/wubucell apps/wubushow apps/wubudoc apps/wubuodf apps/wubulegacy
  apps/wubupdf apps/wubuconv apps/wuburead src/wubujson)
target_link_libraries(wubuconv_app PUBLIC wubuoxml wubuxml wubuzip wubucfb wuburead_app wubujson)

# A real static lib wrapping the conv objects, so downstream link ordering
# (wubujson must precede the conv objects) is handled by CMake automatically.
# It also bundles wubushow_app/wubucell_app objects because conv_map calls into
# both (docx<->pptx bridges etc.), making wubuconv self-contained.
add_library(wubuconv STATIC
  $<TARGET_OBJECTS:wubuconv_app>
  $<TARGET_OBJECTS:wubushow_app>
  $<TARGET_OBJECTS:wubucell_app>
  $<TARGET_OBJECTS:wubuword_app>
  $<TARGET_OBJECTS:wubuedit_app>
  $<TARGET_OBJECTS:wubudoc_app>
  $<TARGET_OBJECTS:wubuodf_app>
  $<TARGET_OBJECTS:wubulegacy_app>
  $<TARGET_OBJECTS:wubupdf_app>)
target_link_libraries(wubuconv PUBLIC wubuoxml wubuxml wubuzip wubucfb wuburead_app wubujson wubuformula)

# wubulegacy: readers + writers for legacy binary Office (.doc/.xls/.ppt) over MS-CFB
add_library(wubulegacy_app OBJECT
  apps/wubulegacy/xls_biff.c apps/wubulegacy/doc_bin.c apps/wubulegacy/ppt_bin.c
  apps/wubulegacy/legacy_xls_write.c apps/wubulegacy/legacy_doc_write.c apps/wubulegacy/legacy_ppt_write.c)
target_include_directories(wubulegacy_app PRIVATE src/wubucfb apps/wubulegacy apps/wubucell apps/wubushow apps/wubuedit src/wubuoxml)
target_link_libraries(wubulegacy_app PRIVATE wubucfb)

# wubupdf: minimal PDF/1.7 writer over the TEXT model (dm_doc)
add_library(wubupdf_app OBJECT apps/wubupdf/pdf_text.c apps/wubupdf/pdf_write.c)
target_include_directories(wubupdf_app PRIVATE apps/wubupdf apps/wubuedit)

# --- standalone executables ---
add_executable(wubuword $<TARGET_OBJECTS:wubuword_app> apps/wubuword/entry.c)
target_link_libraries(wubuword PRIVATE wubuoxml wubuxml)
add_executable(wubucell $<TARGET_OBJECTS:wubucell_app> apps/wubucell/entry.c)
target_link_libraries(wubucell PRIVATE wubuoxml wubuformula)
add_executable(wubushow $<TARGET_OBJECTS:wubushow_app> apps/wubushow/entry.c)
target_link_libraries(wubushow PRIVATE wubuoxml)
add_executable(wuburead $<TARGET_OBJECTS:wuburead_app> apps/wuburead/entry.c)
target_link_libraries(wuburead PRIVATE wubuoxml wubuxml wubuzip wubuedit_app wubuword_app wubucell_app wubushow_app)
add_executable(wubuedit $<TARGET_OBJECTS:wubuedit_app> apps/wubuedit/entry.c)
target_link_libraries(wubuedit PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)

# --- unified CLI ---
add_executable(wubuoffice apps/wubuoffice/main.c tools/image2doc.c)
target_compile_definitions(wubuoffice PRIVATE WUBEOFFICE_EMBED)
target_include_directories(wubuoffice PRIVATE apps/wubuconv src/wubuocr src/wubuimage src/wubufont)
target_link_libraries(wubuoffice PRIVATE wubuoxml wubuxml wubuzip wubuformula wubucfb wubujson wubuconv crnnocr wubuimage wubufont m)

# --- wubujson: minimal JSON parse+emit (vendored from WuBuPad, same API) ---
add_library(wubujson STATIC src/wubujson/json.c)
target_include_directories(wubujson PUBLIC src/wubujson)

# --- wubusvg: clean-room SVG ingest + regurgitate (document engine) ---
add_library(wubusvg STATIC src/wubusvg/wubusvg.c src/wubusvg/agent.c src/wubuxml/parser.c)
target_include_directories(wubusvg PUBLIC src/wubusvg src/wubuxml)
target_link_libraries(wubusvg PUBLIC wubuxml wubujson)

# wubusvg CLI: ingest an SVG and regurgitate it (byte-safe round-trip)
add_executable(wubusvg_cli apps/wubusvg/main.c)
target_link_libraries(wubusvg_cli PRIVATE wubusvg)

# wubusvg_agent: NDJSON agent protocol (wubuOS "AGI GUI" surface)
add_executable(wubusvg_agent apps/wubusvg/agent_main.c)
target_link_libraries(wubusvg_agent PRIVATE wubusvg)

# --- wubudoc: unified document ingestion + creation facade (wubuOS bus) ---
add_library(wubudoc STATIC
    src/wubudoc/wubudoc.c
    src/wubudoc/b64.c
    src/wubudoc/agent.c)
target_include_directories(wubudoc PUBLIC
    src/wubudoc src/wubujson src/wubusvg src/wubufont src/wubuzip
    src/wubucfb src/wubuxml)
target_link_libraries(wubudoc PUBLIC wubusvg wubufont wubuzip wubucfb wubuxml wubujson wubuoxml wuburead_app wubuconv wubuimage)

# wubudoc_agent: NDJSON agent protocol unifying the whole format range
add_executable(wubudoc_agent apps/wubudoc/agent_main.c)
target_link_libraries(wubudoc_agent PRIVATE wubudoc)

# --- wubufont: clean-room SFNT/TrueType parser + SVG font emitter ---
add_library(wubufont STATIC src/wubufont/wubufont.c src/wubufont/woff.c)
target_include_directories(wubufont PUBLIC src/wubufont src/wubuzip)
target_link_libraries(wubufont PUBLIC wubuzip)

# wubufont CLI: sfnt (ttf/otf) -> SVG <font> document
add_executable(wubufont_cli apps/wubufont/main.c)
target_link_libraries(wubufont_cli PRIVATE wubufont)

# --- wubuocr: clean-room image -> structured document digestion ---
# Netpbm decode -> Otsu binarize -> XY-cut reading order -> connected components
# -> document JSON model. Dependency-free; recognizer is a pluggable slot.
add_library(wubuocr STATIC
    src/wubuocr/image.c src/wubuocr/binarize.c src/wubuocr/layout.c
    src/wubuocr/components.c src/wubuocr/wubuocr.c
    src/wubuocr/font8x8.c src/wubuocr/recognize.c
    src/wubuocr/fontbank.c src/wubuocr/gauntlet.c
    src/wubuocr/page_compose.c src/wubuocr/latin1.c
    src/wubuocr/unicode.c src/wubuocr/fontdir.c
    src/wubuocr/zoning.c src/wubuocr/mlp.c
    src/wubuocr/convnet.c src/wubuocr/convnet3.c
    src/wubuocr/stylebank.c src/wubuocr/dft.c src/wubuocr/goldplace.c
    src/wubuocr/crnn_transcribe.c src/wubuocr/docfmt.c src/wubuocr/pdfsearch.c
    src/wubuocr/qr.c
    src/wubuocr/gru_layout.c)
target_include_directories(wubuocr PUBLIC src/wubuocr src/wubujson src/wubufont)
target_link_libraries(wubuocr PUBLIC wubujson wubufont wubuzip crnnocr m)

# --- wubuimage: clean-room raster decoders (PNG; no libpng) ---
# PNG decode reuses the in-tree wubuzip DEFLATE engine; outputs an RGBA plane.
# img2doc bridges a decoded raster into the wubuocr pipeline (image -> doc).
# pdf_extract pulls text out of dropped PDFs (no third-party lib; FlateDecode
#   via wubuzip). All three are the "drag a document/image in" front-end.
add_library(wubuimage STATIC
    src/wubuimage/png.c src/wubuimage/img2doc.c src/wubuimage/imgops.c
    src/wubupdf/pdf_extract.c)
target_include_directories(wubuimage PUBLIC src/wubuimage src/wubuzip
                                     src/wubuocr src/wubufont src/wubupdf)
target_link_libraries(wubuimage PUBLIC wubuzip wubuocr wubufont m)

# wubuocr CLI: image -> ocr_page / document JSON
add_executable(wubuocr_cli apps/wubuocr/main.c)
target_link_libraries(wubuocr_cli PRIVATE wubuocr)

# wubugauntlet CLI: OCR robustness battery for the multi-font bank
add_executable(wubugauntlet_cli apps/wubugauntlet/main.c)
target_link_libraries(wubugauntlet_cli PRIVATE wubuocr wubufont)

# ocringest: end-to-end coordinate-aware OCR ingestion demo. Ties the whole
# pipeline together -- many real fonts -> golden-ratio warped layout ->
# deterministic ingest (binarize/XY-cut/components) -> per-glyph DFT compression
# + golden-region tags -> coordinate JSON. Depends on wubuocr + wubufont +
# wubujson + wubuzip (woff needs inflate).
add_executable(ocringest tools/ocringest.c)
target_link_libraries(ocringest PRIVATE wubuocr wubufont wubujson wubuzip m)

# --- Modern CRNN + CTC modular OCR pipeline (clean-room, dependency-free) ---
# Self-contained modules, opaque structs, no god headers:
#   crnn.c   sequence model (conv trunk + recurrent + CTC head), save/load
#   ctc.c    CTC loss (label-smooth + focal) + Graves two-score beam decode
#   rnn.c    LSTM core          gru.c  GRU core (extracted, single offset table)
#   lexicon.c  beam post-correction
#   lens.c   perspective flatten (Office-Lens corner warp)
#   png.c / png_encode.c  no-lib PNG decode/encode (reuses wubuzip inflate)
# The conv trunk + image plumbing are reused from the wubuocr lib
# (convnet3.c / image.c / mlp.c). Each module compiles and links independently.
add_library(crnnocr STATIC
    src/wubuocr/crnn.c
    src/wubuocr/ctc.c
    src/wubuocr/rnn.c
    $<$<BOOL:${WITH_CUDA}>:src/gpu/gru_gpu.c>
    src/wubuocr/gru.c
    src/wubuocr/ocl_gru.c
    src/wubuocr/lexicon.c
    src/wubuocr/lens.c
    src/wubuocr/png.c
    src/wubuocr/png_encode.c)
target_include_directories(crnnocr PUBLIC src/wubuocr src/wubufont src/wubujson src/wubuzip)
target_link_libraries(crnnocr PUBLIC wubuocr dl)
if(WITH_CUDA)
    target_link_libraries(crnnocr PUBLIC gpublas)
    # when CUDA is on, use the GPU-matmul GRU instead of the scalar one
    set_source_files_properties(src/wubuocr/gru.c PROPERTIES HEADER_FILE_ONLY ON)
    target_compile_definitions(crnnocr PUBLIC WITH_CUDA_BUILD)
endif()
# OpenCL fallback (#98): symbols are resolved at runtime via dlopen/dlsym inside
# ocl_gru.c, so there is NO link-time OpenCL dependency (the binary links cleanly
# with -static, and the GPU path activates only if libOpenCL.so is present on the
# host at runtime; otherwise gru_forward falls back to the CPU scalar path).
if(WITH_CUDA)
    target_link_libraries(crnnocr PUBLIC gpublas)
    # when CUDA is on, use the GPU-matmul GRU instead of the scalar one
    set_source_files_properties(src/wubuocr/gru.c PROPERTIES HEADER_FILE_ONLY ON)
    target_compile_definitions(crnnocr PUBLIC WITH_CUDA_BUILD)
endif()

# CRNN pipeline unit tests (built + registered as ctest).
add_executable(ctc_test tools/ctc_test.c src/wubuocr/ctc.c)
target_include_directories(ctc_test PRIVATE src/wubuocr)
target_link_libraries(ctc_test PRIVATE m)
add_test(NAME ctc_test COMMAND ctc_test)

add_executable(gru_test tools/gru_test.c)
target_include_directories(gru_test PRIVATE src/wubuocr)
# Pull the active GRU/LSTM from crnnocr (under WITH_CUDA that is the GPU
# matmul GRU; otherwise the scalar one). Do NOT compile gru.c/rnn.c inline
# here -- with WITH_CUDA=ON those sources are HEADER_FILE_ONLY and would
# yield no object (undefined gru_create at link).
target_link_libraries(gru_test PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME gru_test COMMAND gru_test)

# OpenCL GRU fallback (#98): numeric equivalence to the scalar CPU forward.
add_executable(ocl_gru_test tools/ocl_gru_test.c src/wubuocr/ocl_gru.c)
target_include_directories(ocl_gru_test PRIVATE src/wubuocr)
target_link_libraries(ocl_gru_test PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME ocl_gru COMMAND ocl_gru_test)

# Math/equation line detector (#48).
add_executable(test_math tools/test_math.c)
target_include_directories(test_math PRIVATE src/wubuocr)
target_link_libraries(test_math PRIVATE wubuocr m)
add_test(NAME test_math COMMAND test_math)

# QR-code codec (#49): encode -> matrix -> decode round-trip, plus image acquire.
add_executable(test_qr tools/test_qr.c)
target_include_directories(test_qr PRIVATE src/wubuocr)
target_link_libraries(test_qr PRIVATE wubuocr m)
add_test(NAME test_qr COMMAND test_qr)

add_executable(rnn_test tools/rnn_test.c)
target_include_directories(rnn_test PRIVATE src/wubuocr)
target_link_libraries(rnn_test PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME rnn_test COMMAND rnn_test)

add_executable(gradcheck_conv3 tools/gradcheck_conv3.c
               src/wubuocr/convnet3.c src/wubuocr/mlp.c)
target_include_directories(gradcheck_conv3 PRIVATE src/wubuocr)
target_link_libraries(gradcheck_conv3 PRIVATE m)
add_test(NAME gradcheck_conv3 COMMAND gradcheck_conv3)

# CRNN training / inference tools (render glyphs -> train -> recognize).
foreach(t crnn_train crnn_img_train crnn_ocr_train crnn_lex_train crnn_warp_train)
  add_executable(${t} tools/${t}.c)
  target_include_directories(${t} PRIVATE src/wubuocr src/wubufont)
  target_link_libraries(${t} PRIVATE crnnocr wubufont m)
endforeach()
add_executable(crnn_doc_demo tools/crnn_doc_demo.c)
target_include_directories(crnn_doc_demo PRIVATE src/wubuocr src/wubufont)
target_link_libraries(crnn_doc_demo PRIVATE crnnocr wubufont m)
add_executable(crnn_photo_demo tools/crnn_photo_demo.c)
target_include_directories(crnn_photo_demo PRIVATE src/wubuocr src/wubufont)
target_link_libraries(crnn_photo_demo PRIVATE crnnocr wubufont m)

# image2doc: page image -> editable document via the CRNN line recognizer.
# PNG -> OcrImage -> crnn_transcribe (docmodel JSON) -> wubuconv (docx/md/...).
add_executable(image2doc tools/image2doc.c)
target_include_directories(image2doc PRIVATE src/wubuocr src/wubuimage src/wubufont apps/wubuconv)
target_link_libraries(image2doc PRIVATE crnnocr wubuimage wubuconv wubujson wubufont pthread m)

# gen_pgmpage: synthetic multi-line Latin page (PGM) for image2doc testing.
add_executable(gen_pgmpage tools/gen_pgmpage.c)
target_include_directories(gen_pgmpage PRIVATE src/wubuocr src/wubufont)
target_link_libraries(gen_pgmpage PRIVATE wubuocr wubufont m)

# Synthetic skewed+noisy PNG page (photo-like validation input)
add_executable(gen_realpage tools/gen_realpage.c)
target_include_directories(gen_realpage PRIVATE src/wubuocr src/wubufont)
target_link_libraries(gen_realpage PRIVATE crnnocr wubuimage wubufont m)

# crnn_save/crnn_load fidelity probe
add_executable(crnn_roundtrip tools/crnn_roundtrip.c)
target_include_directories(crnn_roundtrip PRIVATE src/wubuocr src/wubufont)
target_link_libraries(crnn_roundtrip PRIVATE crnnocr wubufont m)
add_executable(fontcpgrep tools/fontcpgrep.c)
target_include_directories(fontcpgrep PRIVATE src/wubuocr src/wubufont)
target_link_libraries(fontcpgrep PRIVATE crnnocr wubufont m)
add_executable(charsetgen tools/charsetgen.c)
target_include_directories(charsetgen PRIVATE src/wubuocr src/wubufont)
target_link_libraries(charsetgen PRIVATE crnnocr wubufont m)

# --- wubutui: native terminal UI toolkit (human GUIs) ---
# Pure C11 render/input core (screen buffer, ANSI diff, key decode, word wrap,
# draw primitives) + a thin POSIX termios edge (term.c). No dependency, no
# toolkit -- consistent with the headless suite. The pure modules are unit-
# tested without a TTY; term.c isolates the impure edge.
add_library(wubutui STATIC
    src/wubutui/screen.c src/wubutui/input.c
    src/wubutui/draw.c   src/wubutui/term.c)
target_include_directories(wubutui PUBLIC src/wubutui)

# wubunote_core: pure text-buffer model (Notepad++-class editing core).
# Self-contained, allocation-checked, unit-tested without a TTY.
add_library(wubunote_core STATIC src/wubunote/edit.c)
target_include_directories(wubunote_core PUBLIC src/wubunote)

# wubunote: tabbed terminal text editor (Notepad++-class UI).
# Pure controller (tabs + editing + prompt/palette) is unit-tested; main.c
# is the thin TTY driver. Reuses the wubunote editing core.
add_executable(wubunote apps/wubunote/entry.c apps/wubunote/main.c
                      apps/wubunote/note_controller.c)
target_include_directories(wubunote PRIVATE apps/wubunote src/wubunote src/wubutui src/wubudoc)
target_link_libraries(wubunote PRIVATE wubunote_core wubutui wubudoc)

# wubunote controller test (real editor interaction path)
add_executable(test_wubunote_ctrl tests/test_wubunote_ctrl.c
                          apps/wubunote/note_controller.c)
target_include_directories(test_wubunote_ctrl PRIVATE apps/wubunote src/wubunote src/wubutui)
target_link_libraries(test_wubunote_ctrl PRIVATE wubunote_core wubutui)
add_test(NAME wubunote_ctrl COMMAND test_wubunote_ctrl)

# wubunote test (pure editing core)
add_executable(test_wubunote tests/test_wubuedit.c)
target_link_libraries(test_wubunote PRIVATE wubunote_core)
add_test(NAME wubunote COMMAND test_wubunote)

# --- tests ---
enable_testing()
add_executable(test_wubuzip tests/test_wubuzip.c)
target_link_libraries(test_wubuzip PRIVATE wubuzip)
add_test(NAME wubuzip_crc COMMAND test_wubuzip)

add_executable(test_inflate tests/test_inflate.c)
target_link_libraries(test_inflate PRIVATE wubuzip z)
add_test(NAME inflate COMMAND test_inflate)

add_executable(test_formula tests/test_formula.c)
target_link_libraries(test_formula PRIVATE wubuformula)
target_include_directories(test_formula PRIVATE src/wubuformula/functions)
add_test(NAME formula COMMAND test_formula)

add_executable(test_model tests/test_model.c)
target_link_libraries(test_model PRIVATE wubumodel)
add_test(NAME model COMMAND test_model)

add_executable(test_doc_cache tests/test_doc_cache.c)
target_link_libraries(test_doc_cache PRIVATE wubumodel)
add_test(NAME doc_cache COMMAND test_doc_cache)

add_executable(test_load_async tests/test_load_async.c)
target_link_libraries(test_load_async PRIVATE wubumodel)
add_test(NAME load_async COMMAND test_load_async)

add_executable(test_deflate_roundtrip tests/test_deflate_roundtrip.c)
target_link_libraries(test_deflate_roundtrip PRIVATE wubuzip)
add_test(NAME deflate_roundtrip COMMAND test_deflate_roundtrip)

add_executable(test_cold_start tests/test_cold_start.c)
target_link_libraries(test_cold_start PRIVATE wubumodel)
add_test(NAME cold_start COMMAND test_cold_start)

add_executable(test_model_io_roundtrip tests/test_model_io_roundtrip.c)
target_link_libraries(test_model_io_roundtrip PRIVATE wubumodel)
add_test(NAME model_io_roundtrip COMMAND test_model_io_roundtrip)

add_executable(test_wubuxml_parser tests/test_wubuxml_parser.c)
target_link_libraries(test_wubuxml_parser PRIVATE wubuoxml)
target_include_directories(test_wubuxml_parser PRIVATE src/wubuxml)
add_test(NAME wubuxml_parser COMMAND test_wubuxml_parser)

add_executable(test_reader tests/test_reader.c)
target_link_libraries(test_reader PRIVATE wubuoxml wubuword_app)
target_include_directories(test_reader PRIVATE src/wubuoxml src/wubuxml apps/wubuword)
add_test(NAME reader_roundtrip COMMAND test_reader)

add_executable(test_extract tests/test_extract.c)
target_link_libraries(test_extract PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_extract PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow)
add_test(NAME extract COMMAND test_extract)

add_executable(test_edit tests/test_edit.c)
target_link_libraries(test_edit PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app wubuedit_app)
target_include_directories(test_edit PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow apps/wubuedit)
add_test(NAME edit_roundtrip COMMAND test_edit)

add_executable(test_conformance tests/test_conformance.c)
target_link_libraries(test_conformance PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_conformance PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow)
target_compile_definitions(test_conformance PRIVATE CONFORMANCE_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/validate.py")
add_test(NAME conformance COMMAND test_conformance)

# Robustness gate: read files produced by INDEPENDENT tools (openpyxl,
# python-pptx). SKIPs cleanly if those optional libs are absent.
add_executable(test_foreign tests/test_foreign.c)
target_link_libraries(test_foreign PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_foreign PRIVATE src/wubuoxml src/wubuxml apps/wubuword apps/wubucell apps/wubushow)
target_compile_definitions(test_foreign PRIVATE FOREIGN_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/produce_foreign.py")
add_test(NAME foreign_read COMMAND test_foreign)

add_executable(test_cell_read tests/test_cell_read.c)
target_link_libraries(test_cell_read PRIVATE wubucell_app wubuoxml wubuxml wubuformula)
target_include_directories(test_cell_read PRIVATE apps/wubucell src/wubuoxml src/wubuxml src/wubuformula src/wubuformula/functions)
add_test(NAME cell_read COMMAND test_cell_read)

add_executable(test_cell_cycle tests/test_cell_cycle.c)
target_link_libraries(test_cell_cycle PRIVATE wubucell_app wubuformula)
target_include_directories(test_cell_cycle PRIVATE apps/wubucell src/wubuformula src/wubuformula/functions)
add_test(NAME cell_cycle COMMAND test_cell_cycle)

add_executable(test_csv tests/test_csv.c)
target_link_libraries(test_csv PRIVATE wubucell_app wubuoxml wubuxml wubuformula)
target_include_directories(test_csv PRIVATE apps/wubucell src/wubuoxml src/wubuxml src/wubuformula src/wubuformula/functions)
add_test(NAME csv COMMAND test_csv)

add_executable(test_md tests/test_md.c $<TARGET_OBJECTS:wubudoc_app>)
target_link_libraries(test_md PRIVATE wubuword_app wubuedit_app wubuoxml wubuxml wubucell_app wubushow_app wubuformula)
target_include_directories(test_md PRIVATE apps/wubudoc apps/wubuword apps/wubuedit src/wubuoxml src/wubuxml)
add_test(NAME md COMMAND test_md)

add_executable(test_rtf_json tests/test_rtf_json.c $<TARGET_OBJECTS:wubudoc_app>)
target_link_libraries(test_rtf_json PRIVATE wubuword_app wubuedit_app wubucell_app wubushow_app wubuoxml wubuxml wubuformula)
target_include_directories(test_rtf_json PRIVATE apps/wubudoc apps/wubuword apps/wubuedit apps/wubucell apps/wubushow src/wubuoxml src/wubuxml)
add_test(NAME rtf_json COMMAND test_rtf_json)

add_executable(test_odf tests/test_odf.c)
target_link_libraries(test_odf PRIVATE wubuword_app wubuedit_app wubucell_app wubushow_app wubuoxml wubuxml wubuzip wubuformula wubuconv)
target_include_directories(test_odf PRIVATE apps/wubuodf apps/wubuedit apps/wubucell apps/wubushow src/wubuoxml src/wubuxml src/wubuzip)
target_compile_definitions(test_odf PRIVATE ODF_ORACLE="${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/odf_oracle.py")
add_test(NAME odf COMMAND test_odf)

add_executable(test_convert tests/test_convert.c)
target_link_libraries(test_convert PRIVATE wubuword_app wubuedit_app wubucell_app wubushow_app wubuoxml wubuxml wubuzip wubuformula wubucfb wubujson wubuconv)
target_include_directories(test_convert PRIVATE apps/wubuconv apps/wubuodf apps/wubudoc apps/wubuedit apps/wubucell apps/wubushow apps/wubulegacy apps/wubupdf src/wubuoxml src/wubuxml src/wubuzip src/wubucfb)
target_compile_definitions(test_convert PRIVATE
    WUBUOFFICE_BIN="${CMAKE_CURRENT_BINARY_DIR}/wubuoffice"
    ODF_ORACLE_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/odf_oracle.py")
add_test(NAME convert COMMAND test_convert)

# Reverse-direction readers (rtf/html/epub -> dm_doc) round-trip.
add_executable(test_readers tests/test_readers.c)
target_link_libraries(test_readers PRIVATE wubuoxml wubuxml wubuzip wubucfb wubuformula wubuedit_app wubujson wubuconv)
target_include_directories(test_readers PRIVATE
  apps/wuburead apps/wubuconv apps/wubuword apps/wubuedit apps/wubudoc
  apps/wubuodf apps/wubulegacy apps/wubupdf src/wubuoxml src/wubuxml src/wubuzip src/wubucfb)
add_test(NAME readers COMMAND test_readers)

# Legacy binary readers (.doc/.xls/.ppt) + PDF writer, validated by the
# independent xlwt/xlrd + pypdf/pdfminer oracles. SKIPs cleanly if absent.
add_executable(test_legacy_pdf tests/test_legacy_pdf.c
  $<TARGET_OBJECTS:wubulegacy_app> $<TARGET_OBJECTS:wubupdf_app>
  $<TARGET_OBJECTS:wubucell_app> $<TARGET_OBJECTS:wubushow_app>
  $<TARGET_OBJECTS:wubuword_app> $<TARGET_OBJECTS:wubuedit_app>)
target_link_libraries(test_legacy_pdf PRIVATE wubucfb wubuoxml wubuxml wubuzip wubuformula)
target_include_directories(test_legacy_pdf PRIVATE apps/wubulegacy apps/wubupdf apps/wubucell apps/wubushow apps/wubuedit src/wubucfb src/wubuoxml src/wubuxml)
target_compile_definitions(test_legacy_pdf PRIVATE LEGACY_ORACLE="${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/legacy_oracle.py")
add_test(NAME legacy_pdf COMMAND test_legacy_pdf)

# Flat ODF (.fodt/.fods/.fodp) round trips + EPUB writer, validated by a stdlib
# XML oracle (flat) and EbookLib (epub, SKIPs cleanly if absent).
add_executable(test_flat_epub tests/test_flat_epub.c
  $<TARGET_OBJECTS:wubuodf_app> $<TARGET_OBJECTS:wubudoc_app>
  $<TARGET_OBJECTS:wubucell_app> $<TARGET_OBJECTS:wubushow_app>
  $<TARGET_OBJECTS:wubuword_app> $<TARGET_OBJECTS:wubuedit_app>)
target_link_libraries(test_flat_epub PRIVATE wubuoxml wubuxml wubuzip wubuformula)
target_include_directories(test_flat_epub PRIVATE apps/wubuodf apps/wubudoc apps/wubucell apps/wubushow apps/wubuword apps/wubuedit src/wubuoxml src/wubuxml src/wubuzip)
target_compile_definitions(test_flat_epub PRIVATE FLAT_EPUB_ORACLE="${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/flat_epub_oracle.py")
add_test(NAME flat_epub COMMAND test_flat_epub)

# wubufont: SFNT/TrueType parser + SVG font emitter (validates a real system font).
add_executable(test_wubufont tests/test_wubufont.c)
target_link_libraries(test_wubufont PRIVATE wubufont)
add_test(NAME wubufont COMMAND test_wubufont)

# wubuocr: image -> document pipeline (Otsu, XY-cut reading order, components).
add_executable(test_wubuocr tests/test_wubuocr.c)
target_link_libraries(test_wubuocr PRIVATE wubuocr)
# CRNN line transcribe -> docmodel JSON (document path regression test)
add_executable(test_crnn_transcribe tests/test_crnn_transcribe.c)
target_include_directories(test_crnn_transcribe PRIVATE src/wubuocr src/wubufont)
target_link_libraries(test_crnn_transcribe PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME crnn_transcribe COMMAND test_crnn_transcribe)
set_tests_properties(crnn_transcribe PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# CRNN line transcribe -> docmodel JSON (full document charset regression test)
add_executable(test_crnn_transcribe_full tests/test_crnn_transcribe_full.c)
target_include_directories(test_crnn_transcribe_full PRIVATE src/wubuocr src/wubufont)
target_link_libraries(test_crnn_transcribe_full PRIVATE crnnocr wubuimage wubufont m)
add_test(NAME crnn_transcribe_full COMMAND test_crnn_transcribe_full)
set_tests_properties(crnn_transcribe_full PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Multi-column detection (1/2/3-col) end-to-end via crnn_transcribe_page_json
add_executable(test_columns tests/test_columns.c)
target_include_directories(test_columns PRIVATE src/wubuocr src/wubufont)
target_link_libraries(test_columns PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME columns COMMAND test_columns)

# Binarize unit tests: Sauvola / Otsu / despeckle / auto-crop / empty-page
add_executable(test_imageops tests/test_imageops.c)
target_include_directories(test_imageops PRIVATE src/wubuocr)
target_link_libraries(test_imageops PRIVATE wubuocr m)

# xlsx (Excel OOXML) export: docmodel JSON -> valid .xlsx zip with 6 OOXML parts
add_executable(test_xlsx tests/test_xlsx.c)
target_include_directories(test_xlsx PRIVATE src/wubuocr src/wubuzip)
target_link_libraries(test_xlsx PRIVATE wubuocr wubuzip m)
add_test(NAME xlsx COMMAND test_xlsx)

# docfmt2 unit test: TEI serialization + reading-order/confidence fields
add_executable(test_docfmt2 tests/test_docfmt2.c)
target_include_directories(test_docfmt2 PRIVATE src/wubuocr)
target_link_libraries(test_docfmt2 PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME docfmt2 COMMAND test_docfmt2)

# pdfua unit test: PDF/UA structure tree + language tag
add_executable(test_pdfua tests/test_pdfua.c)
target_include_directories(test_pdfua PRIVATE src/wubuocr)
target_link_libraries(test_pdfua PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME pdfua COMMAND test_pdfua)

# nfc unit test: Unicode NFC Latin composition
add_executable(test_nfc tests/test_nfc.c)
target_include_directories(test_nfc PRIVATE src/wubuocr)
target_link_libraries(test_nfc PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME nfc COMMAND test_nfc)
add_test(NAME imageops COMMAND test_imageops)

# imgops: image preprocessing (rotate / contrast / median / shading / sharpen)
add_executable(test_imgops tests/test_imgops.c)
target_include_directories(test_imgops PRIVATE src/wubuimage src/wubuocr)
target_link_libraries(test_imgops PRIVATE wubuimage wubuocr m)
add_test(NAME imgops COMMAND test_imgops)

# docfmt: alternate document serializations (txt/tsv/csv/jsonl/latex/rtf/hocr/alto)
add_executable(test_docfmt tests/test_docfmt.c)
target_include_directories(test_docfmt PRIVATE src/wubuocr src/wubujson)
target_link_libraries(test_docfmt PRIVATE wubuocr wubujson m)
add_test(NAME docfmt COMMAND test_docfmt)


add_executable(test_ocr_realpage tests/test_ocr_realpage.c)
target_include_directories(test_ocr_realpage PRIVATE src/wubuocr src/wubufont)
target_link_libraries(test_ocr_realpage PRIVATE crnnocr wubuimage wubufont m)
add_test(NAME ocr_realpage COMMAND test_ocr_realpage)

add_test(NAME wubuocr COMMAND test_wubuocr)

# wubuocr recognizer: zoning + 1-NN glyph classification (render->recognize).
add_executable(test_wubuocr_recognize tests/test_wubuocr_recognize.c)
target_link_libraries(test_wubuocr_recognize PRIVATE wubuocr)
add_test(NAME wubuocr_recognize COMMAND test_wubuocr_recognize)

# wubuocr fontbank: multi-font bank built from REAL font rasterizations.
add_executable(test_fontbank tests/test_fontbank.c)
target_link_libraries(test_fontbank PRIVATE wubuocr wubufont)
add_test(NAME fontbank COMMAND test_fontbank)

# wubugauntlet: OCR robustness battery for the multi-font bank.
add_executable(test_gauntlet tests/test_gauntlet.c)
target_link_libraries(test_gauntlet PRIVATE wubuocr wubufont)
add_test(NAME gauntlet COMMAND test_gauntlet)

# wubuocr Latin-1 tier: English-first, Latin second (accented glyph recall).
add_executable(test_latin1 tests/test_latin1.c)
target_link_libraries(test_latin1 PRIVATE wubuocr wubufont)
add_test(NAME latin1 COMMAND test_latin1)

# wubuocr EMNIST Letters training/inference tools (lightweight C11 OCR).
#   emnist_train : train the zoning+MLP recognizer with the wubu Riemannian SGD
#                  optimizer (needs WuBuMath). See tools/build_wubu_ocr.sh.
#   emnist_infer : dependency-free inference (zoning + mlp modules only).
# These need the downloaded IDX corpus under data/emnist/ (tools/fetch_emnist.sh)
# and are NOT ctests. Run manually, e.g.:
#   ./build/emnist_train data/emnist 12 128 64 40
#   ./build/emnist_infer  data/emnist build/emnist_wubu_mlp_g12_h128_64.wts
find_path(WUBUMATH_INCLUDE_DIR NAMES wubumath.h
          PATHS ${CMAKE_SOURCE_DIR}/../WuBuMath/include
                $ENV{HOME}/WuBuMath/include
          NO_DEFAULT_PATH)
find_library(WUBUMATH_SGD  NAMES wubu_riemannian_sgd
             PATHS ${CMAKE_SOURCE_DIR}/../WuBuMath/src/train
                   $ENV{HOME}/WuBuMath/src/train NO_DEFAULT_PATH)
if (WUBUMATH_INCLUDE_DIR AND EXISTS "${WUBUMATH_INCLUDE_DIR}/wubu_riemannian_sgd.h")
  add_executable(emnist_train tools/emnist_train.c src/wubuocr/zoning.c src/wubuocr/mlp.c)
  target_include_directories(emnist_train PRIVATE ${WUBUMATH_INCLUDE_DIR} src/wubuocr)
  target_sources(emnist_train PRIVATE
    ${WUBUMATH_INCLUDE_DIR}/../src/train/wubu_riemannian_sgd.c
    ${WUBUMATH_INCLUDE_DIR}/../src/train/wubu_q_controller.c
    ${WUBUMATH_INCLUDE_DIR}/../src/math/wubu_utils.c
    ${WUBUMATH_INCLUDE_DIR}/../src/math/wubu_hyperbolic.c)
  target_link_libraries(emnist_train PRIVATE m)
endif()

add_executable(emnist_infer tools/emnist_infer.c src/wubuocr/zoning.c src/wubuocr/mlp.c)
target_include_directories(emnist_infer PRIVATE src/wubuocr)
target_link_libraries(emnist_infer PRIVATE m)

# emnist_train_conv / emnist_infer_conv: ultra-light conv+MLP (dependency-free
# plain C11 SGD). The "2011 single-core" build: convnet + mlp modules only.
# emnist_infer_conv: evaluates the trained conv3+MLP on the EMNIST Letters test
# set (full accuracy + per-class). Uses the live convnet3 API (the trainer
# emnist_train_conv3 saves conv3.wts / conv3_mlp.wts). Supersedes the retired
# convnet.c-based emnist_train_conv / emnist_infer_conv pair.
add_executable(emnist_infer_conv tools/emnist_infer_conv.c
               src/wubuocr/convnet3.c src/wubuocr/mlp.c
               src/wubuocr/dft.c src/wubuocr/goldplace.c
               src/wubuocr/stylebank.c src/wubuocr/zoning.c)
target_include_directories(emnist_infer_conv PRIVATE src/wubuocr)
target_link_libraries(emnist_infer_conv PRIVATE m)

# wubuocr: zoning + mlp module unit tests (dependency-free, fast).
add_executable(test_zoning tests/test_zoning.c src/wubuocr/zoning.c)
target_include_directories(test_zoning PRIVATE src/wubuocr)
add_test(NAME zoning COMMAND test_zoning)

add_executable(test_mlp tests/test_mlp.c src/wubuocr/mlp.c)
target_include_directories(test_mlp PRIVATE src/wubuocr)
target_link_libraries(test_mlp PRIVATE m)
add_test(NAME mlp COMMAND test_mlp)

# conv+MLP gradient pipeline test (end-to-end on a separable problem).
add_executable(test_convnet tests/test_convnet.c
               src/wubuocr/convnet.c src/wubuocr/mlp.c)
target_include_directories(test_convnet PRIVATE src/wubuocr)
target_link_libraries(test_convnet PRIVATE m)
add_test(NAME convnet COMMAND test_convnet)

# conv3 (3-stage MED) gradient pipeline test (same decisive proof).
add_executable(test_convnet3 tests/test_convnet3.c
               src/wubuocr/convnet3.c src/wubuocr/mlp.c)
target_include_directories(test_convnet3 PRIVATE src/wubuocr)
target_link_libraries(test_convnet3 PRIVATE m)
add_test(NAME convnet3 COMMAND test_convnet3)

# im2col+GEMM conv rewrite: numeric equivalence to brute-force conv + bias-grad FD check.
add_executable(test_convnet3_im2col tools/test_convnet3_im2col.c
               src/wubuocr/convnet3.c src/wubuocr/mlp.c
               src/wubuocr/dft.c src/wubuocr/goldplace.c
               src/wubuocr/stylebank.c src/wubuocr/zoning.c)
target_include_directories(test_convnet3_im2col PRIVATE src/wubuocr)
target_link_libraries(test_convnet3_im2col PRIVATE m)
add_test(NAME convnet3_im2col COMMAND test_convnet3_im2col)

# instance-norm (batch-free BatchNorm) gamma/beta gradient finite-diff check.
add_executable(gradcheck_inorm tests/gradcheck_inorm.c
               src/wubuocr/convnet3.c src/wubuocr/mlp.c)
target_include_directories(gradcheck_inorm PRIVATE src/wubuocr)
target_link_libraries(gradcheck_inorm PRIVATE m)
add_test(NAME inorm_gradcheck COMMAND gradcheck_inorm)

# stylebank: 64MB multi-style expert bank (56 best + 8 rolling) smoke test.
add_executable(test_stylebank tests/test_stylebank.c
               src/wubuocr/stylebank.c src/wubuocr/convnet3.c src/wubuocr/mlp.c)
target_include_directories(test_stylebank PRIVATE src/wubuocr)
target_link_libraries(test_stylebank PRIVATE m)
add_test(NAME stylebank COMMAND test_stylebank)

# emnist_train_conv3 / emnist_infer_conv3: 3-stage MED conv+MLP (2011 build).
add_executable(emnist_train_conv3 tools/emnist_train_conv3.c
               src/wubuocr/convnet3.c src/wubuocr/mlp.c)
target_include_directories(emnist_train_conv3 PRIVATE src/wubuocr)
target_link_libraries(emnist_train_conv3 PRIVATE m pthread)
add_test(NAME emnist_train_conv3_smoke COMMAND emnist_train_conv3 data/emnist 128 64 2 2000)
set_tests_properties(emnist_train_conv3_smoke PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Modular trainer CLI
add_executable(trainer_cli tools/trainer_cli.c src/wubuocr/trainer.c
               src/wubuocr/convnet3.c src/wubuocr/mlp.c)
target_include_directories(trainer_cli PRIVATE src/wubuocr)
target_link_libraries(trainer_cli PRIVATE m pthread)

# Fast validation trainer (small net, few epochs, capped data)
add_executable(emnist_train_fast tools/emnist_train_fast.c src/wubuocr/convnet3.c src/wubuocr/mlp.c)
target_include_directories(emnist_train_fast PRIVATE src/wubuocr)
target_link_libraries(emnist_train_fast PRIVATE m pthread)



# wubuview: docflat model->text flattening (pure).
add_executable(test_wubuview tests/test_wubuview.c apps/wubuview/docflat.c)
target_include_directories(test_wubuview PRIVATE apps/wubuview src/wubujson)
target_link_libraries(test_wubuview PRIVATE wubujson)
add_test(NAME wubuview COMMAND test_wubuview)

# wubuview: tabbed terminal document viewer (drag/drop + multi-format).
add_executable(wubuview apps/wubuview/entry.c apps/wubuview/main.c
                     apps/wubuview/controller.c apps/wubuview/docflat.c)
target_include_directories(wubuview PRIVATE apps/wubuview src/wubujson
                                     src/wubutui src/wubudoc)
target_link_libraries(wubuview PRIVATE wubutui wubudoc wubujson)

# wubuview controller: pure event->state mapping (real app glue path)
add_executable(test_wubuview_ctrl tests/test_wubuview_ctrl.c apps/wubuview/controller.c)
target_include_directories(test_wubuview_ctrl PRIVATE apps/wubuview src/wubutui)
target_link_libraries(test_wubuview_ctrl PRIVATE wubutui)
add_test(NAME wubuview_ctrl COMMAND test_wubuview_ctrl)

# wubusvg: SVG ingest + regurgitate round-trip.
add_executable(test_wubusvg tests/test_wubusvg.c)
target_link_libraries(test_wubusvg PRIVATE wubusvg)
add_test(NAME wubusvg COMMAND test_wubusvg)

# wubudoc agent test
add_executable(test_wubudoc tests/test_wubudoc.c)
target_link_libraries(test_wubudoc PRIVATE wubudoc)
add_test(NAME wubudoc COMMAND test_wubudoc)

# drag/drop + bracketed-paste ingestion (magic-byte dispatch, paste decode)
add_executable(test_drop tests/test_drop.c)
target_link_libraries(test_drop PRIVATE wubudoc wubutui wubuimage)
add_test(NAME drop COMMAND test_drop)

# wubusvg agent: NDJSON protocol for wubuOS (mirrors WuBuPad agent).
add_executable(test_wubusvg_agent tests/test_wubusvg_agent.c)
target_link_libraries(test_wubusvg_agent PRIVATE wubusvg)
add_test(NAME wubusvg_agent COMMAND test_wubusvg_agent)

# ---------------------------------------------------------------------------
# Optional CUDA training backend (C11 + .cu kernels, nvcc).
# OFF by default so the project builds with no CUDA dependency. When ON and a
# CUDA toolkit is found, gpu_blas.cu (real kernels) is compiled; otherwise the
# CPU stub is used and gpu_available() returns 0. The on-disk model format and
# the C inference path are unaffected either way.
# ---------------------------------------------------------------------------
option(WITH_CUDA "Build the CUDA training backend (requires nvcc)" OFF)
set(CUDA_LIBS "")
if(WITH_CUDA)
  find_program(NVCC_EXECUTABLE nvcc PATHS /usr/local/cuda/bin ENV PATH)
  if(NVCC_EXECUTABLE)
    message(STATUS "CUDA enabled: using ${NVCC_EXECUTABLE}")
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 11)
    # gpu_blas.cu is the real implementation; do NOT compile the stub.
    add_library(gpublas STATIC src/gpu/gpu_blas.cu)
    target_include_directories(gpublas PUBLIC src/gpu)
    target_compile_options(gpublas PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:
        --gpu-architecture=sm_89 -O2>)
    find_library(CUDA_RUNTIME_LIB cuda HINTS /usr/local/cuda/targets/x86_64-linux/lib)
    set(CUDA_LIBS gpublas ${CUDA_RUNTIME_LIB})
    add_compile_definitions(WITH_CUDA_BUILD)
  else()
    message(WARNING "WITH_CUDA=ON but nvcc not found; falling back to CPU stub")
    add_library(gpublas STATIC src/gpu/gpu_blas_stub.c)
    target_include_directories(gpublas PUBLIC src/gpu)
    set(CUDA_LIBS gpublas)
  endif()
else()
  add_library(gpublas STATIC src/gpu/gpu_blas_stub.c)
  target_include_directories(gpublas PUBLIC src/gpu)
  set(CUDA_LIBS gpublas)
endif()

# Numerical parity test: CUDA kernels vs CPU reference.
add_executable(test_gpu_blas tests/test_gpu_blas.c)
target_link_libraries(test_gpu_blas PRIVATE gpublas m)
add_test(NAME gpu_blas COMMAND test_gpu_blas)

# Finite-difference gradcheck for the GPU-matmul GRU (forward + BPTT).
add_executable(test_gru_gpu tests/test_gru_gpu.c src/gpu/gru_gpu.c src/wubuocr/gru_layout.c)
target_include_directories(test_gru_gpu PRIVATE src/wubuocr src/gpu)
target_compile_definitions(test_gru_gpu PRIVATE WITH_CUDA_BUILD)
target_link_libraries(test_gru_gpu PRIVATE gpublas m)
add_test(NAME gru_gpu_gradcheck COMMAND test_gru_gpu)

add_executable(test_gemm_nt tests/test_gemm_nt.c)
target_link_libraries(test_gemm_nt PRIVATE gpublas m)
add_test(NAME gemm_nt COMMAND test_gemm_nt)

add_executable(test_gru_cmp tests/test_gru_cmp.c src/gpu/gru_gpu.c src/wubuocr/gru_layout.c src/wubuocr/ocl_gru.c)
target_include_directories(test_gru_cmp PRIVATE src/wubuocr src/gpu)
target_compile_definitions(test_gru_cmp PRIVATE WITH_CUDA_BUILD)
target_link_libraries(test_gru_cmp PRIVATE gpublas m)
add_test(NAME gru_cmp COMMAND test_gru_cmp)
