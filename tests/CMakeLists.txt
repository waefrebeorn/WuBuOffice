# WuBuOffice regression tests (ctest). Mirrors the original monolith set.

add_executable(test_wubuzip test_wubuzip.c)
target_link_libraries(test_wubuzip PRIVATE wubuzip)
add_test(NAME wubuzip_crc COMMAND test_wubuzip)

add_executable(test_inflate test_inflate.c)
target_link_libraries(test_inflate PRIVATE wubuzip z)
add_test(NAME inflate COMMAND test_inflate)

add_executable(test_formula test_formula.c)
target_link_libraries(test_formula PRIVATE wubuformula)
target_include_directories(test_formula PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuformula/functions)
add_test(NAME formula COMMAND test_formula)

add_executable(test_spell test_spell.c)
target_link_libraries(test_spell PRIVATE wubuspell)
add_test(NAME spell COMMAND test_spell)

add_executable(test_base test_base.c)
target_link_libraries(test_base PRIVATE wububase)
add_test(NAME base COMMAND test_base)

add_executable(test_png test_png.c)
target_link_libraries(test_png PRIVATE wubupng wubuocr z)
add_test(NAME png COMMAND test_png)

add_executable(test_chart test_chart.c)
target_link_libraries(test_chart PRIVATE wubuchart)
add_test(NAME chart COMMAND test_chart)

add_executable(test_autosave test_autosave.c)
target_link_libraries(test_autosave PRIVATE wubuautosave)
add_test(NAME autosave COMMAND test_autosave)

add_executable(test_draw test_draw.c)
target_link_libraries(test_draw PRIVATE wubudraw)
add_test(NAME draw COMMAND test_draw)

add_executable(test_wubumath test_math.c)
target_link_libraries(test_wubumath PRIVATE wubumath)
add_test(NAME wubumath COMMAND test_wubumath)

add_executable(test_epub test_epub.c)
target_link_libraries(test_epub PRIVATE wubuepub)
add_test(NAME epub COMMAND test_epub)

add_executable(test_a11y test_a11y.c)
target_link_libraries(test_a11y PRIVATE wubua11y)
add_test(NAME a11y COMMAND test_a11y)

add_executable(test_script test_script.c)
target_link_libraries(test_script PRIVATE wubuscript)
add_test(NAME script COMMAND test_script)

add_executable(test_model test_model.c)
target_link_libraries(test_model PRIVATE wubumodel)
add_test(NAME model COMMAND test_model)

add_executable(test_doc_cache test_doc_cache.c)
target_link_libraries(test_doc_cache PRIVATE wubumodel)
add_test(NAME doc_cache COMMAND test_doc_cache)

add_executable(test_load_async test_load_async.c)
target_link_libraries(test_load_async PRIVATE wubumodel)
add_test(NAME load_async COMMAND test_load_async)

add_executable(test_deflate_roundtrip test_deflate_roundtrip.c)
target_link_libraries(test_deflate_roundtrip PRIVATE wubuzip)
add_test(NAME deflate_roundtrip COMMAND test_deflate_roundtrip)

add_executable(test_cold_start test_cold_start.c)
target_link_libraries(test_cold_start PRIVATE wubumodel)
add_test(NAME cold_start COMMAND test_cold_start)

add_executable(test_model_io_roundtrip test_model_io_roundtrip.c)
target_link_libraries(test_model_io_roundtrip PRIVATE wubumodel)
add_test(NAME model_io_roundtrip COMMAND test_model_io_roundtrip)

add_executable(test_wubuxml_parser test_wubuxml_parser.c)
target_link_libraries(test_wubuxml_parser PRIVATE wubuoxml)
target_include_directories(test_wubuxml_parser PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuxml)
add_test(NAME wubuxml_parser COMMAND test_wubuxml_parser)

add_executable(test_reader test_reader.c)
target_link_libraries(test_reader PRIVATE wubuoxml wubuword_app)
target_include_directories(test_reader PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/apps/wubuword)
add_test(NAME reader_roundtrip COMMAND test_reader)

add_executable(test_extract test_extract.c)
target_link_libraries(test_extract PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_extract PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow)
add_test(NAME extract COMMAND test_extract)

add_executable(test_edit test_edit.c)
target_link_libraries(test_edit PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app wubuedit_app)
target_include_directories(test_edit PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow ${CMAKE_SOURCE_DIR}/apps/wubuedit)
add_test(NAME edit_roundtrip COMMAND test_edit)

add_executable(test_conformance test_conformance.c)
target_link_libraries(test_conformance PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_conformance PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow)
target_compile_definitions(test_conformance PRIVATE CONFORMANCE_SCRIPT="${CMAKE_SOURCE_DIR}/tests/conformance/validate.py")
add_test(NAME conformance COMMAND test_conformance)

# Robustness gate: read files produced by INDEPENDENT tools (openpyxl, python-pptx).
add_executable(test_foreign test_foreign.c)
target_link_libraries(test_foreign PRIVATE wubuoxml wubuword_app wubucell_app wubushow_app)
target_include_directories(test_foreign PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow)
target_compile_definitions(test_foreign PRIVATE FOREIGN_SCRIPT="${CMAKE_SOURCE_DIR}/tests/conformance/produce_foreign.py")
add_test(NAME foreign_read COMMAND test_foreign)

add_executable(test_cell_read test_cell_read.c)
target_link_libraries(test_cell_read PRIVATE wubucell_app wubuoxml wubuxml wubuformula)
target_include_directories(test_cell_read PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuformula ${CMAKE_SOURCE_DIR}/src/wubuformula/functions)
add_test(NAME cell_read COMMAND test_cell_read)

add_executable(test_cell_cycle test_cell_cycle.c)
target_link_libraries(test_cell_cycle PRIVATE wubucell_app wubuformula)
target_include_directories(test_cell_cycle PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/src/wubuformula ${CMAKE_SOURCE_DIR}/src/wubuformula/functions)
add_test(NAME cell_cycle COMMAND test_cell_cycle)

add_executable(test_cell_merge test_cell_merge.c)
target_link_libraries(test_cell_merge PRIVATE wubucell_app wubuoxml wubuxml wubuformula)
target_include_directories(test_cell_merge PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuformula ${CMAKE_SOURCE_DIR}/src/wubuformula/functions)
add_test(NAME cell_merge COMMAND test_cell_merge)

add_executable(test_csv test_csv.c)
target_link_libraries(test_csv PRIVATE wubucell_app wubuoxml wubuxml wubuformula)
target_include_directories(test_csv PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuformula ${CMAKE_SOURCE_DIR}/src/wubuformula/functions)
add_test(NAME csv COMMAND test_csv)

add_executable(test_md test_md.c $<TARGET_OBJECTS:wubudoc_app>)
target_link_libraries(test_md PRIVATE wubuword_app wubuedit_app wubuoxml wubuxml wubucell_app wubushow_app wubuformula)
target_include_directories(test_md PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubudoc ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubuedit ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml)
add_test(NAME md COMMAND test_md)

add_executable(test_rtf_json test_rtf_json.c $<TARGET_OBJECTS:wubudoc_app>)
target_link_libraries(test_rtf_json PRIVATE wubuword_app wubuedit_app wubucell_app wubushow_app wubuoxml wubuxml wubuformula)
target_include_directories(test_rtf_json PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubudoc ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubuedit ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml)
add_test(NAME rtf_json COMMAND test_rtf_json)

add_executable(test_odf test_odf.c)
target_link_libraries(test_odf PRIVATE wubuword_app wubuedit_app wubucell_app wubushow_app wubuoxml wubuxml wubuzip wubuformula wubuconv)
target_include_directories(test_odf PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubuodf ${CMAKE_SOURCE_DIR}/apps/wubuedit ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuzip)
target_compile_definitions(test_odf PRIVATE ODF_ORACLE="${CMAKE_SOURCE_DIR}/tests/conformance/odf_oracle.py")
add_test(NAME odf COMMAND test_odf)

add_executable(test_convert test_convert.c)
target_link_libraries(test_convert PRIVATE wubuword_app wubuedit_app wubucell_app wubushow_app wubuoxml wubuxml wubuzip wubuformula wubucfb wubujson wubuconv)
target_include_directories(test_convert PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubuconv ${CMAKE_SOURCE_DIR}/apps/wubuodf ${CMAKE_SOURCE_DIR}/apps/wubudoc ${CMAKE_SOURCE_DIR}/apps/wubuedit ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow ${CMAKE_SOURCE_DIR}/apps/wubulegacy ${CMAKE_SOURCE_DIR}/apps/wubupdf ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuzip ${CMAKE_SOURCE_DIR}/src/wubucfb)
target_compile_definitions(test_convert PRIVATE
    WUBUOFFICE_BIN="${CMAKE_CURRENT_BINARY_DIR}/wubuoffice"
    ODF_ORACLE_SCRIPT="${CMAKE_SOURCE_DIR}/tests/conformance/odf_oracle.py")
add_test(NAME convert COMMAND test_convert)

# Word lists (bullet + numbered) feature: assemble + re-read validation.
add_executable(test_word_lists test_word_lists.c)
target_link_libraries(test_word_lists PRIVATE wubuword_app wubuoxml wubuxml wubuzip)
target_include_directories(test_word_lists PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuzip)
add_test(NAME word_lists COMMAND test_word_lists)

# Reverse-direction readers (rtf/html/epub -> dm_doc) round-trip.
add_executable(test_readers test_readers.c)
target_link_libraries(test_readers PRIVATE wubuoxml wubuxml wubuzip wubucfb wubuformula wubuedit_app wubujson wubuconv)
target_include_directories(test_readers PRIVATE
  ${CMAKE_SOURCE_DIR}/apps/wuburead ${CMAKE_SOURCE_DIR}/apps/wubuconv ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubuedit ${CMAKE_SOURCE_DIR}/apps/wubudoc
  ${CMAKE_SOURCE_DIR}/apps/wubuodf ${CMAKE_SOURCE_DIR}/apps/wubulegacy ${CMAKE_SOURCE_DIR}/apps/wubupdf ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuzip ${CMAKE_SOURCE_DIR}/src/wubucfb)
add_test(NAME readers COMMAND test_readers)

# Legacy binary readers (.doc/.xls/.ppt) + PDF writer, validated by oracles.
add_executable(test_legacy_pdf test_legacy_pdf.c
  $<TARGET_OBJECTS:wubulegacy_app> $<TARGET_OBJECTS:wubupdf_app>
  $<TARGET_OBJECTS:wubucell_app> $<TARGET_OBJECTS:wubushow_app>
  $<TARGET_OBJECTS:wubuword_app> $<TARGET_OBJECTS:wubuedit_app>)
target_link_libraries(test_legacy_pdf PRIVATE wubucfb wubuoxml wubuxml wubuzip wubuformula)
target_include_directories(test_legacy_pdf PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubulegacy ${CMAKE_SOURCE_DIR}/apps/wubupdf ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow ${CMAKE_SOURCE_DIR}/apps/wubuedit ${CMAKE_SOURCE_DIR}/src/wubucfb ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml)
target_compile_definitions(test_legacy_pdf PRIVATE LEGACY_ORACLE="${CMAKE_SOURCE_DIR}/tests/conformance/legacy_oracle.py")
add_test(NAME legacy_pdf COMMAND test_legacy_pdf)

# Flat ODF (.fodt/.fods/.fodp) round trips + EPUB writer.
add_executable(test_flat_epub test_flat_epub.c
  $<TARGET_OBJECTS:wubuodf_app> $<TARGET_OBJECTS:wubudoc_app>
  $<TARGET_OBJECTS:wubucell_app> $<TARGET_OBJECTS:wubushow_app>
  $<TARGET_OBJECTS:wubuword_app> $<TARGET_OBJECTS:wubuedit_app>)
target_link_libraries(test_flat_epub PRIVATE wubuoxml wubuxml wubuzip wubuformula)
target_include_directories(test_flat_epub PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubuodf ${CMAKE_SOURCE_DIR}/apps/wubudoc ${CMAKE_SOURCE_DIR}/apps/wubucell ${CMAKE_SOURCE_DIR}/apps/wubushow ${CMAKE_SOURCE_DIR}/apps/wubuword ${CMAKE_SOURCE_DIR}/apps/wubuedit ${CMAKE_SOURCE_DIR}/src/wubuoxml ${CMAKE_SOURCE_DIR}/src/wubuxml ${CMAKE_SOURCE_DIR}/src/wubuzip)
target_compile_definitions(test_flat_epub PRIVATE FLAT_EPUB_ORACLE="${CMAKE_SOURCE_DIR}/tests/conformance/flat_epub_oracle.py")
add_test(NAME flat_epub COMMAND test_flat_epub)

# wubufont: SFNT/TrueType parser + SVG font emitter (validates a real system font).
add_executable(test_wubufont test_wubufont.c)
target_link_libraries(test_wubufont PRIVATE wubufont)
add_test(NAME wubufont COMMAND test_wubufont)

# wubuocr: image -> document pipeline (Otsu, XY-cut reading order, components).
add_executable(test_wubuocr test_wubuocr.c)
target_link_libraries(test_wubuocr PRIVATE wubuocr)
add_test(NAME wubuocr COMMAND test_wubuocr)

# CRNN line transcribe -> docmodel JSON (document path regression test)
add_executable(test_crnn_transcribe test_crnn_transcribe.c)
target_include_directories(test_crnn_transcribe PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(test_crnn_transcribe PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME crnn_transcribe COMMAND test_crnn_transcribe)
set_tests_properties(crnn_transcribe PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# CRNN line transcribe -> docmodel JSON (full document charset regression test)
add_executable(test_crnn_transcribe_full test_crnn_transcribe_full.c)
target_include_directories(test_crnn_transcribe_full PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(test_crnn_transcribe_full PRIVATE crnnocr wubuimage wubufont m)
add_test(NAME crnn_transcribe_full COMMAND test_crnn_transcribe_full)
set_tests_properties(crnn_transcribe_full PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Multi-column detection (1/2/3-col) end-to-end via crnn_transcribe_page_json
add_executable(test_columns test_columns.c)
target_include_directories(test_columns PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(test_columns PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME columns COMMAND test_columns)

# Binarize unit tests: Sauvola / Otsu / despeckle / auto-crop / empty-page
add_executable(test_imageops test_imageops.c)
target_include_directories(test_imageops PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_imageops PRIVATE wubuocr m)
add_test(NAME imageops COMMAND test_imageops)

# xlsx (Excel OOXML) export: docmodel JSON -> valid .xlsx zip with 6 OOXML parts
add_executable(test_xlsx test_xlsx.c)
target_include_directories(test_xlsx PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubuzip)
target_link_libraries(test_xlsx PRIVATE wubuocr wubuzip m)
add_test(NAME xlsx COMMAND test_xlsx)

# docfmt2 unit test: TEI serialization + reading-order/confidence fields
add_executable(test_docfmt2 test_docfmt2.c)
target_include_directories(test_docfmt2 PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_docfmt2 PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME docfmt2 COMMAND test_docfmt2)

# pdfua unit test: PDF/UA structure tree + language tag
add_executable(test_pdfua test_pdfua.c)
target_include_directories(test_pdfua PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_pdfua PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME pdfua COMMAND test_pdfua)

# nfc unit test: Unicode NFC Latin composition
add_executable(test_nfc test_nfc.c)
target_include_directories(test_nfc PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_nfc PRIVATE crnnocr wubuocr wubufont m)
add_test(NAME nfc COMMAND test_nfc)

# imgops: image preprocessing (rotate / contrast / median / shading / sharpen)
add_executable(test_imgops test_imgops.c)
target_include_directories(test_imgops PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuimage ${CMAKE_SOURCE_DIR}/src/wubuocr)
target_link_libraries(test_imgops PRIVATE wubuimage wubuocr m)
add_test(NAME imgops COMMAND test_imgops)

# docfmt: alternate document serializations (txt/tsv/csv/jsonl/latex/rtf/hocr/alto)
add_executable(test_docfmt test_docfmt.c)
target_include_directories(test_docfmt PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubujson)
target_link_libraries(test_docfmt PRIVATE wubuocr wubujson m)
add_test(NAME docfmt COMMAND test_docfmt)

add_executable(test_ocr_realpage test_ocr_realpage.c)
target_include_directories(test_ocr_realpage PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/wubufont)
target_link_libraries(test_ocr_realpage PRIVATE crnnocr wubuimage wubufont m)
add_test(NAME ocr_realpage COMMAND test_ocr_realpage)

# wubuocr recognizer: zoning + 1-NN glyph classification (render->recognize).
add_executable(test_wubuocr_recognize test_wubuocr_recognize.c)
target_link_libraries(test_wubuocr_recognize PRIVATE wubuocr)
add_test(NAME wubuocr_recognize COMMAND test_wubuocr_recognize)

# wubuocr fontbank: multi-font bank built from REAL font rasterizations.
add_executable(test_fontbank test_fontbank.c)
target_link_libraries(test_fontbank PRIVATE wubuocr wubufont)
add_test(NAME fontbank COMMAND test_fontbank)

# wubugauntlet: OCR robustness battery for the multi-font bank.
add_executable(test_gauntlet test_gauntlet.c)
target_link_libraries(test_gauntlet PRIVATE wubuocr wubufont)
add_test(NAME gauntlet COMMAND test_gauntlet)

# wubuocr Latin-1 tier: English-first, Latin second (accented glyph recall).
add_executable(test_latin1 test_latin1.c)
target_link_libraries(test_latin1 PRIVATE wubuocr wubufont)
add_test(NAME latin1 COMMAND test_latin1)

# wubunote: tabbed terminal text editor (Notepad++-class UI).
add_executable(test_wubunote_ctrl test_wubunote_ctrl.c
                          ${CMAKE_SOURCE_DIR}/apps/wubunote/note_controller.c)
target_include_directories(test_wubunote_ctrl PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubunote ${CMAKE_SOURCE_DIR}/src/wubunote ${CMAKE_SOURCE_DIR}/src/wubutui)
target_link_libraries(test_wubunote_ctrl PRIVATE wubunote_core wubutui)
add_test(NAME wubunote_ctrl COMMAND test_wubunote_ctrl)

add_executable(test_wubunote test_wubuedit.c)
target_link_libraries(test_wubunote PRIVATE wubunote_core)
add_test(NAME wubunote COMMAND test_wubunote)

# wubuview: docflat model->text flattening (pure).
add_executable(test_wubuview test_wubuview.c ${CMAKE_SOURCE_DIR}/apps/wubuview/docflat.c)
target_include_directories(test_wubuview PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubuview ${CMAKE_SOURCE_DIR}/src/wubujson)
target_link_libraries(test_wubuview PRIVATE wubujson)
add_test(NAME wubuview COMMAND test_wubuview)

add_executable(test_wubuview_ctrl test_wubuview_ctrl.c ${CMAKE_SOURCE_DIR}/apps/wubuview/controller.c)
target_include_directories(test_wubuview_ctrl PRIVATE ${CMAKE_SOURCE_DIR}/apps/wubuview ${CMAKE_SOURCE_DIR}/src/wubutui)
target_link_libraries(test_wubuview_ctrl PRIVATE wubutui)
add_test(NAME wubuview_ctrl COMMAND test_wubuview_ctrl)

# wubusvg: SVG ingest + regurgitate round-trip.
add_executable(test_wubusvg test_wubusvg.c)
target_link_libraries(test_wubusvg PRIVATE wubusvg)
add_test(NAME wubusvg COMMAND test_wubusvg)

# wubudoc agent test
add_executable(test_wubudoc test_wubudoc.c)
target_link_libraries(test_wubudoc PRIVATE wubudoc)
add_test(NAME wubudoc COMMAND test_wubudoc)

# drag/drop + bracketed-paste ingestion (magic-byte dispatch, paste decode)
add_executable(test_drop test_drop.c)
target_link_libraries(test_drop PRIVATE wubudoc wubutui wubuimage)
add_test(NAME drop COMMAND test_drop)

# wubusvg agent: NDJSON protocol for wubuOS (mirrors WuBuPad agent).
add_executable(test_wubusvg_agent test_wubusvg_agent.c)
target_link_libraries(test_wubusvg_agent PRIVATE wubusvg)
add_test(NAME wubusvg_agent COMMAND test_wubusvg_agent)

# --- GPU / CUDA parity tests (defined even without CUDA; gpublas is a stub) ---
add_executable(test_gpu_blas test_gpu_blas.c)
target_link_libraries(test_gpu_blas PRIVATE gpublas m)
add_test(NAME gpu_blas COMMAND test_gpu_blas)

add_executable(test_gru_gpu test_gru_gpu.c ${CMAKE_SOURCE_DIR}/src/gpu/gru_gpu.c ${CMAKE_SOURCE_DIR}/src/wubuocr/gru_layout.c)
target_include_directories(test_gru_gpu PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/gpu)
target_compile_definitions(test_gru_gpu PRIVATE WITH_CUDA_BUILD)
target_link_libraries(test_gru_gpu PRIVATE gpublas m)
add_test(NAME gru_gpu_gradcheck COMMAND test_gru_gpu)

add_executable(test_gemm_nt test_gemm_nt.c)
target_link_libraries(test_gemm_nt PRIVATE gpublas m)
add_test(NAME gemm_nt COMMAND test_gemm_nt)

add_executable(test_gru_cmp test_gru_cmp.c ${CMAKE_SOURCE_DIR}/src/gpu/gru_gpu.c ${CMAKE_SOURCE_DIR}/src/wubuocr/gru_layout.c ${CMAKE_SOURCE_DIR}/src/wubuocr/ocl_gru.c)
target_include_directories(test_gru_cmp PRIVATE ${CMAKE_SOURCE_DIR}/src/wubuocr ${CMAKE_SOURCE_DIR}/src/gpu)
target_compile_definitions(test_gru_cmp PRIVATE WITH_CUDA_BUILD)
target_link_libraries(test_gru_cmp PRIVATE gpublas m)
add_test(NAME gru_cmp COMMAND test_gru_cmp)

# ---------------------------------------------------------------------------
# MODULAR TEST LABELS
# Each test is tagged with its owning module so smoke runs can target a single
# module instead of re-running the whole suite (incl. the slow OCR/CNN battery).
#   ctest -L spell     # only the spell module's tests
#   ctest -LE ocr      # everything EXCEPT the slow OCR/CNN suite
#   ./tools/smoke.sh spell   # convenience wrapper
# 'ocr' marks the long CNN/recognition smoke tests that must NOT run per change.
# ---------------------------------------------------------------------------
set_property(TEST wubuzip_crc        PROPERTY LABELS zip)
set_property(TEST inflate            PROPERTY LABELS zip)
set_property(TEST deflate_roundtrip  PROPERTY LABELS zip)
set_property(TEST formula            PROPERTY LABELS formula)
set_property(TEST spell              PROPERTY LABELS spell)
set_property(TEST base               PROPERTY LABELS base)
set_property(TEST png                PROPERTY LABELS png)
set_property(TEST chart              PROPERTY LABELS chart)
set_property(TEST autosave           PROPERTY LABELS autosave)
set_property(TEST draw               PROPERTY LABELS draw)
set_property(TEST wubumath           PROPERTY LABELS math)
set_property(TEST epub               PROPERTY LABELS epub)
set_property(TEST a11y               PROPERTY LABELS a11y)
set_property(TEST script             PROPERTY LABELS script)
set_property(TEST model              PROPERTY LABELS model)
set_property(TEST doc_cache          PROPERTY LABELS model)
set_property(TEST load_async         PROPERTY LABELS model)
set_property(TEST cold_start         PROPERTY LABELS model)
set_property(TEST model_io_roundtrip PROPERTY LABELS model)
set_property(TEST wubuxml_parser      PROPERTY LABELS oxml)
set_property(TEST reader_roundtrip    PROPERTY LABELS oxml)
set_property(TEST extract            PROPERTY LABELS oxml)
set_property(TEST edit_roundtrip      PROPERTY LABELS oxml)
set_property(TEST conformance        PROPERTY LABELS oxml)
set_property(TEST foreign_read       PROPERTY LABELS oxml)
set_property(TEST cell_read          PROPERTY LABELS cell)
set_property(TEST cell_cycle         PROPERTY LABELS cell)
set_property(TEST cell_merge         PROPERTY LABELS cell)
set_property(TEST csv                PROPERTY LABELS cell)
set_property(TEST md                 PROPERTY LABELS word)
set_property(TEST rtf_json           PROPERTY LABELS word)
set_property(TEST word_lists         PROPERTY LABELS word)
set_property(TEST readers            PROPERTY LABELS word)
set_property(TEST legacy_pdf         PROPERTY LABELS word)
set_property(TEST flat_epub          PROPERTY LABELS word)
set_property(TEST odf                PROPERTY LABELS odf)
set_property(TEST convert            PROPERTY LABELS odf)
set_property(TEST wubufont           PROPERTY LABELS font)
set_property(TEST wubuocr            PROPERTY LABELS ocr)
set_property(TEST crnn_transcribe    PROPERTY LABELS ocr)
set_property(TEST crnn_transcribe_full PROPERTY LABELS ocr)
set_property(TEST columns            PROPERTY LABELS ocr)
set_property(TEST imageops           PROPERTY LABELS ocr)
set_property(TEST xlsx               PROPERTY LABELS ocr)
set_property(TEST docfmt2            PROPERTY LABELS ocr)
set_property(TEST pdfua              PROPERTY LABELS ocr)
set_property(TEST nfc                PROPERTY LABELS ocr)
set_property(TEST imgops             PROPERTY LABELS ocr)
set_property(TEST docfmt             PROPERTY LABELS ocr)
set_property(TEST ocr_realpage       PROPERTY LABELS ocr)
set_property(TEST wubuocr_recognize  PROPERTY LABELS ocr)
set_property(TEST fontbank           PROPERTY LABELS ocr)
set_property(TEST gauntlet           PROPERTY LABELS ocr)
set_property(TEST latin1             PROPERTY LABELS ocr)
set_property(TEST wubunote_ctrl      PROPERTY LABELS note)
set_property(TEST wubunote           PROPERTY LABELS note)
set_property(TEST wubuview           PROPERTY LABELS view)
set_property(TEST wubuview_ctrl      PROPERTY LABELS view)
set_property(TEST wubusvg            PROPERTY LABELS svg)
set_property(TEST wubusvg_agent      PROPERTY LABELS svg)
set_property(TEST wubudoc            PROPERTY LABELS doc)
set_property(TEST drop               PROPERTY LABELS doc)
set_property(TEST gpu_blas           PROPERTY LABELS gpu)
set_property(TEST gru_gpu_gradcheck  PROPERTY LABELS gpu)
set_property(TEST gemm_nt            PROPERTY LABELS gpu)
set_property(TEST gru_cmp            PROPERTY LABELS gpu)

# wubulayout (central pipeline) + wubuexp (EXP cluster)
add_executable(test_layout test_layout.c)
target_link_libraries(test_layout PRIVATE wubulayout)
add_test(NAME layout COMMAND test_layout)
set_property(TEST layout PROPERTY LABELS layout)

add_executable(test_exp test_exp.c)
target_link_libraries(test_exp PRIVATE wubuexp wubulayout)
add_test(NAME exp COMMAND test_exp)
set_property(TEST exp PROPERTY LABELS exp)

# wubusettings (persistent config) + wubushape (INT-7: RTL reorder)
add_executable(test_settings test_settings.c)
target_link_libraries(test_settings PRIVATE wubusettings)
add_test(NAME settings COMMAND test_settings)
set_property(TEST settings PROPERTY LABELS settings)

add_executable(test_shape test_shape.c)
target_link_libraries(test_shape PRIVATE wubushape)
add_test(NAME shape COMMAND test_shape)
set_property(TEST shape PROPERTY LABELS shape)
